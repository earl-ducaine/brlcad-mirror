/*
 *			B U T T O N S . C
 *
 * Functions -
 *	buttons		Process button-box functions
 */

#include	<math.h>
#include "ged_types.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "menu.h"
#include "dm.h"
#include "db.h"
#include "sedit.h"

extern void	perror();
extern int	printf(), read();
extern long	lseek();

int	adcflag;	/* angle/distance cursor in use */

/* This flag indicates that SOLID editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (eg, BE_S_SCALE).
 */
static	edsol;

/* This flag indicates that OBJECT editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (eg, BE_O_XY).
 */
static	edobj;		/* object editing */
int	movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

/*
 * The "accumulation" solid rotation matrix and scale factor
 */
mat_t	acc_rot_sol;
float	acc_sc_sol;

static	sliceflag;	/* 0 = depth cue mode, !0 = "slice" mode */

static void bv_top(), bv_bottom(), bv_right(), bv_left(), bv_front(), bv_rear();
static void bv_vrestore(), bv_vsave(), bv_adcursor(), bv_reset();
static void bv_90_90(), bv_35_25();
static void be_o_illuminate(), be_s_illuminate();
static void be_o_scale(), be_o_x(), be_o_y(), be_o_xy(), be_o_rotate();
static void be_accept(), be_reject(), bv_slicemode();
static void be_s_edit(), be_s_rotate(), be_s_trans(), be_s_scale();

struct buttons  {
	int	bu_code;	/* dm_values.dv_button */
	char	*bu_name;	/* keyboard string */
	void	(*bu_func)();	/* function to call */
}  button_table[] = {
	BV_TOP,		"top",		bv_top,
	BV_BOTTOM,	"bottom",	bv_bottom,
	BV_RIGHT,	"right",	bv_right,
	BV_LEFT,	"left",		bv_left,
	BV_FRONT,	"front",	bv_front,
	BV_REAR,	"rear",		bv_rear,
	BV_VRESTORE,	"restore",	bv_vrestore,
	BV_VSAVE,	"save",		bv_vsave,
	BV_ADCURSOR,	"adc",		bv_adcursor,
	BV_RESET,	"reset",	bv_reset,
	BV_90_90,	"90,90",	bv_90_90,
	BV_35_25,	"35,25",	bv_35_25,
	BE_O_ILLUMINATE,"oill",		be_o_illuminate,
	BE_S_ILLUMINATE,"sill",		be_s_illuminate,
	BE_O_SCALE,	"oscale",	be_o_scale,
	BE_O_X,		"ox",		be_o_x,
	BE_O_Y,		"oy",		be_o_y,
	BE_O_XY,	"oxy",		be_o_xy,
	BE_O_ROTATE,	"orot",		be_o_rotate,
	BE_ACCEPT,	"accept",	be_accept,
	BE_REJECT,	"reject",	be_reject,
	BV_SLICEMODE,	"slice",	bv_slicemode,
	BE_S_EDIT,	"sedit",	be_s_edit,
	BE_S_ROTATE,	"srot",		be_s_rotate,
	BE_S_TRANS,	"sxy",		be_s_trans,
	BE_S_SCALE,	"sscale",	be_s_scale,
	-1,		"-end-",	be_reject
};

static mat_t sav_viewrot, sav_toviewcenter;
static float sav_viewscale;
static int	vsaved = 0;	/* set iff view saved */

/*
 *			B U T T O N
 */
void
button( bnum )
register long bnum;
{
	register struct buttons *bp;

	if( edsol && edobj )
		(void)printf("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;
		bp->bu_func();
		return;
	}
	(void)printf("button(%d):  Not a defined operation\n", bnum);
}

/*
 *  			P R E S S
 */
void
press( str )
char *str;{
	register struct buttons *bp;

	if( edsol && edobj )
		(void)printf("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );

	if( str[0] == '?' )  {
		register int i=0;
		for( bp = button_table; bp->bu_code >= 0; bp++ )  {
			printf("%s, ", bp->bu_name );
			if( ++i > 4 )  {
				putchar('\n');
				i = 0;
			}
		}
		return;
	}

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( strcmp( str, bp->bu_name) )
			continue;
		bp->bu_func();
		return;
	}
	(void)printf("press(%s):  Unknown operation, type 'press ?' for help\n",
		str);
}
/*
 *  			L A B E L _ B U T T O N
 *  
 *  For a given GED button number, return the "press" ID string.
 *  Useful for displays with programable button lables, etc.
 */
char *
label_button(bnum)
{
	register struct buttons *bp;

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;
		return( bp->bu_name );
	}
	(void)printf("label_button(%d):  Not a defined operation\n", bnum);
}

static void bv_top()  {
	/* Top view */
	setview( 0, 0, 0 );
}

static void bv_bottom()  {
	/* Bottom view */
	setview( 180, 0, 0 );
}

static void bv_right()  {
	/* Right view */
	setview( 270, 0, 0 );
}

static void bv_left()  {
	/* Left view */
	setview( 270, 0, 180 );
}

static void bv_front()  {
	/* Front view */
	setview( 270, 0, 270 );
}

static void bv_rear()  {
	/* Rear view */
	setview( 270, 0, 90 );
}

static void bv_vrestore()  {
	/* restore to saved view */
	if ( vsaved )  {
		Viewscale = sav_viewscale;
		mat_copy( Viewrot, sav_viewrot );
		mat_copy( toViewcenter, sav_toviewcenter );
		new_mats();
		dmaflag++;
	}
}

static void bv_vsave()  {
	/* save current view */
	sav_viewscale = Viewscale;
	mat_copy( sav_viewrot, Viewrot );
	mat_copy( sav_toviewcenter, toViewcenter );
	vsaved = 1;
	dmp->dmr_light( LIGHT_ON, BV_VRESTORE );
}

static void bv_adcursor()  {
	if (adcflag)  {
		/* Was on, turn off */
		adcflag = 0;
		dmp->dmr_light( LIGHT_OFF, BV_ADCURSOR );
	}  else  {
		/* Was off, turn on */
		adcflag = 1;
		dmp->dmr_light( LIGHT_ON, BV_ADCURSOR );
	}
	dmaflag = 1;
}

static void bv_reset()  {
	/* Reset to initial viewing conditions */
	mat_idn( toViewcenter );
	Viewscale = maxview;
	setview( 0, 0, 0 );
}

static void bv_90_90()  {
	/* azm 90   elev 90 */
	setview( 360, 0, 180 );
}

static void bv_35_25()  {
	/* Use Azmuth=35, Elevation=25 in Keith's backwards space */
	setview( 295, 0, 235 );
}

/* returns 0 if error, !0 if success */
static int ill_common()  {
	/* Common part of illumination */
	dmp->dmr_light( LIGHT_ON, BE_REJECT );
	if( HeadSolid.s_forw == &HeadSolid )  {
		(void)printf("no solids in view\n");
		return(0);	/* BAD */
	}
	illump = HeadSolid.s_forw;/* any valid solid would do */
	edobj = 0;		/* sanity */
	edsol = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	mat_idn( modelchanges );	/* No changes yet */
	new_mats();
	dmaflag++;
	return(1);		/* OK */
}

static void be_o_illuminate()  {
	if( not_state( ST_VIEW, "Object Illuminate" ) )
		return;

	dmp->dmr_light( LIGHT_ON, BE_O_ILLUMINATE );
	if( ill_common() )
		(void)chg_state( ST_VIEW, ST_O_PICK, "Object Illuminate" );
}

static void be_s_illuminate()  {
	if( not_state( ST_VIEW, "Solid Illuminate" ) )
		return;

	dmp->dmr_light( LIGHT_ON, BE_S_ILLUMINATE );
	if( ill_common() )
		(void)chg_state( ST_VIEW, ST_S_PICK, "Solid Illuminate" );
}

static void be_o_scale()  {
	if( not_state( ST_O_EDIT, "Object Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_SCALE );
	movedir = SARROW;
	dmaflag++;
}

static void be_o_x()  {
	if( not_state( ST_O_EDIT, "Object X Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_X );
	movedir = RARROW;
	dmaflag++;
}

static void be_o_y()  {
	if( not_state( ST_O_EDIT, "Object Y Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_Y );
	movedir = UARROW;
	dmaflag++;
}


static void be_o_xy()  {
	if( not_state( ST_O_EDIT, "Object XY Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_XY );
	movedir = UARROW | RARROW;
	dmaflag++;
}

static void be_o_rotate()  {
	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_ROTATE );
	movedir = ROTARROW;
	dmaflag++;
}

static void be_accept()  {
	register struct solid *sp;

	if( state == ST_S_EDIT )  {
		/* Accept a solid edit */
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );
		/* write editing changes out to disc */
		db_putrec( illump->s_path[illump->s_last], &es_rec, 0 );

		dmp->dmr_light( LIGHT_OFF, edsol );
		edsol = 0;
		MENU_ON(FALSE);
		MENU_INSTALL( (struct menu_item *)NULL );
		dmp->dmr_light( LIGHT_OFF, BE_S_EDIT );
		es_edflag = -1;
		menuflag = 0;

		FOR_ALL_SOLIDS( sp )
			sp->s_iflag = DOWN;

		illump = SOLID_NULL;
		(void)chg_state( ST_S_EDIT, ST_VIEW, "Edit Accept" );
		dmaflag = 1;		/* show completion */
	}  else if( state == ST_O_EDIT )  {
		/* Accept an object edit */
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );
		dmp->dmr_light( LIGHT_OFF, edobj );
		edobj = 0;
		movedir = 0;	/* No edit modes set */
		if( ipathpos == 0 )  {
			moveHobj( illump->s_path[ipathpos], modelchanges );
		}  else  {
			moveHinstance(
				illump->s_path[ipathpos-1],
				illump->s_path[ipathpos],
				modelchanges
			);
		}

		/*
		 *  Redraw all solids affected by this edit.
		 *  Regenerate a new control list which does not
		 *  include the solids about to be replaced,
		 *  so we can safely fiddle the displaylist.
		 */
		modelchanges[15] = 1000000000;	/* => small ratio */
		dmaflag=1;
		refresh();

		/* Now, recompute new chunks of displaylist */
		FOR_ALL_SOLIDS( sp )  {
			if( sp->s_iflag == DOWN )
				continue;
			/* Rip off es_mat and es_rec for redraw() */
			pathHmat( sp, es_mat );
			db_getrec(sp->s_path[sp->s_last], &es_rec, 0);
			illump = sp;	/* flag to drawHsolid! */
			redraw( sp, &es_rec );
			sp->s_iflag = DOWN;
		}
		mat_idn( modelchanges );

		illump = SOLID_NULL;
		(void)chg_state( ST_O_EDIT, ST_VIEW, "Edit Accept" );
		dmaflag = 1;		/* show completion */
	} else {
		(void)not_state( ST_S_EDIT, "Edit Accept" );
		return;
	}
}

static void be_reject()  {
	register struct solid *sp;

	/* Reject edit */
	dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
	dmp->dmr_light( LIGHT_OFF, BE_REJECT );

	switch( state )  {
	default:
		state_err( "Edit Reject" );
		return;

	case ST_S_EDIT:
		/* Reject a solid edit */
		if( edsol )
			dmp->dmr_light( LIGHT_OFF, edsol );
		MENU_ON( FALSE );
		MENU_INSTALL( (struct menu_item *)NULL );

		/* Restore the saved original solid */
		illump = redraw( illump, &es_orig );
		break;

	case ST_O_EDIT:
		if( edobj )
			dmp->dmr_light( LIGHT_OFF, edobj );
		break;
	case ST_O_PICK:
		dmp->dmr_light( LIGHT_OFF, BE_O_ILLUMINATE );
		break;
	case ST_S_PICK:
		dmp->dmr_light( LIGHT_OFF, BE_S_ILLUMINATE );
		break;
	case ST_O_PATH:
		break;
	}

	menuflag = 0;
	movedir = 0;
	edsol = 0;
	edobj = 0;
	es_edflag = -1;
	illump = SOLID_NULL;		/* None selected */
	dmaflag = 1;

	/* Clear illumination flags */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	(void)chg_state( state, ST_VIEW, "Edit Reject" );
}

static void bv_slicemode() {
	if( sliceflag )  {
		/* depth cue mode */
		sliceflag = 0;
		inten_scale = 0x7FF0;
		dmp->dmr_light( LIGHT_OFF, BV_SLICEMODE );
	} else {
		/* slice mode */
		sliceflag = 1;
		inten_scale = 0xFFF0;
		dmp->dmr_light( LIGHT_ON, BV_SLICEMODE );
	}
}

static void be_s_edit()  {
	/* solid editing */
	if( not_state( ST_S_EDIT, "Solid Edit (Menu)" ) )
		return;

	if( edsol )
		dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_EDIT );
	es_edflag = MENU;
	menuflag = 0;		/* No menu item selected yet */
	MENU_ON(TRUE);
	dmaflag++;
}

static void be_s_rotate()  {
	/* rotate solid */
	if( not_state( ST_S_EDIT, "Solid Rotate" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_ROTATE );
	menuflag = 0;
	MENU_ON( FALSE );
	es_edflag = SROT;
	mat_idn(acc_rot_sol);
	dmaflag++;
}

static void be_s_trans()  {
	/* translate solid */
	if( not_state( ST_S_EDIT, "Solid Translate" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_TRANS );
	menuflag = 0;
	es_edflag = STRANS;
	movedir = UARROW | RARROW;
	MENU_ON( FALSE );
	dmaflag++;
}

static void be_s_scale()  {
	/* scale solid */
	if( not_state( ST_S_EDIT, "Solid Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_SCALE );
	menuflag = 0;
	es_edflag = SSCALE;
	MENU_ON( FALSE );
	acc_sc_sol = 1.0;
	dmaflag++;
}

/*
 *			N O T _ S T A T E
 *  
 *  Returns 0 if current state is as desired,
 *  Returns !0 and prints error message if state mismatch.
 */
int
not_state( desired, str )
int desired;
char *str;
{
	if( state != desired ) {
		(void)printf("Unable to do <%s> from %s state.\n",
			str, state_str[state] );
		return(1);	/* BAD */
	}
	return(0);		/* GOOD */
}

/*
 *  			C H G _ S T A T E
 *
 *  Returns 0 if state change is OK,
 *  Returns !0 and prints error message if error.
 */
int
chg_state( from, to, str )
int from, to;
char *str;
{
	if( state != from ) {
		(void)printf("Unable to do <%s> going from %s to %s state.\n",
			str, state_str[from], state_str[to] );
		return(1);	/* BAD */
	}
	state = to;
	/* Advise display manager of state change */
	dmp->dmr_statechange( from, to );
	return(0);		/* GOOD */
}

state_err( str )
char *str;
{
	(void)printf("Unable to do <%s> from %s state.\n",
		str, state_str[state] );
}
