/*
 *			R E F R A C T
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985,1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrefract[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./rdebug.h"
#include "./material.h"
#include "./mathtab.h"

#define MAX_IREFLECT	5	/* Maximum internal reflection level */
#define MAX_BOUNCE	3	/* Maximum recursion level */

#define MSG_PROLOGUE	20		/* # initial messages to see */
#define MSG_INTERVAL	4000		/* message interval thereafter */

#define RI_AIR		1.0    /* Refractive index of air.		*/

HIDDEN int	rr_hit(), rr_miss();
HIDDEN int	rr_refract();

extern vect_t	background;

/*
 *			R R _ R E N D E R
 */
HIDDEN int
rr_render( ap, pp, swp, dp )
register struct application *ap;
struct partition *pp;
struct shadework	*swp;
char	*dp;
{
	auto struct application sub_ap;
	auto fastf_t	f;
	auto vect_t	work;
	auto vect_t	incident_dir;
	auto vect_t	filter_color;

	if( swp->sw_reflect <= 0 && swp->sw_transmit <= 0 )
		goto finish;

	if( ap->a_level >= MAX_BOUNCE )  {
		/* Nothing more to do for this ray */
		static long count = 0;		/* Not PARALLEL, should be OK */

		if( (rdebug&RDEBUG_SHOWERR) && (
			count++ < MSG_PROLOGUE ||
			(count%MSG_INTERVAL) == 3
		) )  {
			rt_log("rr_render: %d,%d Max bounces=%d: %s\n",
				ap->a_x, ap->a_y,
				ap->a_level,
				pp->pt_regionp->reg_name );
		}

		/* 
		 * Return the basic color of the object, ignoring the
		 * the fact that it is supposed to be
		 * filtering or reflecting light here.
		 * This is much better than returning just black,
		 * but something better might be done.
		 */
		VMOVE( swp->sw_color, swp->sw_basecolor );
		goto finish;
	}
	VMOVE( filter_color, swp->sw_basecolor );

	if( (swp->sw_inputs & (MFI_HIT|MFI_NORMAL)) != (MFI_HIT|MFI_NORMAL) )
		shade_inputs( ap, pp, swp, MFI_HIT|MFI_NORMAL );


	/*
	 *  Diminish base color appropriately, and add in
	 *  contributions from mirror reflection & transparency
	 */
	f = 1 - (swp->sw_reflect + swp->sw_transmit);
	if( f < 0 )  f = 0;
	else if( f > 1 )  f = 1;
	VSCALE( swp->sw_color, swp->sw_color, f );

	if( swp->sw_reflect > 0 )  {
		LOCAL vect_t	to_eye;

		/* Mirror reflection */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_onehit = 1;
		VMOVE( sub_ap.a_ray.r_pt, swp->sw_hit.hit_point );
		VREVERSE( to_eye, ap->a_ray.r_dir );
		f = 2 * VDOT( to_eye, swp->sw_hit.hit_normal );
		VSCALE( work, swp->sw_hit.hit_normal, f );
		/* I have been told this has unit length */
		VSUB2( sub_ap.a_ray.r_dir, work, to_eye );
		sub_ap.a_purpose = "reflected ray";
		(void)rt_shootray( &sub_ap );

		/* a_user has hit/miss flag! */
		if( sub_ap.a_user == 0 )  {
			VMOVE( sub_ap.a_color, background );
		}
		VJOIN1(swp->sw_color, swp->sw_color,
			swp->sw_reflect, sub_ap.a_color);
	}
	if( swp->sw_transmit > 0 )  {
		/*
		 *  Calculate refraction at entrance.
		 *  XXX A fairly serious bug with this code is that it doees
		 *  not handle the case of two adjacent pieces of glass,
		 *  ie, the RI of the last/current medium
		 *  does not propagate along the ray.
		 */
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = 0;	/* # of internal reflections */
		sub_ap.a_user = (int)(pp->pt_regionp);
		VMOVE( sub_ap.a_ray.r_pt, swp->sw_hit.hit_point );
		VMOVE( incident_dir, ap->a_ray.r_dir );
		if( !rr_refract(incident_dir, /* Incident ray (IN) */
			swp->sw_hit.hit_normal,
			RI_AIR, swp->sw_refrac_index,
			sub_ap.a_ray.r_dir	/* Refracted ray (OUT) */
		) )  {
			/* Reflected back outside solid */
			VSETALL( filter_color, 1 );
			goto do_exit;
		}

		/*
		 *  Find new exit point from the inside. 
		 *  We will iterate, but not recurse, due to the special
		 *  (non-recursing) hit and miss routines used here for
		 *  internal reflection.
		 */
do_inside:
		sub_ap.a_hit =  rr_hit;
		sub_ap.a_miss = rr_miss;
		sub_ap.a_purpose = "internal reflection";
		sub_ap.a_onehit = 0;	/* need 1st EXIT, not just 1st HIT */
		switch( rt_shootray( &sub_ap ) )  {
		case 2:
			/* All is well */
			break;
		case 1:
			/* Treat as escaping ray */
			goto do_exit;
		case 0:
		default:
			/* Dreadful error */
			VSET( swp->sw_color, 0, 99, 0 );	/* very green */
			goto finish;			/* abandon hope */
		}

		/* NOTE: rr_hit returns EXIT Point in sub_ap.a_uvec,
		 *  and returns EXIT Normal in sub_ap.a_vvec.
		 */
		if( rdebug&RDEBUG_RAYWRITE )  {
			wraypts( sub_ap.a_ray.r_pt, sub_ap.a_uvec,
				ap, stdout );
		}
		if( rdebug&RDEBUG_RAYPLOT )  {
			pl_color( stdout, 0, 255, 0 );
			rt_drawvec( stdout, ap->a_rt_i,
				sub_ap.a_ray.r_pt,
				sub_ap.a_uvec );
		}
		VMOVE( sub_ap.a_ray.r_pt, sub_ap.a_uvec );

		/* Calculate refraction at exit. */
		VMOVE( incident_dir, sub_ap.a_ray.r_dir );
		if( !rr_refract( incident_dir,		/* input direction */
			sub_ap.a_vvec,			/* exit normal */
			swp->sw_refrac_index, RI_AIR,
			sub_ap.a_ray.r_dir		/* output direction */
		) )  {
			static long count = 0;		/* not PARALLEL, should be OK */

			/* Reflected internally -- keep going */
			if( (++sub_ap.a_level) <= MAX_IREFLECT )
				goto do_inside;

			/*
			 *  Internal Reflection limit exceeded -- just let
			 *  the ray escape, continuing on current course.
			 *  This will cause some energy from somewhere in the
			 *  sceen to be received through this glass,
			 *  which is much better than just returning
			 *  grey or black, as before.
			 */
			if( (rdebug&RDEBUG_SHOWERR) && (
				count++ < MSG_PROLOGUE ||
				(count%MSG_INTERVAL) == 3
			) )  {
				rt_log("rr_render: %d,%d Int.reflect=%d: %s lvl=%d\n",
					sub_ap.a_x, sub_ap.a_y,
					sub_ap.a_level,
					pp->pt_regionp->reg_name,
					ap->a_level );
			}
			VMOVE( sub_ap.a_ray.r_dir, incident_dir );
			goto do_exit;
		}
do_exit:
		/*
		 *  Process the escaping refracted ray.
		 *  This is the only place we might recurse dangerously,
		 *  so we are careful to use our caller's recursion level+1.
		 *  NOTE: point & direction already filled in
		 */
		sub_ap.a_hit =  ap->a_hit;
		sub_ap.a_miss = ap->a_miss;
		sub_ap.a_onehit = ap->a_onehit;
		sub_ap.a_level = ap->a_level+1;
		sub_ap.a_purpose = "escaping refracted ray";
		(void) rt_shootray( &sub_ap );

		/* a_user has hit/miss flag! */
		if( sub_ap.a_user == 0 )  {
			VMOVE( sub_ap.a_color, background );
		}
		VELMUL( work, filter_color, sub_ap.a_color );
		VJOIN1( swp->sw_color, swp->sw_color,
			swp->sw_transmit, work );
	}
finish:
	return(1);
}

/*
 *			R R _ M I S S
 */
HIDDEN int
/*ARGSUSED*/
rr_miss( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	return(1);	/* treat as escaping ray */
}

/*
 *			R R _ H I T
 *
 *  This routine is called when an internal reflection ray hits something
 *  (which is ordinarily the case).
 *
 * XXX If this hit resulted from a shot done with the "one hit" flag set,
 * there are no assurances about the accuracy of things behind
 * the ENTRY point.  We need the EXIT point to be accurate.
 * Perhaps this might be good motivation for adding support for
 * a setting of the "one hit" flag that is accurate through the
 * EXIT point, rather than the entry point.
 *
 *  Explicit Returns -
 *	0	dreadful internal error
 *	1	treat as escaping ray & reshoot
 *	2	Proper exit point determined, with Implicit Returns:
 *		a_uvec	exit Point
 *		a_vvec	exit Normal (inward pointing)
 */
HIDDEN int
rr_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *pp;
	register struct hit	*hitp;
	register struct soltab	*stp;
	struct partition	*psave;				

	for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_outhit->hit_dist > 0.0 )  break;
	if( pp == PartHeadp )  {
		if(rdebug&RDEBUG_SHOWERR)  {
			rt_log("rr_hit:  %d,%d no hit out front?\n",
				ap->a_x, ap->a_y );
			return(0);	/* error */
		}
		return(1);		/* treat as escaping ray */
	}

	/*
	 *  Ensure that the partition we are given is part of the same
	 *  region that we started in.  When the internal reflection
	 *  is happening very near an edge or corner, this is not always
	 *  the case, and either (a) a small sliver of some other region
	 *  is found to be in the way, or (b) the ray completely misses the
	 *  region that it started in, although not by much.
	 */
	psave = pp;
	for( ; pp != PartHeadp; pp = pp->pt_forw )
		if( pp->pt_regionp == (struct region *)(ap->a_user) )  break;
	if( pp == PartHeadp )  {
		if(rdebug&RDEBUG_SHOWERR)  {
			rt_log("rr_hit:  %d,%d Ray int.reflected in %s landed in %s\n",
				ap->a_x, ap->a_y,
				((struct region *)(ap->a_user))->reg_name,
				psave->pt_regionp->reg_name );
			return(0);	/* error */
		}
		return(1);		/* treat as escaping ray */
	}

	/*
	 *  At one time, this was a check for pp->pt_inhit->hit_dist
	 *  being NEAR zero.  That was a mistake, because we may have
	 *  been at the edge of a subtracted out center piece when
	 *  internal reflection happened, except that floating point
	 *  error (being right on the surface of the interior solid)
	 *  prevented us from "seeing" that solid on the next ray,
	 *  causing our ray endpoints to be quite far from the starting
	 *  point, yet with the ray still validly inside the glass region.
	 *
	 *  There is a major problem if the entry point
	 *  is further ahead than the firing point, ie, >0.
	 *
	 *  Because this error has not yet been encountered, it is
	 *  considered dreadful.  Some recovery may be possible.
	 */
	if( pp->pt_inhit->hit_dist > 10 )  {
		stp = pp->pt_inseg->seg_stp;
		rt_log("rr_hit: %d,%d %s inhit %g > 0.0!\n",
			ap->a_x, ap->a_y,
			pp->pt_regionp->reg_name,
			hitp->hit_dist);
		return(0);		/* dreadful error */
	}

	/*
	 * If there is a very small crack in the glass, perhaps formed
	 * by a small error when taking the Union of two solids,
	 * attempt to find the real exit point.
	 * NOTE that this is usually taken care of inside librt
	 * in the bool_weave code, but it is inexpensive to check for it
	 * here.  If this case is detected, push on, and log it.
	 * This code is not expected to be needed.
	 */
	while( pp->pt_forw != PartHeadp )  {
		register fastf_t d;
		d = pp->pt_forw->pt_inhit->hit_dist - pp->pt_outhit->hit_dist;
		if( !NEAR_ZERO( d, 1.0e-3 ) )	/* XXX absolute tolerance */
			break;
		if( pp->pt_forw->pt_regionp != pp->pt_regionp )
			break;
		rt_log("rr_hit: %d,%d fusing small crack in glass %s\n",
			ap->a_x, ap->a_y,
			pp->pt_regionp->reg_name );
		pp = pp->pt_forw;
	}

	hitp = pp->pt_outhit;
	stp = pp->pt_outseg->seg_stp;
	if( hitp->hit_dist >= INFINITY )  {
		rt_log("rr_hit: %d,%d infinite glass (%g,%g) %s\n",
			ap->a_x, ap->a_y,
			pp->pt_inhit->hit_dist, hitp->hit_dist,
			pp->pt_regionp->reg_name );
		return(0);		/* dreadful error */
	}
	VJOIN1( hitp->hit_point, ap->a_ray.r_pt,
		hitp->hit_dist, ap->a_ray.r_dir );
	RT_HIT_NORM( hitp, stp, &(ap->a_ray) );
	if( pp->pt_outflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}

	VMOVE( ap->a_uvec, hitp->hit_point );

	/* For refraction, want exit normal to point inward. */
	VREVERSE( ap->a_vvec, hitp->hit_normal );
	return(2);			/* OK */
}

/*
 *			R E F R A C T
 *
 *	Compute the refracted ray 'v_2' from the incident ray 'v_1' with
 *	the refractive indices 'ri_2' and 'ri_1' respectively.
 *	Using Schnell's Law:
 *
 *		theta_1 = angle of v_1 with surface normal
 *		theta_2 = angle of v_2 with reversed surface normal
 *		ri_1 * sin( theta_1 ) = ri_2 * sin( theta_2 )
 *
 *		sin( theta_2 ) = ri_1/ri_2 * sin( theta_1 )
 *		
 *	The above condition is undefined for ri_1/ri_2 * sin( theta_1 )
 *	being greater than 1, and this represents the condition for total
 *	reflection, the 'critical angle' is the angle theta_1 for which
 *	ri_1/ri_2 * sin( theta_1 ) equals 1.
 *
 *  Returns TRUE if refracted, FALSE if reflected.
 *
 *  Note:  output (v_2) can be same storage as an input.
 */
HIDDEN int
rr_refract( v_1, norml, ri_1, ri_2, v_2 )
register vect_t	v_1;
register vect_t	norml;
double	ri_1, ri_2;
register vect_t	v_2;
{
	LOCAL vect_t	w, u;
	FAST fastf_t	beta;

	if( NEAR_ZERO(ri_1, 0.0001) || NEAR_ZERO( ri_2, 0.0001 ) )  {
		rt_log("rr_refract:ri1=%g, ri2=%g\n", ri_1, ri_2 );
		beta = 1;
	} else {
		beta = ri_1/ri_2;		/* temp */
		if( beta > 10000 )  {
			rt_log("rr_refract:  beta=%g\n", beta);
			beta = 1000;
		}
	}
	VSCALE( w, v_1, beta );
	VCROSS( u, w, norml );
	    	
	/*
	 *	|w X norml| = |w||norml| * sin( theta_1 )
	 *	        |u| = ri_1/ri_2 * sin( theta_1 ) = sin( theta_2 )
	 */
	if( (beta = VDOT( u, u )) > 1.0 )  {
		/*  Past critical angle, total reflection.
		 *  Calculate reflected (bounced) incident ray.
		 */
		VREVERSE( u, v_1 );
		beta = 2 * VDOT( u, norml );
		VSCALE( w, norml, beta );
		VSUB2( v_2, w, u );
		return(0);		/* reflected */
	} else {
		/*
		 * 1 - beta = 1 - sin( theta_2 )^^2
		 *	    = cos( theta_2 )^^2.
		 *     beta = -1.0 * cos( theta_2 ) - Dot( w, norml ).
		 */
		beta = -sqrt( 1.0 - beta) - VDOT( w, norml );
		VSCALE( u, norml, beta );
		VADD2( v_2, w, u );
		return(1);		/* refracted */
	}
	/* NOTREACHED */
}
