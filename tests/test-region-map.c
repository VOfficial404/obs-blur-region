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

#include "region-map.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

static struct region_placement placement(float scale_x, float scale_y, float tx, float ty)
{
	struct region_placement p = {scale_x, 0.0f, 0.0f, scale_y, tx, ty, 0.0f, 0.0f};
	return p;
}

/* Maps region pixel (x, y) and compares with the expected target pixel. */
static void expect_point(const char *name, const struct region_placement *region, const struct region_placement *target,
			 float x, float y, float want_x, float want_y)
{
	float mx[3], my[3];
	if (!region_map_compute(region, target, mx, my)) {
		printf("FAIL %s: mapping rejected\n", name);
		failures++;
		return;
	}
	const float got_x = mx[0] * x + mx[1] * y + mx[2];
	const float got_y = my[0] * x + my[1] * y + my[2];
	if (fabsf(got_x - want_x) > 1e-3f || fabsf(got_y - want_y) > 1e-3f) {
		printf("FAIL %s: (%g, %g) -> (%g, %g), want (%g, %g)\n", name, x, y, got_x, got_y, want_x, want_y);
		failures++;
	}
}

int main(void)
{
	const struct region_placement identity = placement(1.0f, 1.0f, 0.0f, 0.0f);
	expect_point("identity", &identity, &identity, 12.0f, 34.0f, 12.0f, 34.0f);

	/* Game scaled 2x at (100, 50); region at (300, 250) unscaled. */
	const struct region_placement game = placement(2.0f, 2.0f, 100.0f, 50.0f);
	const struct region_placement region = placement(1.0f, 1.0f, 300.0f, 250.0f);
	expect_point("scaled target", &region, &game, 0.0f, 0.0f, 100.0f, 100.0f);
	expect_point("scaled target, offset", &region, &game, 10.0f, 4.0f, 105.0f, 102.0f);

	/* Region resized by dragging: its pixels stretch over twice the canvas. */
	const struct region_placement stretched = placement(2.0f, 3.0f, 0.0f, 0.0f);
	expect_point("stretched region", &stretched, &identity, 10.0f, 10.0f, 20.0f, 30.0f);

	/* Game cropped by (20, 10) and drawn at the origin: canvas (50, 50) shows game pixel (70, 60). */
	struct region_placement cropped_game = identity;
	cropped_game.crop_x = 20.0f;
	cropped_game.crop_y = 10.0f;
	const struct region_placement at_50 = placement(1.0f, 1.0f, 50.0f, 50.0f);
	expect_point("cropped target", &at_50, &cropped_game, 0.0f, 0.0f, 70.0f, 60.0f);

	/* Cropped region: its pixel (10, 0) is the first one drawn, at the item's position. */
	struct region_placement cropped_region = at_50;
	cropped_region.crop_x = 10.0f;
	expect_point("cropped region", &cropped_region, &identity, 10.0f, 0.0f, 50.0f, 50.0f);

	/* Game rotated 90 degrees clockwise at (100, 0): its pixel (5, 0) lands on canvas (100, 5). */
	const struct region_placement rotated = {0.0f, 1.0f, -1.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f};
	expect_point("rotated target", &identity, &rotated, 100.0f, 5.0f, 5.0f, 0.0f);
	expect_point("rotated target, y", &identity, &rotated, 97.0f, 0.0f, 0.0f, 3.0f);

	float mx[3], my[3];
	const struct region_placement collapsed = placement(0.0f, 1.0f, 0.0f, 0.0f);
	if (region_map_compute(&identity, &collapsed, mx, my)) {
		printf("FAIL collapsed target: mapping accepted\n");
		failures++;
	}

	if (failures == 0)
		printf("all region-map tests passed\n");
	return failures == 0 ? 0 : 1;
}
