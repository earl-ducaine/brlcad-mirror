/*                        L S E G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/*----------------------------------------------------------------------*/
/* @file polygon.h */
/** @addtogroup lineseg */
/** @{ */

/**
 *  @brief Functions for working with line segments
 */

#ifndef BG_LSEG_H
#define BG_LSEG_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* Compute the closest points on the line segments P0->P1 and Q0->Q1.  Returns
 * the distance between the closest points and (optionally) the closest points
 * in question (c1 is the point on P0->P1, c2 is the point on Q0->Q1). */
BG_EXPORT double
bg_lseg_lseg_dist(point_t *c1, point_t *c2,
	const point_t P0, const point_t P1, const point_t Q0, const point_t Q1);

__END_DECLS

#endif  /* BG_LSEG_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
