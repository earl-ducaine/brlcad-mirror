/*
 *                      F A S T 4 - G
 *
 *  Program to convert the FASTGEN4 format to BRL-CAD.
 *
 *  Author -
 *      John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "db.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

extern int errno;
extern int optind,opterr;
extern char *optarg;

#define		LINELEN		128	/* Length of char array for input line */

static char	line[LINELEN];		/* Space for input line */
static FILE	*fdin;			/* Input FASTGEN4 file pointer */
static FILE	*fdout;			/* Output BRL-CAD file pointer */
static int	grid_size;		/* Number of points that will fit in current grid_pts array */
static int	max_grid_no=0;		/* Maximum grid number used */
static int	mode=0;			/* Plate mode (1) or volume mode (2) */
static int	group_id=(-1);		/* Group identification number from SECTION card */
static int	comp_id=(-1);		/* Component identification number from SECTION card */
static int	region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static int	joint_no=0;		/* Count of CLINE joints (sph solids created to connect CLINES) */
static char	field[9];		/* Space for storing one field from an input line */
static char	vehicle[17];		/* Title for BRLCAD model from VEHICLE card */
static char	name_name[NAMESIZE+1];	/* Component name built from $NAME card */
static int	name_count;		/* Count of number of times this name_name has been used */
static int	pass;			/* Pass number (0 -> only make names, 1-> do geometry ) */
static int	nmgs=0;			/* Flag: >0 -> There are NMG's in current component */
static int	warnings=0;		/* Flag: >0 -> Print warning messages */
static int	debug=0;		/* Debug flag */
static int	rt_debug=0;		/* rt_g.debug */
static int	nmg_debug=0;		/* rt_g.NMG_debug */
static int	polysolids=0;		/* Flag: >0 -> Build polysolids, not NMG's */
static int	comp_count=0;		/* Count of components in FASTGEN4 file */
static int	conv_count=0;		/* Count of components successfully converted to BRLCAD */
static int	second_chance=0;	/* Count of PLATE-MODE objects converted on second try */
static long	curr_offset=0;		/* Offset into input file for current element */
static long	curr_sect=0;		/* Offset into input file for current section card */
static long	prev_sect=0;		/* Offset into input file for previous section card */
static int	try_count=0;		/* Counter for number of tries to build currect section */
static struct cline    *cline_last_ptr; /* Pointer to last element in linked list of clines */
static struct wmember  group_head[11];	/* Lists of regions for groups */
static struct nmg_ptbl stack;		/* Stack for traversing name_tree */
static struct model	*m;		/* NMG model for surface elements */
static struct nmgregion	*r;		/* NMGregion */
static struct shell	*s;		/* NMG shell */
static struct rt_tol	tol;		/* Tolerance struct for NMG's */
static char	*usage="Usage:\n\tfast4-g [-dwp] [-x RT_DEBUG_FLAG] [-X NMG_DEBUG_FLAG] [-D distance] [-P cosine] fastgen4_bulk_data_file output.g\n\
	d - print debugging info\n\
	w - print warnings about creating default names\n\
	p - make polysolids rather than NMG solids\n\
	x - set RT debug flag\n\
	X - set NMG debug flag\n\
	D - set tolerance distance (mm)\n\
	P - set tolerance for parallel test (cosine of angle)\n";

RT_EXTERN( fastf_t nmg_loop_plane_area , ( struct loopuse *lu , plane_t pl ) );
RT_EXTERN( struct shell *nmg_dup_shell , ( struct shell *s , long ***copy_tbl ) );
RT_EXTERN( struct shell *nmg_extrude_shell , ( struct shell *s1 , fastf_t thick , int normal_ward , int approximate , struct rt_tol *tol ) );
RT_EXTERN( struct edgeuse *nmg_next_radial_eu , ( CONST struct edgeuse *eu , CONST struct shell *s , int wires ) );
RT_EXTERN( struct faceuse *nmg_mk_new_face_from_loop , ( struct loopuse *lu ) );
RT_EXTERN( fastf_t mat_determinant , (mat_t matrix ) );

static char	*mode_str[3]=		/* mode strings */
{
	"unknown mode",
	"plate mode",
	"volume mode"
};

#define		PLATE_MODE	1
#define		VOLUME_MODE	2

#define		POS_CENTER	1	/* face positions for facets */
#define		POS_FRONT	2

#define		END_OPEN	1	/* End closure codes for cones */
#define		END_CLOSED	2

#define		GRID_BLOCK	256	/* allocate space for grid points in blocks of 256 points */

#define		CLINE		'l'
#define		CHEX1		'p'
#define		CHEX2		'b'
#define		CTRI		't'
#define		CQUAD		'q'
#define		CCONE1		'c'
#define		CCONE2		'd'
#define		CSPHERE		's'
#define		NMG		'n'

/* convenient macro for building regions */
#define		MK_REGION( fp , headp , g_id , c_id , e_id , type ) \
			{\
				int r_id; \
				char name[NAMESIZE+1]; \
				r_id = g_id * 1000 + c_id; \
				make_region_name( name , g_id , c_id , e_id , type ); \
				mk_lrcomb( fp , name , headp , 1 ,\
					(char *)NULL, (char *)NULL, (char *)NULL, r_id, 0, 0, 0, 0 ); \
			}

#define	PUSH( ptr )	nmg_tbl( &stack , TBL_INS , (long *)ptr )
#define POP( structure , ptr )	{ if( NMG_TBL_END( &stack ) == 0 ) \
				ptr = (struct structure *)NULL; \
			  else \
			  { \
			  	ptr = (struct structure *)NMG_TBL_GET( &stack , NMG_TBL_END( &stack )-1 ); \
			  	nmg_tbl( &stack , TBL_RM , (long *)ptr ); \
			  } \
			}

#define	NHEX_FACES	12
int hex_faces[12][3]={
	{ 0 , 1 , 4 }, /* 1 */
	{ 1 , 5 , 4 }, /* 2 */
	{ 1 , 2 , 5 }, /* 3 */
	{ 2 , 6 , 5 }, /* 4 */
	{ 2 , 3 , 6 }, /* 5 */
	{ 3 , 7 , 6 }, /* 6 */
	{ 3 , 0 , 7 }, /* 7 */
	{ 0 , 4 , 7 }, /* 8 */
	{ 4 , 6 , 7 }, /* 9 */
	{ 4 , 5 , 6 }, /* 10 */
	{ 0 , 1 , 2 }, /* 11 */
	{ 0 , 2 , 3 }  /* 12 */
};

struct cline
{
	int pt1,pt2;
	int element_id;
	int made;
	fastf_t thick;
	fastf_t radius;
	struct cline *next;
} *cline_root;

struct name_tree
{
	int region_id;
	int element_id;		/* > 0  -> normal fastgen4 element id
				 * < 0  -> CLINE element (-pt1)
				 * == 0 -> component name */
	int in_comp_group;	/* > 0 -> region already in a component group */
	char name[NAMESIZE+1];
	struct name_tree *nleft,*nright,*rleft,*rright;
} *name_root;

struct hole_list
{
	int group;
	int component;
	struct hole_list *next;
};

struct holes
{
	int group;
	int component;
	struct hole_list *holes;
	struct holes *next;
} *hole_root;

struct fast_verts
{
	point_t pt;
	struct vertex *v;
} *grid_pts;

struct fast_fus
{
	struct faceuse *fu;
	fastf_t thick;
	int pos;
	struct fast_fus *next;
} *fus_root;

struct shell_list
{
	struct shell *s;
	fastf_t thick;
	int pos;
	struct shell_list *next;
} *shell_root;

struct adjacent_faces
{
	struct faceuse *fu1,*fu2;
	struct edgeuse *eu1,*eu2;
	struct adjacent_faces *next;
} *adj_root;

void
tmp_shell_coplanar_face_merge( s, tmp_tol, simplify )
struct shell		*s;
CONST struct rt_tol	*tmp_tol;
CONST int		simplify;
{
	struct model	*m;
	int		len;
	char		*flags1;
	char		*flags2;
	struct faceuse	*fu1;
	struct faceuse	*fu2;
	struct face	*f1;
	struct face	*f2;
	struct face_g	*fg1;
	struct face_g	*fg2;

	NMG_CK_SHELL(s);
	m = nmg_find_model( &s->l.magic );
	len = sizeof(char) * m->maxindex * 2;
	flags1 = (char *)rt_calloc( sizeof(char), m->maxindex * 2,
		"tmp_shell_coplanar_face_mergetmp flags1[]" );
	flags2 = (char *)rt_calloc( sizeof(char), m->maxindex * 2,
		"tmp_shell_coplanar_face_merge flags2[]" );

	/* Visit each face in the shell */
	for( RT_LIST_FOR( fu1, faceuse, &s->fu_hd ) )  {
		plane_t		n1;

		if( RT_LIST_NEXT_IS_HEAD(fu1, &s->fu_hd) )  break;
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);
		if( NMG_INDEX_TEST(flags1, f1) )  continue;
		NMG_INDEX_SET(flags1, f1);

		fg1 = f1->fg_p;
		NMG_CK_FACE_G(fg1);
		NMG_GET_FU_PLANE( n1, fu1 );

		/* For this face, visit all remaining faces in the shell. */
		/* Don't revisit any faces already considered. */
		bcopy( flags1, flags2, len );
		for( fu2 = RT_LIST_NEXT(faceuse, &fu1->l);
		     RT_LIST_NOT_HEAD(fu2, &s->fu_hd);
		     fu2 = RT_LIST_NEXT(faceuse,&fu2->l)
		)  {
			register fastf_t	dist;
			plane_t			n2;

			f2 = fu2->f_p;
			NMG_CK_FACE(f2);
			if( NMG_INDEX_TEST(flags2, f2) )  continue;
			NMG_INDEX_SET(flags2, f2);

			fg2 = f2->fg_p;
			NMG_CK_FACE_G(fg2);

			if( fu2->fumate_p == fu1 || fu1->fumate_p == fu2 )
				rt_bomb("tmp_shell_coplanar_face_merge() mate confusion\n");

			/* See if face geometry is shared & same direction */
			if( fg1 != fg2 || f1->flip != f2->flip )  {
				/* If plane equations are different, done */
				NMG_GET_FU_PLANE( n2, fu2 );

				/* Compare distances from origin */
				dist = n1[3] - n2[3];
				if( !NEAR_ZERO(dist, tmp_tol->dist) )  continue;

				/*
				 *  Compare angle between normals.
				 *  Can't just use RT_VECT_ARE_PARALLEL here,
				 *  because they must point in the same direction.
				 */
				dist = VDOT( n1, n2 );
				if( !(dist >= tmp_tol->para) )  continue;
			}

			/*
			 * Plane equations are the same, within tolerance,
			 * or by shared fg topology.
			 * Move everything into fu1, and
			 * kill now empty faceuse, fumate, and face
			 */
			{
				struct faceuse	*prev_fu;

				prev_fu = RT_LIST_PREV(faceuse, &fu2->l);
				/* The prev_fu can never be the head */
				if( RT_LIST_IS_HEAD(prev_fu, &s->fu_hd) )
					rt_bomb("prev is head?\n");
				if( nmg_ck_fu_verts( fu1 , fu1->f_p , tmp_tol ) )
					rt_bomb( "tmp_shell_coplanar_face_merge: verts not on face\n" );

				if( !nmg_ck_fg_verts( fu2 , fu1->f_p , tmp_tol ) )
				{
					if( debug )
					{
						rt_log( "Merging coplanar faces:\n" );
						rt_log( "plane = ( %f %f %f %f )\n" , V4ARGS( fu1->f_p->fg_p->N ) );
						nmg_pr_fu_briefly( fu1 , (char *)NULL );
						rt_log( "plane = ( %f %f %f %f )\n" , V4ARGS( fu2->f_p->fg_p->N ) );
						nmg_pr_fu_briefly( fu2 , (char *)NULL );
					}
					nmg_jf( fu1, fu2 );
					if( nmg_ck_fg_verts( fu1 , fu1->f_p , tmp_tol ) )
						rt_bomb( "tmp_coplanar_face_merge: joined faces made bad face\n" );
				}

				fu2 = prev_fu;
			}

			/* There is now the option of simplifying the face,
			 * by removing unnecessary edges.
			 */
			if( simplify )  {
				struct loopuse *lu;

				for (RT_LIST_FOR(lu, loopuse, &fu1->lu_hd))
					nmg_simplify_loop(lu);
			}
		}
	}
	rt_free( (char *)flags1, "tmp_shell_coplanar_face_merge flags1[]" );
	rt_free( (char *)flags2, "tmp_shell_coplanar_face_merge flags2[]" );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		rt_log("tmp_shell_coplanar_face_merge(s=x%x, tol=x%x, simplify=%d)\n",
			s, tmp_tol, simplify);
	}
}

void
Subtract_holes( head , comp_id , group_id )
struct wmember *head;
int comp_id;
int group_id;
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	if( debug )
		rt_log( "Subtract_holes( comp_id=%d, group_id=%d )\n" , comp_id , group_id );

	hole_ptr = hole_root;
	while( hole_ptr )
	{
		if( hole_ptr->group == group_id && hole_ptr->component == comp_id )
		{
			list_ptr = hole_ptr->holes;
			while( list_ptr )
			{
				struct name_tree *ptr;
				int reg_id;

				reg_id = list_ptr->group * 1000 + list_ptr->component;
				ptr = name_root;
				while( ptr && ptr->region_id != reg_id )
				{
					int diff;

					diff = reg_id - ptr->region_id;
					if( diff > 0 )
						ptr = ptr->rright;
					else if( diff < 0 )
						ptr = ptr->rleft;
				}

				if( !ptr || ptr->region_id != reg_id )
				{
					rt_log( "ERROR: Can't find holes to subtract for group %d, component %d\n" , group_id , comp_id );
					return;
				}

				nmg_tbl( &stack , TBL_RST , (long *)NULL );

				while( ptr && ptr->region_id == reg_id )
				{
					while( ptr && ptr->region_id == reg_id )
					{
						PUSH( ptr );
						ptr = ptr->rleft;
					}
					POP( name_tree , ptr );
					if( !ptr ||  ptr->region_id != reg_id )
						break;

					if( mk_addmember( ptr->name , head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
						rt_bomb( "Subtract_holes: mk_addmember failed\n" );

					ptr = ptr->rright;
				}

				list_ptr = list_ptr->next;
			}
			break;
		}
		hole_ptr = hole_ptr->next;
	}
}

void
List_holes()
{
	struct holes *hole_ptr;
	struct hole_list *list_ptr;

	hole_ptr = hole_root;

	while( hole_ptr )
	{
		rt_log( "Holes for Group %d, Component %d:\n" , hole_ptr->group, hole_ptr->component );
		list_ptr = hole_ptr->holes;
		while( list_ptr )
		{
			rt_log( "\tgroup %d component %d\n" , list_ptr->group, list_ptr->component );
			list_ptr = list_ptr->next;
		}
		hole_ptr = hole_ptr->next;
	}
}

void
List_names()
{
	struct name_tree *ptr;

	nmg_tbl( &stack , TBL_RST , (long *)NULL );

	rt_log( "\nNames in ident order:\n" );
	ptr = name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->rleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		rt_log( "%s %d %d\n" , ptr->name , ptr->region_id , ptr->element_id );
		ptr = ptr->rright;
	}

	rt_log( "\tAlphabetical list of names:\n" );
	ptr = name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		rt_log( "%s\n" , ptr->name );
		ptr = ptr->nright;
	}
}

struct name_tree *
Search_names( root , name , found )
struct name_tree *root;
char *name;
int *found;
{
	struct name_tree *ptr;

	*found = 0;

	ptr = root;
	if( !ptr )
		return( (struct name_tree *)NULL );

	while( 1 )
	{
		int diff;

		diff = strcmp( name , ptr->name );
		if( diff == 0 )
		{
			*found = 1;
			return( ptr );
		}
		else if( diff > 0 )
		{
			if( ptr->nright )
				ptr = ptr->nright;
			else
				return( ptr );
		}
		else if( diff < 0 )
		{
			if( ptr->nleft )
				ptr = ptr->nleft;
			else
				return( ptr );
		}
	}
}


struct name_tree *
Search_ident( root , reg_id , el_id , found )
struct name_tree *root;
int reg_id;
int el_id;
int *found;
{
	struct name_tree *ptr;

	*found = 0;

	ptr = root;
	if( !ptr )
		return( (struct name_tree *)NULL );

	while( 1 )
	{
		int diff;

		diff = reg_id -  ptr->region_id;
		if( diff == 0 )
			diff = el_id - ptr->element_id;

		if( diff == 0 )
		{
			*found = 1;
			return( ptr );
		}
		else if( diff > 0 )
		{
			if( ptr->rright )
				ptr = ptr->rright;
			else
				return( ptr );
		}
		else if( diff < 0 )
		{
			if( ptr->rleft )
				ptr = ptr->rleft;
			else
				return( ptr );
		}
	}
}

void
Delete_name( root , name )
struct name_tree *root;
char *name;
{
	struct name_tree *ptr,*parent,*ptr2;
	int r_id, e_id;
	int found;
	int diff;

	/* first delete from name portion of tree */
	ptr = root;
	parent = (struct name_tree *)NULL;
	found = 0;

	while( 1 )
	{
		diff = strcmp( name , ptr->name );
		if( diff == 0 )
		{
			found = 1;
			break;
		}
		else if( diff > 0 )
		{
			if( ptr->nright )
			{
				parent = ptr;
				ptr = ptr->nright;
			}
			else
				break;
		}
		else if( diff < 0 )
		{
			if( ptr->nleft )
			{
				parent = ptr;
				ptr = ptr->nleft;
			}
			else
				break;
		}
	}

	if( !found )
		return;

	r_id = ptr->region_id;
	e_id = ptr->element_id;

	if( parent == (struct name_tree *)NULL )
	{
		if( ptr->nright )
		{
			root = ptr->nright;
			ptr2 = root;
			while( ptr2->nleft )
				ptr2 = ptr2->nleft;
			ptr2->nleft = ptr->nleft;

			ptr2 = root;
			while( ptr2->rleft )
				ptr2 = ptr2->rleft;
			ptr2->rleft = ptr->rleft;
		}
		else
		{
			root = ptr->nleft;
			ptr2 = root;
			while( ptr2->nright )
				ptr2 = ptr2->nright;
			ptr2->nright = ptr->nright;
			ptr2 = root;
			while( ptr2->rright )
				ptr2 = ptr2->rright;
			ptr2->rright = ptr->rright;
		}

		rt_free( (char *)ptr , "Delete_name: ptr" );
		return;

	}
	else
	{
		if( parent->nright == ptr )
		{
			if( ptr->nleft )
			{
				parent->nright = ptr->nleft;
				ptr2 = ptr->nleft;
				while( ptr2->nright )
					ptr2 = ptr2->nright;
				ptr2->nright = ptr->nright;
			}
			else
				parent->nright = ptr->nright;
		}
		else if( parent->nleft == ptr )
		{
			if( ptr->nright )
			{
				parent->nleft = ptr->nright;
				ptr2 = ptr->nright;
				while( ptr2->nleft )
					ptr2 = ptr2->nleft;
				ptr2->nleft = ptr->nleft;
			}
			else
				parent->nleft = ptr->nleft;
		}
	}


	/* now delete from ident prtion of tree */
	ptr = root;
	parent = (struct name_tree *)NULL;
	found = 0;

	while( 1 )
	{
		diff = r_id - ptr->region_id;
		if( diff == 0 )
			diff = e_id - ptr->element_id;

		if( diff == 0 )
		{
			found = 1;
			break;
		}
		else if( diff > 0 )
		{
			if( ptr->rright )
			{
				parent = ptr;
				ptr = ptr->rright;
			}
			else
				break;
		}
		else if( diff < 0 )
		{
			if( ptr->rleft )
			{
				parent = ptr;
				ptr = ptr->rleft;
			}
			else
				break;
		}
	}

	if( !found )
	{
		rt_log( "name (%s) deleted from name tree, but not found in ident tree!!\n" , name );
		rt_bomb( "Delete_name\n" );
	}

	if( !parent )
	{
		rt_log( "name (%s) is root of ident tree, but not name tree!!\n" , name );
		rt_bomb( "Delete_name\n" );
	}


	if( parent->rright == ptr )
	{
		if( ptr->rleft )
		{
			parent->rright = ptr->rleft;
			ptr2 = ptr->rleft;
			while( ptr2->rright )
				ptr2 = ptr2->rright;
			ptr2->rright = ptr->rright;
		}
		else
			parent->rright = ptr->rright;
	}
	else if( parent->rleft == ptr )
	{
		if( ptr->rright )
		{
			parent->rleft = ptr->rright;
			ptr2 = ptr->rright;
			while( ptr2->rleft )
				ptr2 = ptr2->rleft;
			ptr2->rleft = ptr->rleft;
		}
		else
			parent->rleft = ptr->rleft;
	}
}

void
Insert_name( root , name )
struct name_tree **root;
char *name;
{
	struct name_tree *ptr;
	struct name_tree *new_ptr;
	int found;
	int diff;

	ptr = Search_names( *root , name , &found );

	if( found )
	{
		rt_log( "Insert_name: %s already in name tree\n" , name );
		return;
	}

	new_ptr = (struct name_tree *)rt_malloc( sizeof( struct name_tree ) , "Insert_name: new_ptr" );

	strncpy( new_ptr->name , name , NAMESIZE+1 );
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->region_id = (-region_id);
	new_ptr->in_comp_group = 0;
	new_ptr->element_id = 0;

	if( !*root )
	{
		*root = new_ptr;
		return;
	}

	diff = strncmp( name , ptr->name , NAMESIZE );
	if( diff > 0 )
	{
		if( ptr->nright )
		{
			rt_log( "Insert_name: ptr->nright not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nright = new_ptr;
	}
	else
	{
		if( ptr->nleft )
		{
			rt_log( "Insert_name: ptr->nleft not null\n" );
			rt_bomb( "\tCannot insert new node\n" );
		}
		ptr->nleft = new_ptr;
	}
}

void
Insert_region_name( name , reg_id , el_id )
char *name;
int reg_id;
int el_id;
{
	struct name_tree *nptr_model,*rptr_model;
	struct name_tree *new_ptr;
	int foundn,foundr;
	int diff;

	if( debug )
		rt_log( "Insert_region_name( name=%s, reg_id=%d, el_id=%d\n" , name, reg_id, el_id );

	rptr_model = Search_ident( name_root , reg_id , el_id , &foundr );
	nptr_model = Search_names( name_root , name , &foundn );

	if( foundn && foundr )
		return;

	if( foundn != foundr )
	{
		rt_log( "Insert_region_name: name %s ident %d element %d\n\tfound name is %d\n\tfound ident is %d\n",
			name, reg_id, el_id, foundn, foundr );
		List_names();
		rt_bomb( "\tCannot insert new node\n" );
	}

	/* Add to tree for entire model */
	new_ptr = (struct name_tree *)rt_malloc( sizeof( struct name_tree ) , "Insert_region_name: new_ptr" );
	new_ptr->rleft = (struct name_tree *)NULL;
	new_ptr->rright = (struct name_tree *)NULL;
	new_ptr->nleft = (struct name_tree *)NULL;
	new_ptr->nright = (struct name_tree *)NULL;
	new_ptr->region_id = reg_id;
	new_ptr->element_id = el_id;
	new_ptr->in_comp_group = 0;
	strncpy( new_ptr->name , name , NAMESIZE+1 );

	if( !name_root )
		name_root = new_ptr;
	else
	{
		diff = strncmp( name , nptr_model->name , NAMESIZE );

		if( diff > 0 )
		{
			if( nptr_model->nright )
			{
				rt_log( "Insert_region_name: nptr_model->nright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_model->nright = new_ptr;
		}
		else
		{
			if( nptr_model->nleft )
			{
				rt_log( "Insert_region_name: nptr_model->nleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			nptr_model->nleft = new_ptr;
		}


		diff = reg_id - rptr_model->region_id;

		if( diff == 0 )
			diff = el_id - rptr_model->element_id;

		if( diff > 0 )
		{
			if( rptr_model->rright )
			{
				rt_log( "Insert_region_name: rptr_model->rright not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rright = new_ptr;
		}
		else
		{
			if( rptr_model->rleft )
			{
				rt_log( "Insert_region_name: rptr_model->rleft not null\n" );
				rt_bomb( "\tCannot insert new node\n" );
			}
			rptr_model->rleft = new_ptr;
		}
	}
}

char *
find_region_name( g_id , c_id , el_id )
int g_id;
int c_id;
int el_id;
{
	struct name_tree *ptr;
	int reg_id;
	int found;

	reg_id = g_id * 1000 + c_id;

	if( debug )
		rt_log( "find_region_name( g_id=%d, c_id=%d, el_id=%d ), reg_id=%d\n" , g_id, c_id, el_id, reg_id );

	ptr = Search_ident( name_root , reg_id , el_id , &found );

	if( found )
		return( ptr->name );
	else
		return( (char *)NULL );
}

void
make_unique_name( name )
char *name;
{
	struct name_tree *ptr;
	char append[10];
	int append_len;
	int len;
	int found;

	/* make a unique name from what we got off the $NAME card */

	len = strlen( name );

	ptr = Search_names( name_root , name , &found );
	while( found )
	{
		sprintf( append , "_%d" , name_count );
		name_count++;
		append_len = strlen( append );

		if( len + append_len <= NAMESIZE )
			strcat( name , append );
		else
			strcpy( &name[NAMESIZE-append_len] , append );

		ptr = Search_names( name_root , name , &found );
	}
}

void
add_to_series( name , reg_id )
char *name;
int reg_id;
{
	int group_no;

	group_no = reg_id / 1000;

	if( group_id < 0 || group_id > 10 )
	{
		rt_log( "add_to_series: region (%s) not added, illegal group number %d, region_id=$d\n" ,
			name , group_id , reg_id );
		return;
	}

	if( mk_addmember( name , &group_head[group_id] , WMOP_UNION ) == (struct wmember *)NULL )
		rt_log( "add_to_series: mk_addmember failed for region %s\n" , name );
}

void
make_comp_group()
{
	struct wmember g_head;
	struct name_tree *ptr;
	char name[NAMESIZE+1];

	RT_LIST_INIT( &g_head.l );

	nmg_tbl( &stack , TBL_RST , (long *)NULL );

	ptr = name_root;
	while( 1 )
	{
		while( ptr )
		{
			PUSH( ptr );
			ptr = ptr->nleft;
		}
		POP( name_tree , ptr );
		if( !ptr )
			break;

		if(ptr->region_id == region_id && ptr->element_id && !ptr->in_comp_group )
		{
			if( mk_addmember( ptr->name , &g_head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				rt_log( "make_comp_group: Could not add %s to group for ident %d\n" , ptr->name , ptr->region_id );
				break;
			}
			ptr->in_comp_group = 1;
		}
		ptr = ptr->nright;
	}

	if( RT_LIST_NON_EMPTY( &g_head.l ) )
	{
		char *tmp_name;

		if( (tmp_name=find_region_name( group_id , comp_id , 0 )) )
			strcpy( name , tmp_name );
		else
		{
			sprintf( name , "comp_%d" , region_id );
			make_unique_name( name );
			if( warnings )
				rt_log( "Creating default name (%s) for group %d component %d\n",
						name , group_id , comp_id );
			Insert_name( &name_root , name );
		}

		mk_lfcomb( fdout , name , &g_head , 0 );
		add_to_series( name , region_id );
	}
}

void
do_groups()
{
	int group_no;
	struct wmember head_all;

	if( debug )
		rt_log( "do_groups\n" );

	RT_LIST_INIT( &head_all.l );

	for( group_no=0 ; group_no < 11 ; group_no++ )
	{
		char name[NAMESIZE+1];

		if( RT_LIST_IS_EMPTY( &group_head[group_no].l ) )
			continue;

		sprintf( name , "%dxxx_series" , group_no );
		mk_lfcomb( fdout , name , &group_head[group_no] , 0 );

		if( mk_addmember( name , &head_all , WMOP_UNION ) == (struct wmember *)NULL )
			rt_log( "do_groups: mk_addmember failed to add %s to group all\n" , name );
	}

	if( RT_LIST_NON_EMPTY( &head_all.l ) )
		mk_lfcomb( fdout , "all" , &head_all , 0 );
}

void
do_name()
{
	int i,j;
	int g_id;
	int c_id;
	int len;
	char comp_name[25];
	char tmp_name[25];

	if( pass )
		return;

	if( debug )
		rt_log( "do_name: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	g_id = atoi( field );

	if( g_id != group_id )
	{
		rt_log( "$NAME card for group %d in section for group %d ignored\n" , g_id , group_id );
		rt_log( "%s\n" , line );
		return;
	}

	strncpy( field , &line[16] , 8 );
	c_id = atoi( field );

	if( c_id != comp_id )
	{
		rt_log( "$NAME card for component %d in section for component %d ignored\n" , c_id , comp_id );
		rt_log( "%s\n" , line );
		return;
	}

	strncpy( comp_name , &line[56] , 24 );

	/* eliminate trailing blanks */
	i = 26;
	while(  --i >= 0 && isspace( comp_name[i] ) )
		comp_name[i] = '\0';

	/* copy comp_name to tmp_name while replacing white space with "_" */
	i = (-1);
	j = (-1);

	/* copy */
	while( comp_name[++i] != '\0' )
	{
		if( isspace( comp_name[i] ) || comp_name[i] == '/' )
		{
			if( j == (-1) || tmp_name[j] != '_' )
				tmp_name[++j] = '_';
		}
		else
			tmp_name[++j] = comp_name[i];
	}
	tmp_name[++j] = '\0';

	len = strlen( tmp_name );
	if( len <= NAMESIZE )
		strncpy( name_name , tmp_name , NAMESIZE+1 );
	else
		strncpy( name_name , &tmp_name[len-NAMESIZE] , NAMESIZE+1 );

	/* reserve this name for group name */
	make_unique_name( name_name );
	Insert_region_name( name_name , region_id , 0 );

	name_count = 0;
}

void
make_region_name( name , g_id , c_id , element_id , type )
char *name;
int g_id;
int c_id;
int element_id;
char type;
{
	int r_id;
	int found;
	char *tmp_name;
	struct name_tree *ptr;

	r_id = g_id * 1000 + c_id;

	if( debug )
		rt_log( "make_region_name( g_id=%d, c_id=%d, element_id=%d, type=%c )\n" , g_id, c_id, element_id, type );

	tmp_name = find_region_name( g_id , c_id , element_id );
	if( tmp_name )
	{
		strncpy( name , tmp_name , NAMESIZE+1 );
		return;
	}

	/* create a new name */
	if( name_name[0] )
		strncpy( name , name_name , NAMESIZE+1 );
	else if( element_id < 0 && type == CLINE )
		sprintf( name , "%d.j.%d.r" , r_id , joint_no++ );
	else
		sprintf( name , "%d.%d.%c.r" , r_id , element_id , type );

	make_unique_name( name );

	Insert_region_name( name , r_id , element_id );
}

void
get_solid_name( name , type , element_id , c_id , g_id , inner )
char *name;
char type;
int element_id;
int c_id;
int g_id;
int inner;
{
	int reg_id;

	reg_id = g_id * 1000 + c_id;

	sprintf( name , "%d.%d.%c%d" , reg_id , element_id , type , inner );
}

void
make_solid_name( name , type , element_id , c_id , g_id , inner )
char *name;
char type;
int element_id;
int c_id;
int g_id;
int inner;
{
	get_solid_name( name , type , element_id , c_id , g_id , inner );

	Insert_name( &name_root , name );
}

void
do_grid()
{
	int grid_no;
	fastf_t x,y,z;

	if( !pass )	/* not doing geometry yet */
		return;

	strncpy( field , &line[8] , 8 );
	grid_no = atoi( field );

	if( grid_no < 1 )
	{
		rt_log( "ERROR: grid id number = %d\n" , grid_no );
		rt_bomb( "BAD GRID ID NUMBER\n" );
	}

	strncpy( field , &line[24] , 8 );
	x = atof( field );

	strncpy( field , &line[32] , 8 );
	y = atof( field );

	strncpy( field , &line[40] , 8 );
	z = atof( field );

	while( grid_no > grid_size - 1 )
	{
		grid_size += GRID_BLOCK;
		grid_pts = (struct fast_verts *)rt_realloc( (char *)grid_pts , grid_size * sizeof( struct fast_verts ) , "fast4-g: grid_pts" );
	}

	VSET( grid_pts[grid_no].pt , x*25.4 , y*25.4 , z*25.4 );
	grid_pts[grid_no].v = (struct vertex *)NULL;

	if( grid_no > max_grid_no )
		max_grid_no = grid_no;
}

void
make_cline_regions()
{
	struct cline *cline_ptr;
	struct nmg_ptbl points;
	struct wmember head;
	char name[NAMESIZE+1];
	int sph_no;

	if( debug )
		rt_log( "make_cline_regions\n" );

	RT_LIST_INIT( &head.l );
	nmg_tbl( &points , TBL_INIT , (long *)NULL );

	/* make a list of all the endpoints */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		/* Add endpoints to points list */
		nmg_tbl( &points , TBL_INS_UNIQUE , (long *)cline_ptr->pt1 );
		nmg_tbl( &points , TBL_INS_UNIQUE , (long *)cline_ptr->pt2 );
		
		cline_ptr = cline_ptr->next;
	}

	/* Now build cline objects */
	for( sph_no=0 ; sph_no < NMG_TBL_END( &points ) ; sph_no++ )
	{
		int pt_no;
		int line_no;
		struct nmg_ptbl lines;
		fastf_t sph_radius=0.0;
		fastf_t sph_inner_radius=0.0;

		nmg_tbl( &lines , TBL_INIT , (long *)NULL );

		pt_no = (int)NMG_TBL_GET( &points , sph_no );

		/* get list of clines that touch this point */
		cline_ptr = cline_root;
		while( cline_ptr )
		{
			if( cline_ptr->pt1 == pt_no || cline_ptr->pt2 == pt_no )
			{
				/* get outer radius for sphere */
				if( cline_ptr->radius > sph_radius )
					sph_radius = cline_ptr->radius;

				/* get inner radius for sphere */
				if( cline_ptr->thick > 0.0 &&
				    (cline_ptr->radius - cline_ptr->thick > sph_inner_radius ) )
					sph_inner_radius =  cline_ptr->radius - cline_ptr->thick;

				nmg_tbl( &lines , TBL_INS , (long *)cline_ptr );
			}

			cline_ptr = cline_ptr->next;
		}

		if( NMG_TBL_END( &lines ) > 1 )
		{
			/* make a joint where CLINE's meet */

			/* make sphere solid at cline joint */
			sprintf( name , "%d.%d.j0" , region_id , pt_no );
			Insert_name( &name_root , name );
			mk_sph( fdout , name , grid_pts[pt_no].pt , sph_radius );

			/* Union sphere */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				rt_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
				nmg_tbl( &lines , TBL_FREE , (long *)NULL );
				break;
			}

			/* subtract inner sphere if required */
			if( sph_inner_radius > 0.0 && sph_inner_radius < sph_radius )
			{
				sprintf( name , "%d.%d.j1" , region_id , pt_no );
				Insert_name( &name_root , name );
				mk_sph( fdout , name , grid_pts[pt_no].pt , sph_inner_radius );

				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for outer sphere %s\n" , name );
					nmg_tbl( &lines , TBL_FREE , (long *)NULL );
					break;
				}
			}

			/* subtract all outer cylinders that touch this point */
			for( line_no=0 ; line_no < NMG_TBL_END( &lines ) ; line_no++ )
			{
				cline_ptr = (struct cline *)NMG_TBL_GET( &lines , line_no );

				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for line at joint %s\n" , name );
					nmg_tbl( &lines , TBL_FREE , (long *)NULL );
					break;
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* now make the region */
			if( RT_LIST_NON_EMPTY( &head.l ) )
				MK_REGION( fdout , &head , group_id , comp_id , -pt_no , CLINE )
		}
		else
		{
			char *bad_name;

			/* no need for a region here
			 * but we need to remove this region from the name tree
			 */
			bad_name = find_region_name( group_id , comp_id , -pt_no );
			if( bad_name )
				Delete_name( name_root , bad_name );
		}

		/* make regions for all CLINE elements that start at this pt_no */
		for( line_no=0 ; line_no < NMG_TBL_END( &lines ) ; line_no++ )
		{
			int i;
			cline_ptr = (struct cline *)NMG_TBL_GET( &lines , line_no ) ;
			if( cline_ptr->pt1 != pt_no )
				continue;

			/* make name for outer cylinder */
			get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 0 );

			/* start region */
			if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
			{
				rt_log( "make_cline_regions: mk_addmember failed for outer solid %s\n" , name );
				break;
			}
			cline_ptr->made = 1;

			if( cline_ptr->thick > 0.0 )
			{
				/* subtract inside cylinder */
				get_solid_name( name , CLINE , cline_ptr->element_id , comp_id , group_id , 1 );
				if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				{
					rt_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
					break;
				}
			}

			/* subtract outside cylinders of any other clines that use this grid point */
			for( i=0 ; i < NMG_TBL_END( &lines ) ; i++ )
			{
				struct cline *ptr2;

				ptr2 = (struct cline *)NMG_TBL_GET( &lines , i );

				if( ptr2 != cline_ptr &&
				    ptr2->pt1 == pt_no || ptr2->pt2 == pt_no )
				{
					get_solid_name( name , CLINE , ptr2->element_id , comp_id , group_id , 0 );
					if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
					{
						rt_log( "make_cline_regions: mk_addmember failed for inner solid %s\n" , name );
						break;
					}
				}
			}

			/* subtract any holes for this component */
			Subtract_holes( &head , comp_id , group_id );

			/* make the region */
			MK_REGION( fdout , &head , group_id , comp_id , cline_ptr->element_id , CLINE );
		}
		nmg_tbl( &lines , TBL_FREE , (long *)NULL );
	}

	nmg_tbl( &points , TBL_FREE , (long *)NULL );

	/* free the linked list of cline pointers */
	cline_ptr = cline_root;
	while( cline_ptr )
	{
		struct cline *ptr;

		ptr = cline_ptr;
		cline_ptr = cline_ptr->next;

		rt_free( (char *)ptr , "make_cline_regions: cline_ptr" );
	}

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

}

void
do_sphere()
{
	int element_id;
	int center_pt;
	fastf_t thick;
	fastf_t radius;
	fastf_t inner_radius;
	char name[NAMESIZE+1];
	struct wmember sphere_region;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CSPHERE );
		return;
	}

	strncpy( field , &line[24] , 8 );
	center_pt = atoi( field );

	strncpy( field , &line[56] , 8 );
	thick = atof( field );

	strncpy( field , &line[64] , 8 );
	radius = atof( field );
	if( radius <= 0.0 )
	{
		rt_log( "do_sphere: illegal radius (%f), skipping sphere\n" , radius );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( center_pt < 1 || center_pt > max_grid_no )
	{
		rt_log( "do_sphere: illegal grid number for center point %d, skipping sphere\n" , center_pt );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	RT_LIST_INIT( &sphere_region.l );

	make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 0 );
	mk_sph( fdout , name , grid_pts[center_pt].pt , radius );

	if( mk_addmember( name ,  &sphere_region , WMOP_UNION ) == (struct wmember *)NULL )
	{
		rt_log( "do_sphere: Error in adding %s to sphere region\n" , name );
		rt_bomb( "do_sphere" );
	}

	inner_radius = radius - thick;
	if( thick > 0.0 && inner_radius <= 0.0 )
	{
		rt_log( "do_sphere: illegal thickness (%f), skipping inner sphere\n" , thick );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	make_solid_name( name , CSPHERE , element_id , comp_id , group_id , 1 );
	mk_sph( fdout , name , grid_pts[center_pt].pt , inner_radius );

	if( mk_addmember( name , &sphere_region , WMOP_SUBTRACT ) == (struct wmember *)NULL )
	{
		rt_log( "do_sphere: Error in subtracting %s from sphere region\n" , name );
		rt_bomb( "do_sphere" );
	}

	/* subtract any holes for this component */
	Subtract_holes( &sphere_region , comp_id , group_id );

	MK_REGION( fdout , &sphere_region , group_id , comp_id , element_id , CSPHERE )
}

void
make_nmg_name( name , r_id )
char *name;
int r_id;
{
	sprintf( name , "nmg.%d" , r_id );

	make_unique_name( name );
}

void
Check_normals()
{
	/* XXXX This routine is far from complete */
	struct nmgregion *r1;
	struct nmg_ptbl verts;
	struct shell *s1;

	if( debug )
		rt_log( "Check_normals\n" );
	
	nmg_tbl( &verts , TBL_INIT , (long *)NULL );

	NMG_CK_SHELL( s );

	nmg_decompose_shell( s , &tol );

	/* Calculate center of shell */
	for( RT_LIST_FOR( s1 , shell , &r->s_hd ) )
	{
		struct vertex *v;
		point_t shell_center;
		struct faceuse *fu;
		int i;

		NMG_CK_SHELL( s1 );

		VSET( shell_center , 0.0 , 0.0 , 0.0 );

		nmg_vertex_tabulate( &verts , &s1->l.magic );

		for( i=0 ; i<NMG_TBL_END( &verts ) ; i++ )
		{
			v = (struct vertex *)NMG_TBL_GET( &verts , i );
			VADD2( shell_center , shell_center , v->vg_p->coord );
		}

		VSCALE( shell_center , shell_center , (fastf_t)(NMG_TBL_END( &verts ) ) );

		nmg_tbl( &verts , TBL_RST , (long *)NULL );

		/* check if outward normal points away from center */
		for( RT_LIST_FOR( fu , faceuse , &s1->fu_hd ) )
		{
			vect_t norm;
			vect_t to_center;
			struct loopuse *lu;
			struct edgeuse *eu;
			fastf_t dot;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			NMG_GET_FU_NORMAL( norm , fu );
			lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
			NMG_CK_LOOPUSE( lu );
			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			NMG_CK_EDGEUSE( eu );

			v = eu->vu_p->v_p;

			NMG_CK_VERTEX( v );

			VSUB2( to_center , shell_center , v->vg_p->coord )

			dot = VDOT( to_center , norm );
			if( RT_VECT_ARE_PERP( dot , &tol ) )
			{
				/* shell center is on face, probably only one face */
				break;
			}

			if( dot > 0.0 )
			{
				/* outward normal points inward */
				nmg_invert_shell( s1 , &tol );
				break;
			}
		}
	}

	s1 = RT_LIST_FIRST( shell , &r->s_hd );
	while( RT_LIST_NOT_HEAD( s1 , &r->s_hd ) )
	{
		struct shell *next_s;

		next_s = RT_LIST_PNEXT( shell , &s1->l );

		if( s1 == s )
		{
			s1 = next_s;
			continue;
		}

		nmg_js( s , s1 , &tol );

		s1 = next_s;
	}

	nmg_tbl( &verts , TBL_FREE , (long *)NULL );
}

int
Sort_fus_by_thickness()
{
	struct fast_fus *fus,*tmp,*prev;
	fastf_t thick;
	int done;
	int num_thicks=0;

	done=0;
	while( !done )
	{
		done = 1;
		prev = NULL;
		fus = fus_root;
		while( fus->next )
		{
			if( fus->thick > fus->next->thick )
			{
				done = 0;
				tmp = fus->next->next;
				if( fus == fus_root )
				{
					fus_root = fus->next;
					fus_root->next = fus;
					fus->next = tmp;
					prev = fus_root;
				}
				else
				{
					prev->next = fus->next;
					prev->next->next = fus;
					fus->next = tmp;
					prev = prev->next;
				}
			}
			else
			{
				prev = fus;
				fus = fus->next;
			}
		}
	}

	fus = fus_root;
	thick = fus->thick;
	num_thicks = 1;
	while( fus )
	{
		if( thick != fus->thick )
		{
			thick = fus->thick;
			num_thicks++;
		}
		fus = fus->next;
	}

	return( num_thicks );
}

void
Recalc_edge_g( new_s )
struct shell * new_s;
{
	int i;
	struct nmg_ptbl edges;
	int *flags;

	nmg_tbl( &edges , TBL_INIT , (long *)NULL );

	flags = (int *)rt_calloc( m->maxindex , sizeof( int ) , "Recalc_edge_g: flags" );

	nmg_edge_tabulate( &edges , &new_s->l.magic );

	for( i=0 ; i<NMG_TBL_END( &edges ) ; i++ )
	{
		struct edge *e;
		struct edge_g *eg;

		e = (struct edge *)NMG_TBL_GET( &edges , i );
		NMG_CK_EDGE( e );

		eg = e->eg_p;
		NMG_CK_EDGE_G( eg );

		if( NMG_INDEX_TEST_AND_SET( flags , eg ) )
			nmg_edge_g( e );
	}

	rt_free( (char *)flags , "Recalc_edge_g: flags" );

	nmg_tbl( &edges , TBL_FREE , (long *)NULL );
}

void
Recalc_face_g( new_s )
struct shell *new_s;
{
	struct faceuse *fu;
	int *flags;

	flags = (int *)rt_calloc( m->maxindex , sizeof( int ) , "Recalc_face_g: flags " );

	for( RT_LIST_FOR( fu , faceuse , &new_s->fu_hd ) )
	{
		struct face *f;
		struct face_g *fg;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		f = fu->f_p;
		NMG_CK_FACE( f );
		fg = f->fg_p;
		NMG_CK_FACE_G( fg );

		if( NMG_INDEX_TEST_AND_SET( flags , fg ) )
			(void)nmg_calc_face_g( fu );
	}

	rt_free( (char *)flags , "Recalc_face_g: flags" );
}

int
Adjust_vertices( new_s , thick )
struct shell *new_s;
CONST fastf_t thick;
{
	struct faceuse *fu;
	struct nmg_ptbl verts;
	long *flags;
	int vert_no;
	int failures=0;

	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "Adjust_vertices: flags" );

	/* now adjust all the planes, first move them by distance "thick" */
	for( RT_LIST_FOR( fu , faceuse , &new_s->fu_hd ) )
	{
		struct face_g *fg_p;

		NMG_CK_FACEUSE( fu );
		NMG_CK_FACE( fu->f_p );
		fg_p = fu->f_p->fg_p;
		NMG_CK_FACE_G( fg_p );

		/* move the faces by the distance "thick" */
		if( NMG_INDEX_TEST_AND_SET( flags , fg_p ) )
		{
			if( debug )
			{
				rt_log( "Moving face_g x%x ( %f %f %f %f ) from face x%x\n" ,
					fu->f_p->fg_p, V4ARGS( fu->f_p->fg_p->N ) , fu->f_p );
				rt_log( "\tflip = %d\n" , fu->f_p->flip );
			}
			if( fu->f_p->flip )
				fg_p->N[3] -= thick;
			else
				fg_p->N[3] += thick;
		}
	}

	rt_free( (char *)flags , "Adjust_vertices: flags" );

	/* get table of vertices in this shell */
	nmg_vertex_tabulate( &verts , &new_s->l.magic );

	/* now move all the vertices */
	for( vert_no = 0 ; vert_no < NMG_TBL_END( &verts ) ; vert_no++ )
	{
		struct vertex *new_v;

		new_v = (struct vertex *)NMG_TBL_GET( &verts , vert_no );
		NMG_CK_VERTEX( new_v );

		if( nmg_in_vert( new_v , 1 , &tol ) )
		{
			rt_log( "Adjust_vertices: Failed to calculate new vertex at v=x%x was ( %f %f %f )\n",
				new_v , V3ARGS( new_v->vg_p->coord ) );
			rt_log( "\tgroup id %d, component id %d\n" , group_id , comp_id );
			failures++;
		}
	}

	nmg_tbl( &verts , TBL_FREE , (long *)NULL );

	/* since we used approximate mode in nmg_in_vert, recalculate planes */
	Recalc_face_g( new_s );

	/* and recalculate edge geometry */
	Recalc_edge_g( new_s );

	return( failures );
}

struct edgeuse *
Use_of_eu_outside_shell( eu )
CONST struct edgeuse *eu;
{
	struct edgeuse *eu_radial;
	struct shell *s_eu;

	NMG_CK_EDGEUSE( eu );

	s_eu = nmg_find_s_of_eu( eu );

	eu_radial = nmg_next_radial_eu( eu , (struct shell *)NULL , 0 );
	while( eu_radial != eu )
	{
		if( nmg_find_s_of_eu( eu_radial ) != s_eu )
			return( eu_radial );
		eu_radial = nmg_next_radial_eu( eu_radial , (struct shell *)NULL , 0 );
	}

	return( (struct edgeuse *)NULL );
}

int
vu_used_outside_shell( vu )
CONST struct vertexuse *vu;
{
	struct vertexuse *vu1;
	struct shell *s_vu;

	NMG_CK_VERTEXUSE( vu );

	s_vu = nmg_find_s_of_vu( vu );

	for( RT_LIST_FOR( vu1 , vertexuse , &vu->v_p->vu_hd ) )
	{
		if( nmg_find_s_of_vu( vu1 ) != s_vu )
			return( 1 );
	}

	return( 0 );
}

void
Move_vus_in_s_to_newv( vu , new_s , new_v )
struct vertexuse *vu;
CONST struct shell *new_s;
struct vertex *new_v;
{
	struct vertex *old_v;
	struct vertexuse *tmp_vu;

	NMG_CK_VERTEXUSE( vu );
	NMG_CK_SHELL( new_s );
	NMG_CK_VERTEX( new_v );

	old_v = vu->v_p;
	tmp_vu = RT_LIST_FIRST( vertexuse , &old_v->vu_hd );
	while( RT_LIST_NOT_HEAD( tmp_vu , &old_v->vu_hd ) )
	{
		struct vertexuse *next_vu;

		next_vu = RT_LIST_PNEXT( vertexuse , tmp_vu );
		if( nmg_find_s_of_vu( tmp_vu ) != new_s )
			nmg_movevu( tmp_vu , new_v );

		tmp_vu = next_vu;
	}
}

void
Glue_shell_faces( new_s )
struct shell *new_s;
{
	struct nmg_ptbl faces;
	struct faceuse *fu;

	NMG_CK_SHELL( new_s );

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	for( RT_LIST_FOR( fu , faceuse , &new_s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		nmg_tbl( &faces , TBL_INS , (long *)fu );
	}
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

	nmg_tbl( &faces , TBL_FREE , (long *)NULL );
}

struct shell *
Get_shell( thick , center )
CONST fastf_t thick;
CONST int center;
{
	struct fast_fus *fus;
	struct shell *new_s;
	struct faceuse *fu;

	new_s = nmg_msv( r );

	fus = fus_root;
	while( fus )
	{
		NMG_CK_FACEUSE( fus->fu );
		if( fus->thick == thick && fus->pos == center )
			nmg_mv_fu_between_shells( new_s , s , fus->fu );

		fus = fus->next;
	}

	if( RT_LIST_IS_EMPTY( &new_s->fu_hd ) )
	{
		nmg_ks( new_s );
		new_s = (struct shell *)NULL;
	}
	else
	{
		nmg_kvu( new_s->vu_p );

		/* disconnect faceuses from those not in this shell */
		for( RT_LIST_FOR( fu , faceuse , &new_s->fu_hd ) )
		{
			struct loopuse *lu;

			if( fu->orientation != OT_SAME )
				continue;

			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				struct edgeuse *eu;

				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					struct edgeuse *eu_radial;
					struct vertex *v1,*v2;
					struct edgeuse *new_eu;
					struct adjacent_faces *adj;
					struct faceuse *fu2;

					/* Check if this edge has a use outside this shell */
					if( (eu_radial = Use_of_eu_outside_shell( eu )) ==
							(struct edgeuse *)NULL )
								continue;

					/* Two adjacent faces here will be seperated
					 * put them on the list to be reunited later
					 */
					if( adj_root == (struct adjacent_faces *)NULL )
					{
						adj_root = (struct adjacent_faces *)rt_malloc(
							sizeof( struct adjacent_faces ),
							"Get_shell: adj_root" );
						adj = adj_root;
					}
					else
					{
						adj = adj_root;
						while( adj->next )
							adj = adj->next;
						adj->next = (struct adjacent_faces *)rt_malloc(
							sizeof( struct adjacent_faces ),
							"Get_shell: adj_root" );
						adj = adj->next;
					}
					adj->next = (struct adjacent_faces *)NULL;
					adj->fu1 = fu;
					adj->eu1 = eu;

					fu2 = nmg_find_fu_of_eu( eu_radial );
					if( fu2->orientation != OT_SAME )
					{
						fu2 = fu2->fumate_p;
						eu_radial = eu_radial->eumate_p;
					}
					adj->fu2 = fu2;
					adj->eu2 = eu_radial;

					/* unglue the edge */
					nmg_unglueedge( eu );

					/* make a new edge */
					v1 = (struct vertex *)NULL;
					v2 = (struct vertex *)NULL;
					new_eu = nmg_me( v1 , v2 , new_s );

					/* assign same geometry to this new edge */
					nmg_vertex_gv( new_eu->vu_p->v_p ,
						eu->vu_p->v_p->vg_p->coord );
					nmg_vertex_gv( new_eu->eumate_p->vu_p->v_p ,
						eu->eumate_p->vu_p->v_p->vg_p->coord );

					/* move the original edge vu's to the new vertices */
					/* Also move any other vu's of these
					 * vertices in new_s to the new shell
					 */
					if( vu_used_outside_shell( eu->vu_p ) )
						Move_vus_in_s_to_newv( eu->vu_p , new_s , new_eu->vu_p->v_p );

					if( vu_used_outside_shell( eu->eumate_p->vu_p ) )
						Move_vus_in_s_to_newv( eu->eumate_p->vu_p , new_s ,
								new_eu->eumate_p->vu_p->v_p );

					/* kill the new edge,
					 * I only wanted it for its vertices.
					 */
					nmg_keu( new_eu );
					break;
				}
			}
		}
		Glue_shell_faces( new_s );
	}

	return( new_s );
}

void
Build_connecting_face( eu1 , eu2 , s1 )
struct edgeuse *eu1,*eu2;
struct shell *s1;
{
	struct vertex *verts[3];

	NMG_CK_EDGEUSE( eu1 );
	NMG_CK_EDGEUSE( eu2 );
	NMG_CK_SHELL( s1 );

	/* need to build a connecting face between these two */
	if( eu1->vu_p->v_p != eu2->eumate_p->vu_p->v_p &&
		eu1->eumate_p->vu_p->v_p != eu2->eumate_p->vu_p->v_p )
	{
		struct faceuse *new_fu;
		struct loopuse *lu;
		fastf_t area;
		plane_t pl;

		verts[0] = eu1->eumate_p->vu_p->v_p;
		verts[1] = eu1->vu_p->v_p;
		verts[2] = eu2->eumate_p->vu_p->v_p;

		new_fu = nmg_cface( s1 , verts , 3 );
		lu = RT_LIST_FIRST( loopuse , &new_fu->lu_hd );
		area = nmg_loop_plane_area( lu , pl );
		if( area < 0.0 )
		{
			rt_log( "Cannot calculate plane equation for new face: \n" );
			rt_log( "\t( %f %f %f ), ( %f %f %f ), ( %f %f %f )\n",
				V3ARGS( verts[0]->vg_p->coord ),
				V3ARGS( verts[1]->vg_p->coord ),
				V3ARGS( verts[2]->vg_p->coord ) );
			nmg_kfu( new_fu );
		}
		else
		{
			nmg_face_g( new_fu , pl );
			nmg_loop_g( lu->l_p , &tol );
		}
	}

	if( eu1->eumate_p->vu_p->v_p != eu2->vu_p->v_p &&
		eu1->eumate_p->vu_p->v_p != eu2->eumate_p->vu_p->v_p )
	{
		struct faceuse *new_fu;
		struct loopuse *lu;
		fastf_t area;
		plane_t pl;

		verts[0] = eu1->eumate_p->vu_p->v_p;
		verts[1] = eu2->eumate_p->vu_p->v_p;
		verts[2] = eu2->vu_p->v_p;

		new_fu = nmg_cface( s1 , verts , 3 );
		lu = RT_LIST_FIRST( loopuse , &new_fu->lu_hd );
		area = nmg_loop_plane_area( lu , pl );
		if( area < 0.0 )
		{
			rt_log( "Cannot calculate plane equation for new face: \n" );
			rt_log( "\t( %f %f %f ), ( %f %f %f ), ( %f %f %f )\n",
				V3ARGS( verts[0]->vg_p->coord ),
				V3ARGS( verts[1]->vg_p->coord ),
				V3ARGS( verts[2]->vg_p->coord ) );
			nmg_kfu( new_fu );
		}
		else
		{
			nmg_face_g( new_fu , pl );
			nmg_loop_g( lu->l_p , &tol );
		}
	}

}

void
Reunite_faces( tbl )
CONST long **tbl;
{
	struct adjacent_faces *adj;
	struct vertex *verts[3];
	struct shell *s_eu1;

	/* maybe ther's nothing to do */
	if( adj_root == (struct adjacent_faces *)NULL )
		return;

	/* rebound the model again */
	nmg_rebound( m , &tol );

	nmg_model_fuse( m , &tol );

	adj = adj_root;
	while( adj )
	{
		struct edgeuse *eu1,*eu2;
		struct faceuse *fu1,*fu2;

		/* if the two faceuses are radial, nothing to do */
		if( !nmg_faces_are_radial( adj->fu1 , adj->fu2 ) )
		{

			eu1 = adj->eu1;
			eu2 = adj->eu2;
			NMG_CK_EDGEUSE( eu1 );
			NMG_CK_EDGEUSE( eu2 );

			s_eu1 = nmg_find_s_of_eu( eu1 );

			Build_connecting_face( eu1 , eu2 , s_eu1 );
		}

		fu1 = NMG_INDEX_GETP( faceuse , tbl , adj->fu1 );
		fu2 = NMG_INDEX_GETP( faceuse , tbl , adj->fu2 );
		if( !nmg_faces_are_radial( fu1 , fu2 ) )
		{
			NMG_CK_FACEUSE( fu1 );
			NMG_CK_FACEUSE( fu2 );

			eu1 = NMG_INDEX_GETP( edgeuse , tbl , adj->eu1 );
			eu2 = NMG_INDEX_GETP( edgeuse , tbl , adj->eu2 );
			NMG_CK_EDGEUSE( eu1 );
			NMG_CK_EDGEUSE( eu2 );

			s_eu1 = nmg_find_s_of_eu( eu1 );

			Build_connecting_face( eu1 , eu2 , s_eu1 );
		}

		adj = adj->next;
	}
}

int
Extrude_faces()
{
	struct shell *s1,*s2;
	struct shell **shells,**dup_shells;
	struct fast_fus *fus;
	fastf_t *thicks;
	int i;
	int num_thicks=0;
	int thick_no;
	int center;
	int table_size;
	long **dup_tbl;

	if( debug )
		rt_log( "Extrude_faces:\n" );

/*	Check_normals();	*/

	num_thicks = Sort_fus_by_thickness();

	thicks = (fastf_t *)rt_calloc( num_thicks , sizeof( fastf_t ) , "Extrude_faces: thicks" );
	i = 0;
	fus = fus_root;
	thicks[i] = fus->thick;
	while( fus )
	{
		if( fus->thick != thicks[i] )
		{
			if( ++i >= num_thicks )
				rt_bomb( "Extrude_faces: Wrong number of shell thicknesses\n" );
			thicks[i] = fus->thick;
		}
		fus = fus->next;
	}

	/* rebound the region */
	nmg_rebound( m , &tol );

	/* make array of shells, each with unique combination of "center postion" and thickness */
	shells = (struct shell **)rt_calloc( num_thicks*2 , sizeof( struct shell *) , "Extrude_faces: shells" );

	/* and another for the opposite (extruded) side */
	dup_shells = (struct shell **)rt_calloc( num_thicks*2 , sizeof( struct shell *) , "Extrude_faces: dup_shells" );

	/* Now get the first set of shells */
	for( thick_no=0 ; thick_no<num_thicks ; thick_no++ )
	{
		for( center=1 ; center<3 ; center++ )
			shells[ thick_no*2 + center - 1 ] = Get_shell( thicks[thick_no] , center );
	}

	/* Extrude faces with POS_CENTER first (these faces were defined as the center
	 * of the actual object, so must be extruded in both directions)
	 */
	center = POS_CENTER;
	for( thick_no=0 ; thick_no<num_thicks ; thick_no++ )
	{
		struct shell *s2;

		if( shells[ thick_no*2 + center - 1 ] == (struct shell *)NULL )
			continue;

		/* Extrude distance is one-half the thickness */
		if( Adjust_vertices( shells[ thick_no*2 + center - 1 ] , (fastf_t)(thicks[thick_no]/2.0) ) )
			rt_bomb( "Failure in Adjusting vertices\n" );
	}

	/* make a translation table to store correspondence between original and dups */
	table_size = m->maxindex;
	dup_tbl = (long **)rt_calloc( m->maxindex , sizeof( long *) , "Extrude_faces: dup_tbl" );

	/* Now extrude all the faces by the entire thickness */
	for( thick_no=0 ; thick_no<num_thicks ; thick_no++ )
	{
		for( center=1 ; center<3 ; center++ )
		{
			struct faceuse *fu;
			long **trans_tbl;

			if( shells[ thick_no*2 + center - 1 ] == (struct shell *)NULL )
				continue;

			dup_shells[ thick_no*2 + center - 1 ] = nmg_dup_shell( shells[thick_no*2+center-1],
									&trans_tbl );

			/* move trans_tbl info to dup_tbl */
			for( i=0 ; i<table_size ; i++ )
			{
				if( trans_tbl[i] )
					dup_tbl[i] = trans_tbl[i];
			}

			rt_free( (char *)trans_tbl , "Extrude_faces: trans_tbl" );

			Glue_shell_faces( dup_shells[ thick_no*2 + center - 1 ] );

			nmg_invert_shell( dup_shells[ thick_no*2 + center - 1 ] , &tol );

			if( Adjust_vertices( dup_shells[ thick_no*2 + center - 1 ] , thicks[thick_no] ) )
				rt_bomb( "Failure in Adjust vertices\n" );
		}
	}

	/* And kill the original shell */
	if( RT_LIST_NON_EMPTY( &s->fu_hd ) )
		rt_log( "Original shell should be empty, but it is not!!!\n" );

	nmg_ks( s );
	s = (struct shell *)NULL;

	/* Now reunite adjacent faces that were seperated in "Get_shell" */
	Reunite_faces( dup_tbl );

	/* Merge all shells into just two */
	s1 = (struct shell *)NULL;
	s2 = (struct shell *)NULL;
	for( thick_no=0 ; thick_no<num_thicks ; thick_no++ )
	{
		for( center=1 ; center<3 ; center++ )
		{
			if( shells[ thick_no*2 + center - 1 ] == (struct shell *)NULL )
				continue;

			if( s1 == (struct shell *)NULL )
			{
				s1 = shells[ thick_no*2 + center - 1 ];
				s2 = dup_shells[ thick_no*2 + center - 1 ];
			}
			else
			{
				nmg_js( s1 , shells[ thick_no*2 + center - 1 ] , &tol );
				nmg_js( s2 , dup_shells[ thick_no*2 + center - 1 ] , &tol );
			}
		}
	}
	Glue_shell_faces( s1 );
	Glue_shell_faces( s2 );

	/* Now combine the final two */
	if( debug )
		rt_log( "Close shell\n" );
	if( nmg_open_shells_connect( s1 , s2 , dup_tbl , &tol ) )
		rt_bomb( "Extrude_faces: Could not connect plate mode shells.\n" );

	rt_free( (char *)thicks , "Extrude_faces: thicks" );
	rt_free( (char *)shells , "Extrude_faces: shells" );
	rt_free( (char *)dup_shells , "Extrude_faces: dup_shells" );
	rt_free( (char *)dup_tbl , "Extrude_faces: dup_tbl" );
	s = s1;
	Glue_shell_faces( s );
}

void
Make_arb6_obj()
{
	int arb_count=0;
	point_t pts[8];
	struct fast_fus *fus;
	char name[NAMESIZE+1];
	struct wmember head;
	struct wmember arb6_head;

	RT_LIST_INIT( &arb6_head.l );

	fus = fus_root;
	while( fus )
	{
		struct faceuse *fu;
		struct loopuse *lu;
		struct edgeuse *eu;
		struct vertex *v;
		struct vertex_g *vg;
		vect_t normal;
		char arb6_name[NAMESIZE+1];

		fu = fus->fu;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;
		if( fu->orientation != OT_SAME )
		{
			rt_log( "Make_arb6_obj: face has no OT_SAME use (fu=x%x)\n" , fu );
			return;
		}

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		{
			rt_log( "Make_arb6_obj: Failed (loop without edgeuses)\n" );
			return;
		}

		NMG_GET_FU_NORMAL( normal , fu );

		eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
		NMG_CK_EDGEUSE( eu );
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );
		VMOVE( pts[0] , vg->coord );

		eu = RT_LIST_PNEXT( edgeuse , eu );
		NMG_CK_EDGEUSE( eu );
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );
		VMOVE( pts[1] , vg->coord );

		eu = RT_LIST_PNEXT( edgeuse , eu );
		NMG_CK_EDGEUSE( eu );
		v = eu->vu_p->v_p;
		NMG_CK_VERTEX( v );
		vg = v->vg_p;
		NMG_CK_VERTEX_G( vg );
		VMOVE( pts[4] , vg->coord );
		VMOVE( pts[5] , pts[4] );

		VJOIN1( pts[3] , pts[0] , fus->thick , normal );
		VJOIN1( pts[2] , pts[1] , fus->thick , normal );
		VJOIN1( pts[6] , pts[4] , fus->thick , normal );
		VMOVE( pts[7] , pts[6] );

		sprintf( arb6_name , "arb%d.%d.%d" , group_id , comp_id , arb_count );
		arb_count++;

		mk_arb8( fdout , arb6_name , pts );

		if( mk_addmember( arb6_name , &arb6_head , WMOP_UNION ) == WMEMBER_NULL )
			rt_log( "Make_arb6_obj: Failed to add %s to member list\n" , arb6_name );

		fus = fus->next;
	}

out:	nmg_km( m );

	m = (struct model *)NULL;
	r = (struct nmgregion *)NULL;
	s = (struct shell *)NULL;

	fus = fus_root;
	while( fus )
	{
		struct fast_fus *tmp;

		tmp = fus;
		fus = fus->next;
		rt_free( (char *)tmp , "make_nmg_objects: fus" );
	}
	fus_root = (struct fast_fus *)NULL;

	/* make arb6 group */
	make_nmg_name( name , region_id );
	mk_lfcomb( fdout, name, &arb6_head, 0 );

	/* make region containing nmg object */
	RT_LIST_INIT( &head.l );

	if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
	{
		rt_log( "make_nmg_objects: mk_addmember failed\n" , name );
		rt_bomb( "Cannot make nmg region\n" );
	}

	/* subtract any holes for this component */
	Subtract_holes( &head , comp_id , group_id );

	MK_REGION( fdout , &head , group_id , comp_id , nmgs , NMG )
}

void
make_nmg_objects()
{
	struct faceuse *fu;
	struct nmg_ptbl faces;
	struct fast_fus *fus;
	struct adjacent_faces *adj;
	struct wmember head;
	int failed=0;
	char name[NAMESIZE+1];

	if( !m )
		return;

	/* FASTGEN modellers don't always put vertices in the middle of edges where
	 * another edge ends
	 */
	(void)nmg_break_long_edges( s , &tol );

	/* Fuse vertices.
	 * Don't want to do an overall model fuse here,
	 * shared face geometry will cause problems during extrusion
	 */
	(void)nmg_model_vertex_fuse( m , &tol );

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		nmg_tbl( &faces , TBL_INS , (long *)fu );
	}

	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

	nmg_fix_normals( s , &tol );

	if( mode == PLATE_MODE && try_count )
	{
		Make_arb6_obj();
		return;
	}
	else if( mode == PLATE_MODE )
	{
		if( debug )
		{
			char name[NAMESIZE+1];

			rt_log( "\tExtrude faces\n" );
			sprintf( name , "shell.%d.%d" , group_id , comp_id );
			mk_nmg( fdout , name , m );
			fflush( fdout );
		}
		/* make all faces triangular
		 * nmg_break_long_edges may have made non-triangular faces
		 */
		nmg_triangulate_model( m , &tol );

		if( debug )
			rt_log( "extrude faces\n" );

		if( Extrude_faces() )
		{
			rt_log( "Failed to Extrude group_id = %d, component_id = %d\n" , group_id , comp_id );
			nmg_tbl( &faces , TBL_FREE , (long *)NULL );
			failed = 1;
			goto out;
		}

		nmg_tbl( &faces , TBL_RST , (long *)NULL );

		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			nmg_tbl( &faces , TBL_INS , (long *)fu );
		}

		nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );

		if( debug )
			rt_log( "Fix normals\n" );

		nmg_fix_normals( s , &tol );
	}
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	/* recompute the bounding boxes */
	nmg_rebound( m , &tol );

	if( debug && mode != PLATE_MODE )
	{
		char name[NAMESIZE+1];

		sprintf( name , "shell.%d.%d" , group_id , comp_id );
		mk_nmg( fdout , name , m );
		fflush( fdout );
	}

	if( debug )
		rt_log( "Coplanar face merge\n" );

	tmp_shell_coplanar_face_merge( s , &tol , 0 );
	Recalc_face_g( s , &tol );

	if( debug )
		rt_log( "model fuse\n" );

	nmg_model_fuse( m , &tol );

	make_nmg_name( name , region_id );

	if( polysolids )
	{
		/* XXXX Need a decompose shell, write out each shell as a polysolid
		 * and combine them into one object as "name"
		 */
		write_shell_as_polysolid( fdout , name , s );
	}
	else
	{
		nmg_simplify_shell( s );

		mk_nmg( fdout , name , m );
		fflush( fdout );
	}

	try_count = 0;

out:	nmg_km( m );

	m = (struct model *)NULL;
	r = (struct nmgregion *)NULL;
	s = (struct shell *)NULL;

	fus = fus_root;
	while( fus )
	{
		struct fast_fus *tmp;

		tmp = fus;
		fus = fus->next;
		rt_free( (char *)tmp , "make_nmg_objects: fus" );
	}
	fus_root = (struct fast_fus *)NULL;

	adj = adj_root;
	while( adj )
	{
		struct adjacent_faces *tmp;

		tmp = adj;
		adj = adj->next;
		rt_free( (char *)tmp , "make_nmg_objects: adj" );
	}
	adj_root = (struct adjacent_faces *)NULL;

	if( failed )
		return;

	/* make region containing nmg object */
	RT_LIST_INIT( &head.l );

	if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
	{
		rt_log( "make_nmg_objects: mk_addmember failed\n" , name );
		rt_bomb( "Cannot make nmg region\n" );
	}

	/* subtract any holes for this component */
	Subtract_holes( &head , comp_id , group_id );

	MK_REGION( fdout , &head , group_id , comp_id , nmgs , NMG )

}

void
do_vehicle()
{
	if( pass )
		return;

	strncpy( vehicle , &line[8] , 16 );
	mk_id_units( fdout , vehicle , "in" );
}

void
Add_to_clines( element_id , pt1 , pt2 , thick , radius )
int element_id;
int pt1;
int pt2;
fastf_t thick;
fastf_t radius;
{
	struct cline *cline_ptr;

	cline_ptr = (struct cline *)rt_malloc( sizeof( struct cline ) , "Add_to_clines: cline_ptr" );

	cline_ptr->pt1 = pt1;
	cline_ptr->pt2 = pt2;
	cline_ptr->element_id = element_id;
	cline_ptr->made = 0;
	cline_ptr->thick = thick;
	cline_ptr->radius = radius;
	cline_ptr->next = (struct cline *)NULL;

	if( cline_last_ptr == (struct cline *)NULL )
		cline_root = cline_ptr;
	else
		cline_last_ptr->next = cline_ptr;

	cline_last_ptr = cline_ptr;
}

void
do_cline()
{
	int element_id;
	int pt1,pt2;
	fastf_t thick;
	fastf_t radius;
	vect_t height;
	char name[NAMESIZE+1];

	if( debug )
		rt_log( "do_cline: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );
	if( pass && (pt1 < 1 || pt1 > max_grid_no) )
	{
		rt_log( "Illegal grid point (%d) in CLINE, skipping\n", pt1 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );
	if( pass && (pt2 < 1 || pt2 > max_grid_no) )
	{
		rt_log( "Illegal grid point in CLINE (%d), skipping\n", pt2 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( pt1 == pt2 )
	{
		rt_log( "Ilegal grid points in CLINE ( %d and %d ), skipping\n", pt1 , pt2 );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CLINE );
		make_region_name( name , group_id , comp_id , -pt1 , CLINE );
		return;
	}

	strncpy( field , &line[56] , 8 );
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	radius = atof( field ) * 25.4;

	VSUB2( height , grid_pts[pt2].pt , grid_pts[pt1].pt );

	make_solid_name( name , CLINE , element_id , comp_id , group_id , 0 );
	mk_rcc( fdout , name , grid_pts[pt1].pt , height , radius );

	if( thick > radius )
	{
		rt_log( "ERROR: CLINE thickness (%f) greater than radius (%f)\n", thick, radius );
		rt_log( "\tnot making inner cylinder\n" );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		thick = 0.0;
	}
	else if( thick > 0.0 )
	{
		make_solid_name( name , CLINE , element_id , comp_id , group_id , 1 );
		mk_rcc( fdout , name , grid_pts[pt1].pt , height , radius-thick );
	}

	Add_to_clines( element_id , pt1 , pt2 , thick , radius );
}

void
do_ccone1()
{
	int element_id;
	int pt1,pt2;
	fastf_t thick;
	int c1,c2;
	int end1,end2;
	vect_t height;
	fastf_t r1,r2;
	char outer_name[NAMESIZE+1];
	char inner_name[NAMESIZE+1];
	char reg_name[NAMESIZE+1];
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( outer_name , group_id , comp_id , element_id , CCONE1 );
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , c1 );
			rt_bomb( "CCONE1\n" );
		}
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[56] , 8 );
	thick = atof( field ) * 25.4;

	strncpy( field , &line[64] , 8 );
	r1 = atof( field ) * 25.4;

	strncpy( field , &line[72] , 8 );
	c1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE1\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		rt_log( "WARNING: CCONE1 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
	}

	strncpy( field , &line[8] , 8 );
	r2 = atof( field ) * 25.4;

	strncpy( field , &line[16] , 8 );
	end1 = atoi( field );

	strncpy( field , &line[24] , 8 );
	end2 = atoi( field );

	if( r1 < 0.0 || r2 < 0.0 )
	{
		rt_log( "ERROR: CCONE1 has illegal radii, %f and %f\n" , r1/25.4 , r2/25.4 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		rt_log( "\tCCONE1 solid ignored\n" );
		return;
	}

	if( mode == PLATE_MODE )
	{
		if( thick <= 0.0 )
		{
			rt_log( "ERROR: Plate mode CCONE1 has illegal thickness (%f)\n" , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 solid ignored\n" );
			return;
		}

		if( r1-thick <= SQRT_SMALL_FASTF && r2-thick <= SQRT_SMALL_FASTF )
		{
			rt_log( "ERROR: Plate mode CCONE1 has too large thickness (%f)\n" , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 solid ignored\n" );
			return;
		}
	}

	if( pt1 < 1 || pt1 > max_grid_no || pt2 < 1 || pt2 > max_grid_no || pt1 == pt2 )
	{
		rt_log( "ERROR: CCONE1 has illegal grid points ( %d and %d)\n" , pt1 , pt2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		rt_log( "\tCCONE1 solid ignored\n" );
		return;
	}

	/* BRL_CAD doesn't allow zero radius, so use a very small radius */
	if( r1 < SQRT_SMALL_FASTF )
		r1 = SQRT_SMALL_FASTF;
	if( r2 < SQRT_SMALL_FASTF )
		r2 = SQRT_SMALL_FASTF;

	/* make outside TGC */
	VSUB2( height , grid_pts[pt2].pt , grid_pts[pt1].pt );
	make_solid_name( outer_name , CCONE1 , element_id , comp_id , group_id , 0 );
	mk_trc_h( fdout , outer_name , grid_pts[pt1].pt , height , r1 , r2 );

	RT_LIST_INIT( &r_head.l );
	if( mk_addmember( outer_name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "CCONE1: mk_addmember failed\n" );

	if( mode == PLATE_MODE )
	{
		/* make inside TGC */

		point_t base;
		point_t top;
		vect_t inner_height;
		fastf_t inner_r1,inner_r2;
		fastf_t length;
		vect_t height_dir;

		length = MAGNITUDE( height );
		VSCALE( height_dir , height , 1.0/length );

		if( end1 == END_OPEN )
		{
			inner_r1 = r1 - thick;
			VMOVE( base , grid_pts[pt1].pt );
		}
		else
		{
			inner_r1 = r1 + thick * (r2 - r1)/length - thick;
			VJOIN1( base , grid_pts[pt1].pt , thick , height_dir );
		}

		if( inner_r1 < SQRT_SMALL_FASTF )
		{
			fastf_t dist_to_new_base;

			inner_r1 = SQRT_SMALL_FASTF;
			dist_to_new_base = length * ( thick + inner_r1 - r1)/(r2 - r1);

			VJOIN1( base , grid_pts[pt1].pt , dist_to_new_base , height_dir );
		}

		if( end2 == END_OPEN )
		{
			inner_r2 = r2 - thick;
			VMOVE( top , grid_pts[pt2].pt );
		}
		else
		{
			inner_r2 = r1 + (length - thick)*(r2 - r1)/length - thick;
			VJOIN1( top , grid_pts[pt2].pt , -thick , height_dir );
		}

		if( inner_r2 < SQRT_SMALL_FASTF )
		{
			fastf_t dist_to_new_top;

			inner_r2 = SQRT_SMALL_FASTF;
			dist_to_new_top = length * ( thick + inner_r1 - r1)/(r2 - r1);

			VJOIN1( top , grid_pts[pt1].pt , dist_to_new_top , height );
		}

		VSUB2( inner_height , top , base );
		if( VDOT( inner_height , height ) < 0.0 )
		{
			rt_log( "ERROR: CCONE1 height (%f) too small for thickness (%f)\n" , length/25.4 , thick/25.4 );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
				group_id, comp_id, element_id );
			rt_log( "\tCCONE1 inner solid ignored\n" );
		}
		else
		{
			/* make inner tgc */

			make_solid_name( inner_name , CCONE1 , element_id , comp_id , group_id , 1 );
			mk_trc_h( fdout , inner_name , base , inner_height , inner_r1 , inner_r2 );

			if( mk_addmember( inner_name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
				rt_bomb( "CCONE1: mk_addmember failed\n" );
		}
	}

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE1 )
}

void
do_ccone2()
{
	int element_id;
	int pt1,pt2;
	int c1,c2;
	fastf_t ro1,ro2,ri1,ri2;
	vect_t height;
	char name[NAMESIZE+1];
	struct wmember r_head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		make_region_name( name , group_id , comp_id , element_id , CCONE2 );
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CCONE1\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , c1 );
			rt_bomb( "CCONE2\n" );
		}
		return;
	}

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[64] , 8 );
	ro1 = atof( field ) * 25.4;

	strncpy( field , &line[72] , 8 );
	c1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CCONE2\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , c1 );
		rt_bomb( "CCONE2\n" );
	}

	strncpy( field , line , 8 );
	c2 = atoi( field );

	if( c1 != c2 )
	{
		rt_log( "WARNING: CCONE2 continuation flags disagree, %d vs %d\n" , c1 , c2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
	}

	strncpy( field , &line[8] , 8 );
	ro2 = atof( field ) * 25.4;

	strncpy( field , &line[16] , 8 );
	ri1 = atof( field ) * 25.4;

	strncpy( field , &line[24] , 8 );
	ri2 = atof( field ) * 25.4;

	if( pt1 == pt2 )
	{
		rt_log( "ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0 )
	{
		rt_log( "ERROR: CCONE2 has illegal radii %f %f %f %f\n" , ro1, ro2, ri1, ri2 );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	if( ro1 < SQRT_SMALL_FASTF )
		ro1 = SQRT_SMALL_FASTF;

	if( ro2 < SQRT_SMALL_FASTF )
		ro2 = SQRT_SMALL_FASTF;

	if( ri1 < SQRT_SMALL_FASTF )
		ri1 = SQRT_SMALL_FASTF;

	if( ri2 < SQRT_SMALL_FASTF )
		ri2 = SQRT_SMALL_FASTF;

	RT_LIST_INIT( &r_head.l );

	VSUB2( height , grid_pts[pt2].pt , grid_pts[pt1].pt );

	make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 0 );
	mk_trc_h( fdout , name , grid_pts[pt1].pt , height , ro1 , ro2 );

	if( mk_addmember( name , &r_head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "mk_addmember failed!\n" );

	make_solid_name( name , CCONE2 , element_id , comp_id , group_id , 1 );
	mk_trc_h( fdout , name , grid_pts[pt1].pt , height , ri1 , ri2 );

	if( mk_addmember( name , &r_head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
		rt_bomb( "mk_addmember failed!\n" );

	/* subtract any holes for this component */
	Subtract_holes( &r_head , comp_id , group_id );

	MK_REGION( fdout , &r_head , group_id , comp_id , element_id , CCONE2 )
}

void
Add_holes( gr , comp , ptr )
int gr;
int comp;
struct hole_list *ptr;
{
	struct holes *hole_ptr;

	if( !hole_root )
	{
		hole_root = (struct holes *)rt_malloc( sizeof( struct holes ) , "Add_holes: hole_root" );
		hole_root->group = gr;
		hole_root->component = comp;
		hole_root->holes = ptr;
		hole_root->next = (struct holes *)NULL;
		return;
	}

	hole_ptr = hole_root;
	while( hole_ptr &&
	    hole_ptr->group != gr &&
	    hole_ptr->component != comp &&
	    hole_ptr->next )
		hole_ptr = hole_ptr->next;

	if( !hole_ptr )
		rt_bomb( "ERROR: Add_holes fell off end of list\n" );

	if( hole_ptr->group == gr && hole_ptr->component == comp )
	{
		struct hole_list *list;

		if( !hole_ptr->holes )
			hole_ptr->holes = ptr;
		else
		{
			list = hole_ptr->holes;
			while( list->next )
				list = list->next;
			list->next = ptr;
		}
	}

	hole_ptr->next = (struct holes *)rt_malloc( sizeof( struct holes ) , "Add_holes: hole_ptr->next" );
	hole_ptr = hole_ptr->next;
	hole_ptr->group = gr;
	hole_ptr->component = comp;
	hole_ptr->holes = ptr;
	hole_ptr->next = (struct holes *)NULL;
}

void
do_hole_wall()
{
	struct hole_list *list_ptr;
	struct hole_list *list_start;
	int group, comp;
	int igrp, icmp;
	int s_len;
	int col;

	if( !pass )
		return;

	if( debug )
		rt_log( "do_hole_wall: %s\n" , line );

	s_len = strlen( line );
	if( s_len > 80 )
		s_len = 80;

	strncpy( field , &line[8] , 8 );
	group = atoi( field );

	strncpy( field , &line[16] , 8 );
	comp = atoi( field );

	list_start = (struct hole_list *)NULL;
	list_ptr = (struct hole_list *)NULL;
	col = 24;
	while( col < s_len )
	{
		strncpy( field , &line[col] , 8 );
		igrp = atoi( field );

		col += 8;
		if( col >= s_len )
			break;

		strncpy( field , &line[col] , 8 );
		icmp = atoi( field );

		if( list_ptr )
		{
			list_ptr->next = (struct hole_list *)rt_malloc( sizeof( struct hole_list ) , "do_hole_wall: list_ptr" );
			list_ptr = list_ptr->next;
		}
		else
		{
			list_ptr = (struct hole_list *)rt_malloc( sizeof( struct hole_list ) , "do_hole_wall: list_ptr" );
			list_start = list_ptr;
		}

		list_ptr->group = igrp;
		list_ptr->component = icmp;
		list_ptr->next = (struct hole_list *)NULL;

		col += 8;
	}

	Add_holes( group , comp , list_start );
}

int
getline()
{
	int len;

	bzero( (void *)line , LINELEN );

	if( fgets( line , LINELEN , fdin ) == (char *)NULL )
		return( 0 );

	len = strlen( line );
	if( line[len-1] == '\n' )
		line[len-1] = '\0';

	return( 1 );
}

void
make_fast_fu( pt1 , pt2 , pt3 , element_id , thick , pos )
int pt1,pt2,pt3;
int element_id;
fastf_t thick;
int pos;
{
	struct vertex **verts[3];
	struct faceuse *fu;
	struct loopuse *lu;
	struct fast_fus *fus;
	fastf_t area;
	plane_t pl;

	if( debug )
		rt_log( "make_fast_fu: ( %f %f %f )\n\t( %f %f %f )\n\t( %f %f %f )\n" ,
			V3ARGS( grid_pts[pt1].pt ), V3ARGS( grid_pts[pt2].pt ), V3ARGS( grid_pts[pt3].pt ) );

	verts[0] = &grid_pts[pt1].v;
	verts[1] = &grid_pts[pt2].v;
	verts[2] = &grid_pts[pt3].v;

	if( !m )
	{
		m = nmg_mm();
		r = nmg_mrsv( m );
		s = RT_LIST_FIRST( shell , &r->s_hd );
	}

	if( debug )
		rt_log( "\tm = x%x\n" , m );

	fu = nmg_cmface( s , verts , 3 );
	if( !fu )
	{
		rt_log( "do_tri: nmg_cmface failed\n" );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( debug )
		rt_log( "\tfu = x%x\n" , fu );

	lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
	nmg_vertex_gv( grid_pts[pt1].v , grid_pts[pt1].pt );
	nmg_vertex_gv( grid_pts[pt2].v , grid_pts[pt2].pt );
	nmg_vertex_gv( grid_pts[pt3].v , grid_pts[pt3].pt );

	area = nmg_loop_plane_area( lu , pl );
	if( area <= 0.0 )
	{
		(void)nmg_kfu( fu );
		rt_log( "do_tri: ignoring degenerate CTRI element\n" );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	nmg_face_g( fu , pl );
	nmg_loop_g( lu->l_p , &tol );

	/* save faceuse and thickness info for plate mode */
	if( mode == PLATE_MODE )
	{
		if( !fus_root )
		{
			fus_root = (struct fast_fus *)rt_malloc( sizeof( struct fast_fus ) , "Do_tri: fus_root" );
			fus = fus_root;
		}
		else
		{
			fus = fus_root;
			while( fus->next )
				fus = fus->next;
			fus->next = (struct fast_fus *)rt_malloc( sizeof( struct fast_fus ) , "Do_tri: fus" );
			fus = fus->next;
		}

		fus->next = (struct fast_fus *)NULL;
		fus->fu = fu;
		fus->thick = thick;
		fus->pos = pos;
	}
}

void
do_tri()
{
	int element_id;
	int pt1,pt2,pt3;
	fastf_t thick;
	int pos;

	if( debug )
		rt_log( "do_tri: %s\n" , line );

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !nmgs )
		nmgs = element_id;

	if( !pass )
		return;

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[40] , 8 );
	pt3 = atoi( field );

	if( pt1 == pt2 || pt2 == pt3 || pt1 == pt3 )
	{
		rt_log( "do_tri: ignoring degenerate CTRI element\n" );
		rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
		return;
	}

	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;
		if( thick <= 0.0 )
		{
			rt_log( "do_tri: illegal thickness (%f), skipping CTRI element\n" , thick );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( pos == 0 )	/* use default */
			pos = POS_FRONT;

		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			rt_log( "do_tri: illegal postion parameter (%d), must be one or two\n" , pos );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}
		if( debug )
			rt_log( "\tplate mode: thickness = %f\n" , thick );
	}

	make_fast_fu( pt1 , pt2 , pt3 , element_id , thick , pos );
}

void
do_quad()
{
	int element_id;
	int pt1,pt2,pt3,pt4;
	fastf_t thick;
	int pos;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( debug )
		rt_log( "do_quad: %s\n" , line );

	if( !nmgs )
		nmgs = element_id;

	if( !pass )
		return;

	strncpy( field , &line[24] , 8 );
	pt1 = atoi( field );

	strncpy( field , &line[32] , 8 );
	pt2 = atoi( field );

	strncpy( field , &line[40] , 8 );
	pt3 = atoi( field );

	strncpy( field , &line[48] , 8 );
	pt4 = atoi( field );

	if( mode == PLATE_MODE )
	{
		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;
		if( thick <= 0.0 )
		{
			rt_log( "do_quad: illegal thickness (%f), skipping CQUAD element\n" , thick );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( pos == 0 )	/* use default */
			pos = POS_FRONT;

		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			rt_log( "do_quad: illegal postion parameter (%d), must be one or two\n" , pos );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}
	}

	if( pt1 != pt2 && pt2 != pt3 && pt1 != pt3 )
	{

		make_fast_fu( pt1 , pt2 , pt3 , element_id , thick , pos );
	}

	if( pt1 != pt3 && pt3 != pt4 && pt1 != pt4 )
	{

		make_fast_fu( pt1 , pt3 , pt4 , element_id , thick , pos );
	}
}

/*	cleanup from previous component and start a new one */
void
do_section( final )
int final;
{

	if( debug )
		rt_log( "do_section: %s\n" , line );

	prev_sect = curr_sect;
	curr_sect = curr_offset;

	if( pass )
	{
		if( region_id )
		{
			if( !try_count )
				comp_count++;
			rt_log( "Making component %s, group #%d, component #%d\n",
					name_name, group_id , comp_id );
			if( RT_SETJUMP )
			{
				struct fast_fus *fus;
				struct cline *cline_ptr;
				struct shell_list *shell_ptr;
				struct adjacent_faces *adj;

				RT_UNSETJUMP;
				rt_log( "***********component %s, group #%d, component #%d failed\n",
					name_name, group_id , comp_id );
				if( m )
				{
					nmg_km( m );
					m = (struct model *)NULL;
					r = (struct nmgregion *)NULL;
					s = (struct shell *)NULL;
				}

				/* reset debug flags to what the user requested */
				rt_g.NMG_debug = nmg_debug;
				rt_g.debug = rt_debug;

				/* clear lists */
				fus = fus_root;
				while( fus )
				{
					struct fast_fus *tmp;

					tmp = fus;
					fus = fus->next;
					rt_free( (char *)tmp , "Do_section: fus" );
				}
				fus_root = (struct fast_fus *)NULL;

				cline_ptr = cline_root;
				while( cline_ptr )
				{
					struct cline *tmp;

					tmp = cline_ptr;
					cline_ptr = cline_ptr->next;
					rt_free( (char *)tmp , "Do_section: cline_ptr" );
				}
				cline_root = (struct cline *)NULL;

				shell_ptr = shell_root;
				while( shell_ptr )
				{
					struct shell_list *tmp;

					tmp = shell_ptr;
					shell_ptr = shell_ptr->next;
					rt_free( (char *)tmp , "Do_section: shell_ptr" );
				}
				shell_root = (struct shell_list *)NULL;

				adj = adj_root;
				while( adj )
				{
					struct adjacent_faces *tmp;

					tmp = adj;
					adj = adj->next;
					rt_free( (char *)tmp , "Do_section: adj" );
				}
				adj_root = (struct adjacent_faces *)NULL;

				/* If a plate-mode component failed, try again using arb6's */
				if( mode == PLATE_MODE )
				{
					/* increment try counter */
					try_count++;

					if( try_count == 1 )
					{
						if( fseek( fdin , prev_sect , SEEK_SET ) )
						{
							rt_log( "Cannot seek in input file, not retrying group %d, comnponent %d\n",
								group_id , comp_id );
						}
						else
						{
							rt_log( "Retrying group %d, component %d\n",
								group_id , comp_id );
							if( !final )
								(void)getline();
						}
					}
				}
			}
			else
			{
				make_nmg_objects();
				make_cline_regions();
				make_comp_group();

				if( try_count )
				{
					second_chance++;
					try_count = 0;
				}
				else
					conv_count++;

				RT_UNSETJUMP;
			}
		}
		if( final && debug ) /* The ENDATA card has been found */
			List_names();
	}
	else if( nmgs )
	{
		char	name[NAMESIZE+1];

		make_region_name( name , group_id , comp_id , nmgs , NMG );
	}

	if( !final )
	{
		strncpy( field , &line[8] , 8 );
		group_id = atoi( field );

		strncpy( field , &line[16] , 8 );
		comp_id = atoi( field );

		if( comp_id > 999 )
		{
			rt_log( "Illegal component id number %d, changed to 999\n" , comp_id );
			comp_id = 999;
		}

		region_id = group_id * 1000 + comp_id;

		strncpy( field , &line[24] , 8 );
		mode = atoi( field );
		if( mode != 1 && mode != 2 )
		{
			rt_log( "Illegal mode (%d) for group %d component %d, using volume mode\n",
				mode, group_id, comp_id );
			mode = 2;
		}

		if( pass )
			name_name[0] = '\0';

		nmgs = 0;
	}
}

struct model *
inside_arb( pts , thick )
int pts[8];
fastf_t thick;
{
	int i;
	struct model *m1;
	struct nmgregion *r1;
	struct shell *s1;
	struct faceuse *fu;
	struct vertex **verts[4];
	struct nmg_ptbl faces;

	nmg_tbl( &faces , TBL_INIT , (long *)NULL );

	m1 = nmg_mm();
	r1 = nmg_mrsv( m1 );
	s1 = RT_LIST_FIRST( shell , &r1->s_hd );
	NMG_CK_SHELL( s1 );

	verts[0] = &grid_pts[pts[0]].v;
	verts[1] = &grid_pts[pts[1]].v;
	verts[2] = &grid_pts[pts[2]].v;
	verts[3] = &grid_pts[pts[3]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	verts[0] = &grid_pts[pts[4]].v;
	verts[1] = &grid_pts[pts[5]].v;
	verts[2] = &grid_pts[pts[6]].v;
	verts[3] = &grid_pts[pts[7]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	verts[0] = &grid_pts[pts[0]].v;
	verts[1] = &grid_pts[pts[1]].v;
	verts[2] = &grid_pts[pts[5]].v;
	verts[3] = &grid_pts[pts[4]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	verts[0] = &grid_pts[pts[1]].v;
	verts[1] = &grid_pts[pts[5]].v;
	verts[2] = &grid_pts[pts[6]].v;
	verts[3] = &grid_pts[pts[2]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	verts[0] = &grid_pts[pts[2]].v;
	verts[1] = &grid_pts[pts[6]].v;
	verts[2] = &grid_pts[pts[7]].v;
	verts[3] = &grid_pts[pts[3]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	verts[0] = &grid_pts[pts[0]].v;
	verts[1] = &grid_pts[pts[4]].v;
	verts[2] = &grid_pts[pts[7]].v;
	verts[3] = &grid_pts[pts[3]].v;

	fu = nmg_cmface( s1 , verts , 4 );
	nmg_tbl( &faces , TBL_INS , (long *)fu );

	for( i=0 ; i<8 ; i++ )
		nmg_vertex_gv( grid_pts[pts[i]].v , grid_pts[pts[i]].pt );

	i = 0;
	while( i < NMG_TBL_END( &faces ) )
	{
		struct loopuse *lu;
		fastf_t area;
		plane_t pl;

		fu = (struct faceuse *)NMG_TBL_GET( &faces , i );
		NMG_CK_FACEUSE( fu );

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		NMG_CK_LOOPUSE( lu );

		area = nmg_loop_plane_area( lu , pl );

		if( area <= 0.0 )
		{
			nmg_kfu( fu );
			nmg_tbl( &faces , TBL_RM , (long *)fu );
		}
		else
		{
			nmg_face_g( fu , pl );
			i++;
		}
	}

	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , (long *)NULL );

	nmg_fix_normals( s1 , &tol );

	if( nmg_extrude_shell( s1 , thick , 0 , 0 , &tol ) == (struct shell *)NULL )
	{
		nmg_km( m1 );
		m1 = (struct model *)NULL;
	}

	return( m1 );
}

void
do_hex1()
{
	struct wmember head;
	fastf_t thick=0.0;
	point_t points[8];
	int pos;
	int pts[8];
	int element_id;
	int i;
	int cont1,cont2;
	char name[NAMESIZE+1];

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !pass )
	{
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CHEX1\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , cont1 );
			rt_bomb( "CHEX1\n" );
		}
		return;
	}

	for( i=0 ; i<6 ; i++ )
	{
		strncpy( field , &line[24 + i*8] , 8 );
		pts[i] = atoi( field );
	}

	strncpy( field , &line[72] , 8 );
	cont1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CHEX1\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , cont1 );
		rt_bomb( "CHEX1\n" );
	}

	strncpy( field , line , 8 );
	cont2 = atoi( field );

	if( cont1 != cont2 )
	{
		rt_log( "Continuation card numbers do not match for CHEX1 element (%d vs %d)\n", cont1 , cont2 );
		rt_log( "\tskipping CHEX1 element: group_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	strncpy( field , &line[8] , 8 );
	pts[6] = atoi( field );

	strncpy( field , &line[16] , 8 );
	pts[7] = atoi( field );

	for( i=0 ; i<8 ; i++ )
		VMOVE( points[i] , grid_pts[pts[i]].pt );

	make_solid_name( name , CHEX1 , element_id , comp_id , group_id , 0 );
	mk_arb8( fdout , name , points );

	RT_LIST_INIT( &head.l );

	if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "CHEX1: mk_addmember failed\n" );

	if( mode == PLATE_MODE )
	{
		struct model *m1;

		strncpy( field , &line[56] , 8 );
		thick = atof( field ) * 25.4;
		if( thick <= 0.0 )
		{
			rt_log( "do_hex1: illegal thickness (%f), skipping CHEX1 element\n" , thick );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		strncpy( field , &line[64] , 8 );
		pos = atoi( field );

		if( pos == 0 )	/* use default */
			pos = POS_FRONT;

		if( pos != POS_CENTER && pos != POS_FRONT )
		{
			rt_log( "do_hex1: illegal postion parameter (%d), must be 1 or 2, skipping CHEX1 element\n" , pos );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			return;
		}

		/* get inside arb */
		m1 = inside_arb( pts , thick );

		if( m1 == (struct model *)NULL )
		{
			rt_log( "do_hex1: Could not find inside for CHEX1 element\n" , pos );
			rt_log( "\tLeaving CHEX1 element solid\n" );
			rt_log( "\telement %d, component %d, group %d\n" , element_id , comp_id , group_id );
			MK_REGION( fdout , &head , group_id , comp_id , element_id , CHEX1 )
			return;
		}

		make_solid_name( name , CHEX1 , element_id , comp_id , group_id , 1 );
		mk_nmg( fdout , name , m1 );
		fflush( fdout );

		if( mk_addmember( name , &head , WMOP_SUBTRACT ) == (struct wmember *)NULL )
			rt_bomb( "CHEX1: mk_addmember failed\n" );

		nmg_km( m1 );

	}
	MK_REGION( fdout , &head , group_id , comp_id , element_id , CHEX1 )
}

void
do_hex2()
{
	int pts[8];
	int element_id;
	int i;
	int cont1,cont2;
	point_t points[8];
	char name[NAMESIZE+1];
	struct wmember head;

	strncpy( field , &line[8] , 8 );
	element_id = atoi( field );

	if( !nmgs )
		nmgs = element_id;

	if( !pass )
	{
		if( !getline() )
		{
			rt_log( "Unexpected EOF while reading continuation card for CHEX2\n" );
			rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
				group_id, comp_id, element_id , cont1 );
			rt_bomb( "CHEX2\n" );
		}
		return;
	}

	for( i=0 ; i<6 ; i++ )
	{
		strncpy( field , &line[24 + i*8] , 8 );
		pts[i] = atoi( field );
	}

	strncpy( field , &line[72] , 8 );
	cont1 = atoi( field );

	if( !getline() )
	{
		rt_log( "Unexpected EOF while reading continuation card for CHEX2\n" );
		rt_log( "\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
			group_id, comp_id, element_id , cont1 );
		rt_bomb( "CHEX2\n" );
	}

	strncpy( field , line , 8 );
	cont2 = atoi( field );

	if( cont1 != cont2 )
	{
		rt_log( "Continuation card numbers do not match for CHEX2 element (%d vs %d)\n", cont1 , cont2 );
		rt_log( "\tskipping CHEX2 element: group_id = %d, comp_id = %d, element_id = %d\n",
			group_id, comp_id, element_id );
		return;
	}

	strncpy( field , &line[8] , 8 );
	pts[6] = atoi( field );

	strncpy( field , &line[16] , 8 );
	pts[7] = atoi( field );

	for( i=0 ; i<8 ; i++ )
		VMOVE( points[i] , grid_pts[pts[i]].pt );

	make_solid_name( name , CHEX2 , element_id , comp_id , group_id , 0 );
	mk_arb8( fdout , name , points );

	RT_LIST_INIT( &head.l );

	if( mk_addmember( name , &head , WMOP_UNION ) == (struct wmember *)NULL )
		rt_bomb( "CHEX2: mk_addmember failed\n" );

	MK_REGION( fdout , &head , group_id , comp_id , element_id , CHEX1 )
}

int
Process_input( pass_number )
int pass_number;
{

/*	if( debug ) */
		rt_log( "\n\nProcess_input( pass = %d ), try = %d\n" , pass_number , try_count );
/*	rt_prmem( "At start of Process_input:" );	*/

	if( pass_number != 0 && pass_number != 1 )
	{
		rt_log( "Process_input: illegal pass number %d\n" , pass_number );
		rt_bomb( "Process_input" );
	}

	region_id = 0;
	pass = pass_number;
	curr_offset = ftell( fdin );
	while( getline() )
	{
		if( !strncmp( line , "VEHICLE" , 7 ) )
			do_vehicle();
		else if( !strncmp( line , "HOLE" , 4 ) )
			do_hole_wall();
		else if( !strncmp( line , "WALL" , 4 ) )
			do_hole_wall();
		else if( !strncmp( line , "SECTION" , 7 ) )
			do_section( 0 );
		else if( !strncmp( line , "$NAME" , 5 ) )
			do_name();
		else if( !strncmp( line , "$COMMENT" , 8 ) )
			;
		else if( !strncmp( line , "GRID" , 4 ) )
			do_grid();
		else if( !strncmp( line , "CLINE" , 5 ) )
			do_cline();
		else if( !strncmp( line , "CHEX1" , 5 ) )
			do_hex1();
		else if( !strncmp( line , "CHEX2" , 5 ) )
			do_hex2();
		else if( !strncmp( line , "CTRI" , 4 ) )
			do_tri();
		else if( !strncmp( line , "CQUAD" , 5 ) )
			do_quad();
		else if( !strncmp( line , "CCONE1" , 6 ) )
			do_ccone1();
		else if( !strncmp( line , "CCONE2" , 6 ) )
			do_ccone2();
		else if( !strncmp( line , "CSPHERE" , 7 ) )
			do_sphere();
		else if( !strncmp( line , "ENDDATA" , 7 ) )
		{
			do_section( 1 );
			break;
		}
		else
			rt_log( "ERROR: skipping unrecognized data type\n%s\n" , line );

		curr_offset = ftell( fdin );
	}

	if( try_count )
		return( 1 );

	if( debug )
	{
		rt_log( "At pass %d:\n" , pass );
		List_names();
	}

	return( 0 );
}

main( argc , argv )
int argc;
char *argv[];
{
	int i;
	int c;

	/* Initialze tolerance struct */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	while( (c=getopt( argc , argv , "dwpx:X:D:P:" ) ) != EOF )
	{
		switch( c )
		{
			case 'd':	/* debug option */
				debug = 1;
				break;
			case 'w':	/* print warnings */
				warnings = 1;
				break;
			case 'p':	/* polysolids */
				polysolids = 1;
				break;
			case 'x':
				sscanf( optarg, "%x", &rt_debug );
				rt_g.debug = rt_debug;
				break;
			case 'X':
				sscanf( optarg, "%x", &nmg_debug );
				rt_g.NMG_debug = nmg_debug;
				break;
			case 'D':
				tol.dist = atof( optarg );
				rt_log( "tolerance distance set to %f\n" , tol.dist );
				break;
			case 'P':
				tol.perp = atof( optarg );
				rt_log( "tolerance perpendicular set to %f\n" , tol.perp );
				tol.para = 1.0 - tol.perp;
				rt_log( "tolerance parallel set to %f\n" , tol.para );
				break;
			default:
				rt_log( "Unrecognized option (%c)\n" , c );
				rt_bomb( usage );
				break;
		}
	}

	if( argc-optind != 2 )
		rt_bomb( usage );

	if( (fdin=fopen( argv[optind] , "r" )) == (FILE *)NULL )
	{
		rt_log( "Cannot open FASTGEN4 file (%s)\n" , argv[optind] );
		perror( "fast4-g" );
		exit( 1 );
	}

	if( (fdout=fopen( argv[optind+1] , "w" )) == (FILE *)NULL )
	{
		rt_log( "Cannot open file for output (%s)\n" , argv[optind+1] );
		perror( "fast4-g" );
		exit( 1 );
	}

	grid_size = GRID_BLOCK;
	grid_pts = (struct fast_verts *)rt_malloc( grid_size * sizeof( struct fast_verts ) , "fast4-g: grid_pts" );

	cline_root = (struct cline *)NULL;
	cline_last_ptr = (struct cline *)NULL;

	name_root = (struct name_tree *)NULL;

	hole_root = (struct holes *)NULL;

	fus_root = (struct fast_fus *)NULL;

	adj_root = (struct adjacent_faces *)NULL;

	name_name[0] = '\0';
	name_count = 0;

	vehicle[0] = '\0';

	m = (struct model *)NULL;
	r = (struct nmgregion *)NULL;
	s = (struct shell *)NULL;

	nmg_tbl( &stack , TBL_INIT , (long *)NULL );

	for( i=0 ; i<11 ; i++ )
		RT_LIST_INIT( &group_head[i].l );

	Process_input( 0 );

	rewind( fdin );

	/* Make an ID record if no vehicle card was found */
	if( !vehicle[0] )
		mk_id_units( fdout , argv[optind] , "in" );

	while( Process_input( 1 ) );

	/* make groups */
	do_groups();

	if( debug )
		List_holes();

	rt_log( "%d components converted out of %d attempted\n" , conv_count , comp_count );
	rt_log( "\t%d failures converted on second try (as a group of ARB6 solids)\n", second_chance );
}
