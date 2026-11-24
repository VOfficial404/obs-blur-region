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

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where a scene item lands on its scene: canvas = (a*x + c*y + tx, b*x + d*y + ty), with (x, y) the
 * source pixel minus the item's crop offset. a..ty are the matching fields of
 * obs_sceneitem_get_draw_transform (x.x, x.y, y.x, y.y, t.x, t.y). */
struct region_placement {
	float a, b, c, d;
	float tx, ty;
	float crop_x, crop_y;
};

/* Affine map from a pixel of the region item to the target source pixel under it:
 * target = (map_x[0]*x + map_x[1]*y + map_x[2], map_y[0]*x + map_y[1]*y + map_y[2]).
 * Both items must sit in the same scene. Returns false when the target's transform cannot be
 * inverted (scaled to zero). */
bool region_map_compute(const struct region_placement *region, const struct region_placement *target, float map_x[3],
			float map_y[3]);

#ifdef __cplusplus
}
#endif
