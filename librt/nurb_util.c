/*
 *			N U R B _ U T I L . C
 *
 *  Function -
 *	Utilities for NURB curves and surfaces.
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/* Create a place holder for a nurb surface. */

struct snurb *
rt_nurb_new_snurb(u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, pt_type)
int u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, pt_type;
{
	register struct snurb * srf;
	int pnum;
	
	GET_SNURB(srf);
	srf->order[0] = u_order;
	srf->order[1] = v_order;
	srf->dir = RT_NURB_SPLIT_ROW;

	srf->u_knots.k_size = n_u_knots;
	srf->v_knots.k_size = n_v_knots;

	srf->u_knots.knots = (fastf_t *) rt_malloc ( 
		n_u_knots * sizeof (fastf_t ), "rt_nurb_new_snurb: u kv knot values");
	srf->v_knots.knots = (fastf_t *) rt_malloc ( 
		n_v_knots * sizeof (fastf_t ), "rt_nurb_new_snurb: v kv knot values");

	srf->s_size[0] = n_rows;
	srf->s_size[1] = n_cols;
	srf->pt_type = pt_type;
	
	pnum = sizeof (fastf_t) * n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);
	srf->ctl_points = ( fastf_t *) rt_malloc( 
		pnum, "rt_nurb_new_snurb: control mesh points");

	return srf;
}

/* Create a place holder for a new nurb curve. */
struct cnurb *
rt_nurb_new_cnurb( order, n_knots, n_pts, pt_type)
int order, n_knots, n_pts, pt_type;
{
	register struct cnurb * crv;

	GET_CNURB(crv);
	crv->order = order;

	crv->knot.k_size = n_knots;
	crv->knot.knots = (fastf_t *)
		rt_malloc(n_knots * sizeof(fastf_t),
			"rt_nurb_new_cnurb: knot values");

	crv->c_size = n_pts;
	crv->pt_type = pt_type;

	crv->ctl_points = (fastf_t *)
		rt_malloc( sizeof(fastf_t) * RT_NURB_EXTRACT_COORDS(pt_type) *
			n_pts, 
			"rt_nurb_new_cnurb: mesh point values");

	return crv;
}

/* free routine for a nurb surface */

void
rt_nurb_free_snurb( srf )
struct snurb * srf;
{
	NMG_CK_SNURB(srf);
    /* assume that links to other surface and curves are already deleted */

    rt_free( (char *)srf->u_knots.knots, "rt_nurb_free_snurb: u kv knots" );
    rt_free( (char *)srf->v_knots.knots, "rt_nurb_free_snurb: v kv knots" );
    rt_free( (char *)srf->ctl_points, "rt_nurb_free_snurb: mesh points");

    rt_free( (char *)srf, "rt_nurb_free_snurb: snurb struct" );
}


/* free routine for a nurb curve */

void
rt_nurb_free_cnurb( crv)
struct cnurb * crv;
{
	NMG_CK_CNURB(crv);
	rt_free( (char*)crv->knot.knots, "rt_nurb_free_cnurb: knots");
	rt_free( (char*)crv->ctl_points, "rt_nurb_free_cnurb: control points");
	rt_free( (char*)crv, "rt_nurb_free_cnurb: cnurb struct");

}

void
rt_nurb_c_print( crv)
CONST struct cnurb * crv;
{
	register fastf_t * ptr;
	int i,j;

	NMG_CK_CNURB(crv);
	rt_log("curve = {\n");
	rt_log("\tOrder = %d\n", crv->order);
	rt_log("\tKnot Vector = {\n\t\t");

	for( i = 0; i < crv->knot.k_size; i++)
		rt_log("%3.2f  ", crv->knot.knots[i]);

	rt_log("\n\t}\n");
	rt_log("\t");
	rt_nurb_print_pt_type(crv->pt_type);
	rt_log("\tmesh = {\n");
	for( ptr = &crv->ctl_points[0], i= 0;
		i < crv->c_size; i++, ptr += RT_NURB_EXTRACT_COORDS(crv->pt_type))
	{
		rt_log("\t\t");
		for(j = 0; j < RT_NURB_EXTRACT_COORDS(crv->pt_type); j++)
			rt_log("%4.5f\t", ptr[j]);
		rt_log("\n");

	}
	rt_log("\t}\n}\n");
	

}

void
rt_nurb_s_print( c, srf )
char * c;
CONST struct snurb * srf;
{

    rt_log("%s\n", c );

    rt_log("order %d %d\n", srf->order[0], srf->order[1] );

    rt_log( "u knot vector \n");

    rt_nurb_pr_kv( &srf->u_knots );

    rt_log( "v knot vector \n");

    rt_nurb_pr_kv( &srf->v_knots );

    rt_nurb_pr_mesh( srf );

}

void
rt_nurb_pr_kv( kv )
CONST struct knot_vector * kv;
{
    register fastf_t * ptr = kv->knots;
    int i;

    rt_log("[%d]\t", kv->k_size );


    for( i = 0; i < kv->k_size; i++)
    {
	rt_log("%2.5f  ", *ptr++);
    }
    rt_log("\n");
}

void
rt_nurb_pr_mesh( m )
CONST struct snurb * m;
{
	int i,j,k;
	fastf_t * m_ptr = m->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS(m->pt_type);

	NMG_CK_SNURB(m);

	rt_log("\t[%d] [%d]\n", m->s_size[0], m->s_size[1] );

	for( i = 0; i < m->s_size[0]; i++)
	{
		for( j =0; j < m->s_size[1]; j++)
		{
			rt_log("\t");

			for(k = 0; k < evp; k++)
				rt_log("%f    ", m_ptr[k]);

			rt_log("\n");
			m_ptr += RT_NURB_EXTRACT_COORDS(m->pt_type);
		}
		rt_log("\n");
	}
}

void
rt_nurb_print_pt_type(c)
int c;
{
	int rat;

	rat = RT_NURB_IS_PT_RATIONAL(c);
	
	if( RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_XY)
		rt_log("Point Type = RT_NURB_PT_XY");
	else 
	if( RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_XYZ)
		rt_log("Point Type = RT_NURB_PT_XYX");
	else 
	if( RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_UV)
		rt_log("Point Type = RT_NURB_PT_UV");

	if( rat )
		rt_log("W\n");
	else
		rt_log("\n");
}
