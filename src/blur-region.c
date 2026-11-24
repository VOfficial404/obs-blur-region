/*
obs-blur-region
Copyright (C) 2026 VOfficial404

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

/* "Blur Region" source: placed above a game capture, it draws a blurred, pixelated or solid copy of
 * whatever part of the capture it covers. The user moves and resizes it with OBS's own transform
 * handles; every frame the source maps its own pixels onto the target's through both items' draw
 * transforms, so moving, scaling, rotating or cropping either one keeps the region aligned. */

#include "region-map.h"

#include <obs-module.h>
#include <plugin-support.h>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <util/threading.h>

#include <stdint.h>

#define T(text) obs_module_text(text)

#define SOURCE_ID "blur_region_source"
/* Seconds between lookups of the scene holding the region and the source under it. */
#define REFRESH_INTERVAL 0.5f
/* Blur radius (in texels) above which the image is halved before blurring; keeps the 25-tap kernel dense. */
#define MAX_KERNEL_RADIUS 16.0f
#define MAX_DOWNSAMPLE 6

enum blur_mode {
	MODE_BLUR = 0,
	MODE_MOSAIC = 1,
	MODE_SOLID = 2,
};

struct blur_region {
	obs_source_t *source;

	gs_effect_t *effect;
	gs_eparam_t *p_image;
	gs_eparam_t *p_texel_step;
	gs_eparam_t *p_local_size;
	gs_eparam_t *p_map_x;
	gs_eparam_t *p_map_y;
	gs_eparam_t *p_tex_size;
	gs_eparam_t *p_mosaic;
	gs_eparam_t *p_tint;
	gs_eparam_t *p_radius;
	gs_eparam_t *p_feather;
	gs_eparam_t *p_opacity;

	gs_texrender_t *base;
	gs_texrender_t *down[MAX_DOWNSAMPLE];
	gs_texrender_t *blur_h;
	gs_texrender_t *blur_v;

	uint32_t width;
	uint32_t height;
	enum blur_mode mode;
	float blur;
	float mosaic;
	float radius;
	float feather;
	float opacity;
	float tint;
	struct vec3 color;

	/* Written by update() on the UI thread, read by the lookup on the graphics thread. */
	pthread_mutex_t target_mutex;
	struct dstr target_name; /* empty: the nearest source below the region */

	/* Graphics thread only (video_tick and video_render). */
	float refresh_t;
	obs_weak_source_t *container; /* scene or group holding both items */
	struct dstr located_target;
};

struct source_list {
	DARRAY(obs_source_t *) sources;
};

struct item_list {
	DARRAY(obs_sceneitem_t *) items;
};

static bool name_equals(const char *a, const char *b)
{
	return a && b && strcmp(a, b) == 0;
}

/* ---- Locating the region's scene and target ---------------------------------------------- */

static bool collect_scene(void *param, obs_source_t *scene)
{
	struct source_list *list = param;
	obs_source_t *ref = obs_source_get_ref(scene);
	if (ref)
		da_push_back(list->sources, &ref);
	return true;
}

static bool collect_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	struct item_list *list = param;
	obs_sceneitem_addref(item);
	da_push_back(list->items, &item);
	return true;
}

static void item_list_free(struct item_list *list)
{
	for (size_t i = 0; i < list->items.num; i++)
		obs_sceneitem_release(list->items.array[i]);
	da_free(list->items);
}

/* A scene that (even through nesting) contains the region would render itself forever. */
static bool contains_region(obs_source_t *src, const char *self_name)
{
	obs_scene_t *scene = obs_group_or_scene_from_source(src);
	return scene && obs_scene_find_source_recursive(scene, self_name) != NULL;
}

static bool usable_target(obs_source_t *src, const char *self_name)
{
	return src && (obs_source_get_output_flags(src) & OBS_SOURCE_VIDEO) != 0 &&
	       !name_equals(obs_source_get_id(src), SOURCE_ID) && !contains_region(src, self_name);
}

/* items run bottom to top, as obs_scene_enum_items returns them. */
static const char *pick_target(const struct item_list *list, size_t self_idx, const char *self_name,
			       const char *target_name)
{
	if (*target_name) {
		for (size_t i = 0; i < list->items.num; i++) {
			obs_source_t *src = obs_sceneitem_get_source(list->items.array[i]);
			if (name_equals(obs_source_get_name(src), target_name))
				return usable_target(src, self_name) ? target_name : NULL;
		}
		return NULL;
	}

	for (size_t i = self_idx; i-- > 0;) {
		obs_sceneitem_t *item = list->items.array[i];
		obs_source_t *src = obs_sceneitem_get_source(item);
		if (obs_sceneitem_visible(item) && usable_target(src, self_name))
			return obs_source_get_name(src);
	}
	return NULL;
}

static bool search_scene(struct blur_region *br, obs_scene_t *scene, const char *self_name, const char *target_name)
{
	struct item_list list = {0};
	obs_scene_enum_items(scene, collect_item, &list);

	size_t self_idx = SIZE_MAX;
	for (size_t i = 0; i < list.items.num; i++) {
		if (obs_sceneitem_get_source(list.items.array[i]) == br->source) {
			self_idx = i;
			break;
		}
	}

	bool found = false;
	if (self_idx != SIZE_MAX) {
		const char *target = pick_target(&list, self_idx, self_name, target_name);
		if (target) {
			br->container = obs_source_get_weak_source(obs_scene_get_source(scene));
			dstr_copy(&br->located_target, target);
			found = true;
		}
	} else {
		for (size_t i = 0; i < list.items.num && !found; i++) {
			obs_sceneitem_t *item = list.items.array[i];
			if (obs_sceneitem_is_group(item))
				found = search_scene(br, obs_sceneitem_group_get_scene(item), self_name, target_name);
		}
	}

	item_list_free(&list);
	return found;
}

static void clear_location(struct blur_region *br)
{
	obs_weak_source_release(br->container);
	br->container = NULL;
	dstr_free(&br->located_target);
}

/* Prefer the scene on air, then the one in the preview, then any: a region pasted as a reference
 * into several scenes follows the one being shown. */
static int scene_rank(obs_source_t *scene)
{
	if (obs_source_active(scene))
		return 0;
	return obs_source_showing(scene) ? 1 : 2;
}

static void locate(struct blur_region *br)
{
	clear_location(br);

	const char *self_name = obs_source_get_name(br->source);
	if (!self_name || !*self_name)
		return;

	struct dstr target_name = {0};
	pthread_mutex_lock(&br->target_mutex);
	dstr_copy_dstr(&target_name, &br->target_name);
	pthread_mutex_unlock(&br->target_mutex);

	/* Take references first: searching inside obs_enum_scenes would hold the global source lock
	 * while taking scene locks. */
	struct source_list scenes = {0};
	obs_enum_scenes(collect_scene, &scenes);

	for (int rank = 0; rank < 3 && !br->container; rank++) {
		for (size_t i = 0; i < scenes.sources.num && !br->container; i++) {
			obs_source_t *src = scenes.sources.array[i];
			obs_scene_t *scene = obs_group_or_scene_from_source(src);
			if (scene && scene_rank(src) == rank)
				search_scene(br, scene, self_name, target_name.array ? target_name.array : "");
		}
	}

	for (size_t i = 0; i < scenes.sources.num; i++)
		obs_source_release(scenes.sources.array[i]);
	da_free(scenes.sources);
	dstr_free(&target_name);
}

/* ---- Rendering ------------------------------------------------------------------------------ */

static void get_placement(obs_sceneitem_t *item, struct region_placement *p)
{
	struct matrix4 m;
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_draw_transform(item, &m);
	obs_sceneitem_get_crop(item, &crop);

	p->a = m.x.x;
	p->b = m.x.y;
	p->c = m.y.x;
	p->d = m.y.y;
	p->tx = m.t.x;
	p->ty = m.t.y;
	p->crop_x = (float)crop.left;
	p->crop_y = (float)crop.top;
}

static bool begin_pass(gs_texrender_t *tr, uint32_t cx, uint32_t cy)
{
	gs_texrender_reset(tr);
	if (!gs_texrender_begin(tr, cx, cy))
		return false;

	struct vec4 clear;
	vec4_zero(&clear);
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
	return true;
}

static gs_texture_t *effect_pass(struct blur_region *br, gs_texrender_t *tr, const char *technique, gs_texture_t *src,
				 uint32_t cx, uint32_t cy, float step_x, float step_y)
{
	if (!begin_pass(tr, cx, cy))
		return NULL;

	struct vec2 step;
	vec2_set(&step, step_x, step_y);
	gs_effect_set_texture(br->p_image, src);
	gs_effect_set_vec2(br->p_texel_step, &step);
	while (gs_effect_loop(br->effect, technique))
		gs_draw_sprite(src, 0, cx, cy);

	gs_texrender_end(tr);
	return gs_texrender_get_texture(tr);
}

/* Renders the target at full size, then for blur halves it (each step a 2x2 average) until the
 * remaining radius fits the kernel, and blurs horizontally and vertically. */
static gs_texture_t *render_target_texture(struct blur_region *br, obs_source_t *target, uint32_t cx, uint32_t cy)
{
	if (!begin_pass(br->base, cx, cy))
		return NULL;
	obs_source_video_render(target);
	gs_texrender_end(br->base);

	gs_texture_t *tex = gs_texrender_get_texture(br->base);
	if (br->mode != MODE_BLUR)
		return tex;

	float radius = br->blur;
	for (int level = 0; tex && radius > MAX_KERNEL_RADIUS && level < MAX_DOWNSAMPLE; level++) {
		cx = (cx + 1) / 2;
		cy = (cy + 1) / 2;
		tex = effect_pass(br, br->down[level], "Copy", tex, cx, cy, 0.0f, 0.0f);
		radius *= 0.5f;
	}
	if (!tex)
		return NULL;

	/* The kernel spans 12 taps each side; spread them over the radius. */
	const float spacing = radius / 12.0f;
	tex = effect_pass(br, br->blur_h, "Blur", tex, cx, cy, spacing / (float)cx, 0.0f);
	if (!tex)
		return NULL;
	return effect_pass(br, br->blur_v, "Blur", tex, cx, cy, 0.0f, spacing / (float)cy);
}

static void set_shape_params(struct blur_region *br)
{
	struct vec2 size;
	vec2_set(&size, (float)br->width, (float)br->height);
	gs_effect_set_vec2(br->p_local_size, &size);
	gs_effect_set_float(br->p_radius, br->radius);
	gs_effect_set_float(br->p_feather, br->feather);
	gs_effect_set_float(br->p_opacity, br->opacity);
}

static void render_solid(struct blur_region *br)
{
	struct vec4 tint;
	vec4_set(&tint, br->color.x, br->color.y, br->color.z, 1.0f);
	set_shape_params(br);
	gs_effect_set_vec4(br->p_tint, &tint);
	while (gs_effect_loop(br->effect, "Solid"))
		gs_draw_sprite(NULL, 0, br->width, br->height);
}

static void render_from_target(struct blur_region *br, obs_scene_t *scene)
{
	obs_sceneitem_t *self_item = obs_scene_find_source(scene, obs_source_get_name(br->source));
	obs_sceneitem_t *target_item = obs_scene_find_source(scene, br->located_target.array);
	if (!self_item || !target_item || !obs_sceneitem_visible(target_item))
		return;

	obs_source_t *target = obs_sceneitem_get_source(target_item);
	const uint32_t cx = obs_source_get_width(target);
	const uint32_t cy = obs_source_get_height(target);
	if (!cx || !cy)
		return;

	struct region_placement self_place, target_place;
	get_placement(self_item, &self_place);
	get_placement(target_item, &target_place);
	float map_x[3], map_y[3];
	if (!region_map_compute(&self_place, &target_place, map_x, map_y))
		return;

	/* The scene has pushed the region's own transform; the offscreen passes need none. */
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	gs_texture_t *tex = render_target_texture(br, target, cx, cy);
	gs_blend_state_pop();
	gs_matrix_pop();
	if (!tex)
		return;

	struct vec4 mx, my, tint;
	struct vec2 tex_size;
	vec4_set(&mx, map_x[0], map_x[1], map_x[2], 0.0f);
	vec4_set(&my, map_y[0], map_y[1], map_y[2], 0.0f);
	vec4_set(&tint, br->color.x, br->color.y, br->color.z, br->tint);
	vec2_set(&tex_size, (float)cx, (float)cy);

	set_shape_params(br);
	gs_effect_set_vec4(br->p_map_x, &mx);
	gs_effect_set_vec4(br->p_map_y, &my);
	gs_effect_set_vec2(br->p_tex_size, &tex_size);
	gs_effect_set_float(br->p_mosaic, br->mode == MODE_MOSAIC ? br->mosaic : 0.0f);
	gs_effect_set_vec4(br->p_tint, &tint);
	gs_effect_set_texture(br->p_image, tex);
	while (gs_effect_loop(br->effect, "Final"))
		gs_draw_sprite(tex, 0, br->width, br->height);
}

static void blur_region_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct blur_region *br = data;
	if (!br->effect)
		return;

	if (br->mode == MODE_SOLID) {
		render_solid(br);
		return;
	}
	if (!br->container || !br->located_target.len)
		return;

	obs_source_t *container = obs_weak_source_get_source(br->container);
	if (!container)
		return;
	obs_scene_t *scene = obs_group_or_scene_from_source(container);
	if (scene)
		render_from_target(br, scene);
	obs_source_release(container);
}

static void blur_region_tick(void *data, float seconds)
{
	struct blur_region *br = data;
	br->refresh_t += seconds;
	if (br->refresh_t < REFRESH_INTERVAL)
		return;

	br->refresh_t = 0.0f;
	if (br->mode == MODE_SOLID)
		clear_location(br);
	else
		locate(br);
}

/* ---- Source definition -------------------------------------------------------------------- */

static const char *blur_region_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T("BlurRegion");
}

static void blur_region_update(void *data, obs_data_t *settings)
{
	struct blur_region *br = data;

	const long long width = obs_data_get_int(settings, "width");
	const long long height = obs_data_get_int(settings, "height");
	br->width = (uint32_t)(width > 0 ? width : 1);
	br->height = (uint32_t)(height > 0 ? height : 1);
	br->mode = (enum blur_mode)obs_data_get_int(settings, "mode");
	br->blur = (float)obs_data_get_int(settings, "blur");
	br->mosaic = (float)obs_data_get_int(settings, "mosaic");
	br->radius = (float)obs_data_get_int(settings, "radius");
	br->feather = (float)obs_data_get_int(settings, "feather");
	br->opacity = (float)obs_data_get_int(settings, "opacity") / 100.0f;
	br->tint = (float)obs_data_get_int(settings, "tint") / 100.0f;
	if (br->blur < 1.0f)
		br->blur = 1.0f;
	if (br->mosaic < 2.0f)
		br->mosaic = 2.0f;

	const uint32_t color = (uint32_t)obs_data_get_int(settings, "color"); /* 0xAABBGGRR */
	vec3_set(&br->color, (float)(color & 0xff) / 255.0f, (float)((color >> 8) & 0xff) / 255.0f,
		 (float)((color >> 16) & 0xff) / 255.0f);

	pthread_mutex_lock(&br->target_mutex);
	dstr_copy(&br->target_name, obs_data_get_string(settings, "target"));
	pthread_mutex_unlock(&br->target_mutex);

	/* Look the target up again on the next frame. */
	br->refresh_t = REFRESH_INTERVAL;
}

static void *blur_region_create(obs_data_t *settings, obs_source_t *source)
{
	struct blur_region *br = bzalloc(sizeof(*br));
	br->source = source;
	pthread_mutex_init(&br->target_mutex, NULL);

	char *path = obs_module_file("effects/blur-region.effect");
	obs_enter_graphics();
	br->effect = gs_effect_create_from_file(path, NULL);
	br->base = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	for (size_t i = 0; i < MAX_DOWNSAMPLE; i++)
		br->down[i] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	br->blur_h = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	br->blur_v = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();

	if (br->effect) {
		br->p_image = gs_effect_get_param_by_name(br->effect, "image");
		br->p_texel_step = gs_effect_get_param_by_name(br->effect, "texel_step");
		br->p_local_size = gs_effect_get_param_by_name(br->effect, "local_size");
		br->p_map_x = gs_effect_get_param_by_name(br->effect, "map_x");
		br->p_map_y = gs_effect_get_param_by_name(br->effect, "map_y");
		br->p_tex_size = gs_effect_get_param_by_name(br->effect, "tex_size");
		br->p_mosaic = gs_effect_get_param_by_name(br->effect, "mosaic");
		br->p_tint = gs_effect_get_param_by_name(br->effect, "tint");
		br->p_radius = gs_effect_get_param_by_name(br->effect, "radius");
		br->p_feather = gs_effect_get_param_by_name(br->effect, "feather");
		br->p_opacity = gs_effect_get_param_by_name(br->effect, "opacity");
	} else {
		obs_log(LOG_ERROR, "failed to load effect %s", path ? path : "effects/blur-region.effect");
	}
	bfree(path);

	blur_region_update(br, settings);
	return br;
}

static void blur_region_destroy(void *data)
{
	struct blur_region *br = data;

	obs_enter_graphics();
	gs_effect_destroy(br->effect);
	gs_texrender_destroy(br->base);
	for (size_t i = 0; i < MAX_DOWNSAMPLE; i++)
		gs_texrender_destroy(br->down[i]);
	gs_texrender_destroy(br->blur_h);
	gs_texrender_destroy(br->blur_v);
	obs_leave_graphics();

	clear_location(br);
	dstr_free(&br->target_name);
	pthread_mutex_destroy(&br->target_mutex);
	bfree(br);
}

static uint32_t blur_region_width(void *data)
{
	const struct blur_region *br = data;
	return br->width;
}

static uint32_t blur_region_height(void *data)
{
	const struct blur_region *br = data;
	return br->height;
}

static void blur_region_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 200);
	obs_data_set_default_int(settings, "mode", MODE_BLUR);
	obs_data_set_default_string(settings, "target", "");
	obs_data_set_default_int(settings, "blur", 40);
	obs_data_set_default_int(settings, "mosaic", 20);
	obs_data_set_default_int(settings, "radius", 12);
	obs_data_set_default_int(settings, "feather", 0);
	obs_data_set_default_int(settings, "opacity", 100);
	obs_data_set_default_int(settings, "color", 0xFF000000);
	obs_data_set_default_int(settings, "tint", 0);
}

static bool mode_modified(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	const long long mode = obs_data_get_int(settings, "mode");
	obs_property_set_visible(obs_properties_get(props, "target"), mode != MODE_SOLID);
	obs_property_set_visible(obs_properties_get(props, "blur"), mode == MODE_BLUR);
	obs_property_set_visible(obs_properties_get(props, "mosaic"), mode == MODE_MOSAIC);
	obs_property_set_visible(obs_properties_get(props, "tint"), mode != MODE_SOLID);
	return true;
}

static bool add_target_source(void *param, obs_source_t *src)
{
	obs_property_t *list = param;
	if ((obs_source_get_output_flags(src) & OBS_SOURCE_VIDEO) && !name_equals(obs_source_get_id(src), SOURCE_ID)) {
		const char *name = obs_source_get_name(src);
		obs_property_list_add_string(list, name, name);
	}
	return true;
}

static bool add_target_scene(void *param, obs_source_t *src)
{
	obs_property_t *list = param;
	if (obs_source_is_group(src))
		return true;

	const char *name = obs_source_get_name(src);
	struct dstr label = {0};
	dstr_printf(&label, T("Target.Scene"), name);
	obs_property_list_add_string(list, label.array, name);
	dstr_free(&label);
	return true;
}

static obs_properties_t *blur_region_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p =
		obs_properties_add_list(props, "mode", T("Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T("Mode.Blur"), MODE_BLUR);
	obs_property_list_add_int(p, T("Mode.Mosaic"), MODE_MOSAIC);
	obs_property_list_add_int(p, T("Mode.Solid"), MODE_SOLID);
	obs_property_set_modified_callback(p, mode_modified);

	p = obs_properties_add_list(props, "target", T("Target"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T("Target.Auto"), "");
	obs_enum_sources(add_target_source, p);
	obs_enum_scenes(add_target_scene, p);
	obs_property_set_long_description(p, T("Target.Description"));

	obs_properties_add_int_slider(props, "blur", T("Strength"), 1, 300, 1);
	obs_properties_add_int_slider(props, "mosaic", T("MosaicSize"), 2, 150, 1);
	obs_properties_add_color(props, "color", T("Color"));
	obs_properties_add_int_slider(props, "tint", T("Tint"), 0, 100, 1);
	obs_properties_add_int_slider(props, "opacity", T("Opacity"), 0, 100, 1);
	obs_properties_add_int_slider(props, "radius", T("Radius"), 0, 500, 1);
	obs_properties_add_int_slider(props, "feather", T("Feather"), 0, 200, 1);
	obs_properties_add_int(props, "width", T("Width"), 1, 8192, 1);
	obs_properties_add_int(props, "height", T("Height"), 1, 8192, 1);
	return props;
}

struct obs_source_info blur_region_source = {
	.id = SOURCE_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = blur_region_get_name,
	.create = blur_region_create,
	.destroy = blur_region_destroy,
	.update = blur_region_update,
	.get_defaults = blur_region_defaults,
	.get_properties = blur_region_properties,
	.get_width = blur_region_width,
	.get_height = blur_region_height,
	.video_tick = blur_region_tick,
	.video_render = blur_region_render,
};
