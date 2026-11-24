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

bool region_map_compute(const struct region_placement *r, const struct region_placement *t, float map_x[3],
			float map_y[3])
{
	const double det = (double)t->a * t->d - (double)t->c * t->b;
	if (fabs(det) < 1e-9)
		return false;

	/* Inverse of the target's linear part [[a, c], [b, d]]. */
	const double ia = t->d / det, ic = -t->c / det;
	const double ib = -t->b / det, id = t->a / det;

	/* Region pixel -> canvas, relative to the target's origin: (X, Y) = M_r * (p - crop_r) + t_r - t_t. */
	const double ox = (double)r->tx - (double)r->a * r->crop_x - (double)r->c * r->crop_y - t->tx;
	const double oy = (double)r->ty - (double)r->b * r->crop_x - (double)r->d * r->crop_y - t->ty;

	/* Canvas -> target pixel: M_t^-1 * (X, Y) + crop_t. */
	map_x[0] = (float)(ia * r->a + ic * r->b);
	map_x[1] = (float)(ia * r->c + ic * r->d);
	map_x[2] = (float)(ia * ox + ic * oy + t->crop_x);
	map_y[0] = (float)(ib * r->a + id * r->b);
	map_y[1] = (float)(ib * r->c + id * r->d);
	map_y[2] = (float)(ib * ox + id * oy + t->crop_y);
	return true;
}
