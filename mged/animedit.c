/*
 *			A N I M E D I T . C
 *
 * Process all animation edit commands.
 *
 *  Function -
 *	f_joint	start the animation edit
 *
 *  Author -
 *	Christopher T. Johnson
 *
 *  Source -
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by Geometric Solutions, Inc.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlex.h"
#include "./ged.h"	/* ??? */
#include "./solid.h"
#include "./sedit.h"	/* ??? */
#include "./dm.h"	/* ??? */
#include "./joints.h"

extern int	dmaflag;
extern struct db_i	*dbip;	/* database instance pointer */

static joint_debug = 0;
#define DEBUG_J_MESH	0x00000001
#define DEBUG_J_LOAD	0x00000002
#define DEBUG_J_MOVE	0x00000004
#define DEBUG_J_SOLVE	0x00000008
#define DEBUG_J_EVAL	0x00000010
#define DEBUG_J_SYSTEM	0x00000020
#define DEBUG_J_PARSE	0x00000040
#define DEBUG_J_LEX	0x00000080
#define JOINT_DEBUG_FORMAT \
"\020\10LEX\7PARSE\6SYSTEM\5EVAL\4SOLVE\3MOVE\2LOAD\1MESH"

void joint_move();

static void f_jfhelp();
void f_fhelp2();
static void f_jhelp();
void f_help2();
void f_jmesh();
void f_jdebug();
void f_jload();
void f_junload();
void f_jmove();
void f_jlist();
void f_jaccept();
void f_jreject();
void f_jsave();
void f_jhold();
void f_jsolve();
void f_jtest();

#define MAXARGS	2000
static struct funtab {
	char *ft_name;
	char *ft_parms;
	char *ft_comment;
	void (*ft_func)MGED_ARGS((int, char **));
	int  ft_min;
	int  ft_max;
} joint_tab[] = {

"joint ", "", "Joint command table",
	0, 0, 0,
"?", "[commands]", "summary of available joint commands",
	f_jfhelp, 0, MAXARGS,
"accept", "[joints]", "accept a series of moves",
	f_jaccept, 1, MAXARGS,
"debug", "[hex code]", "Show/set debuging bit vector for joints",
	f_jdebug, 1, 2,
"help", "[commands]", "give usage message for given joint commands",
	f_jhelp, 0, MAXARGS,
"holds","[names]", "list constraints",
	f_jhold, 1, MAXARGS,
"list", "[names]", "list joints.",
	f_jlist, 1, MAXARGS,
"load", "file_name", "load a joint/constraint file",
	f_jload, 2, MAXARGS,
"mesh", "", "Build the grip mesh",
	f_jmesh, 0, 1,
"move", "joint_name p1 [p2...p6]", "Manual adjust a joint",
	f_jmove, 3, 8,
"reject", "[joint_names]", "reject joint motions",
	f_jreject, 1, MAXARGS,
"save",	"file_name", "Save joints and constraints to disk",
	f_jsave, 2, 2,
"solve", "constraint", "Solve a or all constraints",
	f_jsolve, 1, MAXARGS,
"test", "file_name", "test use of rtlex routine.",
	f_jtest, 2, 2,
"unload", "", "Unload any joint/constrants that have been loaded",
	f_junload, 1,1,
0, 0, 0,
	0,0,0
};

#define db_init_full_path(_fp) {\
	(_fp)->fp_len = (_fp)->fp_maxlen = 0; \
	(_fp)->magic = DB_FULL_PATH_MAGIC; }	
void
f_jdebug(argc, argv)
int argc;
char **argv;
{
	if (argc >= 2) {
		sscanf( argv[1], "%x", &joint_debug);
	} else {
		rt_printb( "possible flags", 0xffffffffL, JOINT_DEBUG_FORMAT );
		rt_log("\n");
	}
	rt_printb( "joint_debug", joint_debug, JOINT_DEBUG_FORMAT );
	rt_log("\n");
}
int
f_joint( argc, argv )
int argc;
char **argv;
{
	argc--;
	argv++;

	mged_cmd(argc, argv, &joint_tab[0]);
	return CMD_OK;
}

static void
f_jfhelp(argc, argv)
int argc;
char **argv;
{
	f_fhelp2(argc, argv, &joint_tab[0]);
}

static void
f_jhelp(argc, argv)
int argc;
char **argv;
{
	f_help2(argc, argv, &joint_tab[0]);
}
struct rt_list joint_head = {
	RT_LIST_HEAD_MAGIC,
	&joint_head, &joint_head
};
struct rt_list hold_head = {
	RT_LIST_HEAD_MAGIC,
	&hold_head, &hold_head
};
static struct joint *
joint_lookup(name)
char *name;
{
	register struct joint *jp;

	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		if (strcmp(jp->name, name) == 0) return jp;
	}
	return (struct joint *) 0;
}
static void
free_arc(ap)
struct arc *ap;
{
	register int i;
	if (!ap || ap->type == ARC_UNSET) return;
	for (i=0; i<=ap->arc_last; i++) {
		rt_free(ap->arc[i], "arc entry");
	}
	rt_free((char *)ap->arc, "arc table");
	ap->arc = (char **)0;
	if (ap->type & ARC_BOTH) {
		for (i=0; i<=ap->arc_last; i++) {
			rt_free(ap->original[i], "arc entry");
		}
		rt_free((char *)ap->original, "arc table");
	}
	ap->type=ARC_UNSET;
}
static void
free_joint(jp)
struct joint *jp;
{
	free_arc(&jp->path);
	if (jp->name) rt_free(jp->name, "joint name");
	rt_free((char *)jp, "joint structure");
}

static void
free_hold(hp)
struct hold *hp;
{
	register struct jointH *jh;

	if (!hp || hp->l.magic != MAGIC_HOLD_STRUCT) return;
	if (hp->objective.type != ID_FIXED) {
		if (hp->objective.path.fp_maxlen) {
			db_free_full_path(&hp->objective.path);
		}
		free_arc(&hp->objective.arc);
	}
	if (hp->effector.type != ID_FIXED) {
		if (hp->effector.path.fp_maxlen) {
			db_free_full_path(&hp->effector.path);
		}
		free_arc(&hp->effector.arc);
	}
	while (RT_LIST_WHILE(jh, jointH, &hp->j_head)) {
		jh->p->uses--;
		RT_LIST_DEQUEUE(&jh->l);
		rt_free((char *) jh, "joint handle");
	}
	if (hp->joint) rt_free(hp->joint, "hold joint name");
	if (hp->name) rt_free(hp->name, "hold name");
	rt_free((char *)hp, "hold struct");
}
static void
hold_clear_flags(hp)
struct hold *hp;
{
	hp->effector.flag = hp->objective.flag = 0;
}
#if 0
static
read_hold_point(pp, name, fip)
struct hold_point *pp;
char *name;
FILE *fip;
{
	char			**arc;
	int			arc_last;
	char			text[TEXT_LEN];
	register int 		i;
	struct directory	*dp;
	struct joint		*jp;

	if (!get_line(text, TEXT_LEN, fip)) {
		rt_log("joint load constraint: unable to read %s type\n",
		    name);
		return 0;
	}
	pp->type = atoi(text);
	if (pp->type == 1) pp->type = ID_GRIP;
	if (joint_debug & DEBUG_J_LOAD) {
		static char *names[] = {
			"FIXED",
			"GRIP",
			"JOINT",
			"UNKNOWN"};
		int t = pp->type;
		if (t < 0 || t > 3) t = 3;
		rt_log("joint load point: Type is %s\n", names[t]);
	}
	switch (pp->type) {
	case HOLD_PT_FIXED:
		if (!get_line(text, TEXT_LEN, fip)) {
			rt_log("joint load constraint: unable to read %s 3space point\n",
			    name);
			return 0;
		}

		if (sscanf(text,"%lg %lg %lg", &pp->point[X], &pp->point[Y],
		    &pp->point[Z]) != 3) {
		    	rt_log("joint load constraint: unable to read points for %s.\n",
		    	    name);
		    	return 0;
		}
		if (joint_debug & DEBUG_J_LOAD) {
			rt_log("joint load point: (%g %g %g)\n", pp->point[X],
			    pp->point[Y], pp->point[Z]);
		}
		return 1;
	case ID_GRIP:
/*
 * XXX - Final method will be top grip_name.  For now, we will just use "arc"
 */
		if (!get_line(text, TEXT_LEN, fip)) {
			rt_log("joint load constraint: unable to read %s arc.\n",
			    name);
			return 0;
		}
		arc = parse_arc(&arc_last, text);
		/*
		 * we are now going to build a full_path from the arc.
		 */
		pp->path.fp_len=pp->path.fp_maxlen = arc_last+1;
		pp->path.fp_names = (struct directory **)
		    rt_malloc(sizeof(struct directory **)*pp->path.fp_maxlen,
		    "db full path");
		for (i=0; i<=arc_last; i++) {
			dp = db_lookup(dbip, arc[i], LOOKUP_NOISY);
			if (!dp) break;
			pp->path.fp_names[i] = dp;
		}
		if (!dp) {
			rt_log("joint load constraint: arc '%s' for %s is bad.\n",
			    text, name);
			for (i=0; i<=arc_last; i++) {
				rt_free(arc[i], "arc entry");
			}
			rt_free((char *) arc, "arc table");
			return 0;
		}
		pp->name = arc[arc_last];
		/*
		 * NOTE: we are not freeing the last entry which should be
		 * a grip as we save it's name in pp->name.
		 */
		for (i=0; i< arc_last; i++) {
			rt_free(arc[i], "arc entry");
		}
		rt_free((char *) arc, "arc table");
		return 1;
	case HOLD_PT_JOINT:
/*
 * XXX - Final method will be top joint_name.  For now, we will just use "arc"
 */
		if (!get_line(text, TEXT_LEN, fip)) {
			rt_log("joint load constraint: unable to read %s arc.\n",
			    name);
			return 0;
		}
		arc = parse_arc(&arc_last, text);
		/*
		 * we are now going to build a full_path from the arc.
		 */
		pp->path.fp_len=pp->path.fp_maxlen = arc_last+1;
		pp->path.fp_names = (struct directory **)
		    rt_malloc(sizeof(struct directory **)*pp->path.fp_maxlen,
		    "db full path");
		/*
		 * Except for the last entry which should be a joint name.
		 */
		for (i=0; i<arc_last; i++) {
			dp = db_lookup(dbip, arc[i], LOOKUP_NOISY);
			if (!dp) break;
			pp->path.fp_names[i] = dp;
		}
		if (dp) {
			jp = joint_lookup(arc[arc_last]);
			if (jp) {
				dp = db_lookup(dbip, jp->arc[jp->arc_last],
				    LOOKUP_NOISY);
				if (dp) {
					pp->path.fp_names[i] = dp;
				}
			} else {
				rt_log("joint load constraint: %s gave bad joint name.\n",
				    name);
				dp = 0;
			}
		}
		if (!dp) {
			rt_log("joint load constraint: arc '%s' for %s is bad.\n",
			    text, name);
			for (i=0; i<arc_last; i++) {
				rt_free(arc[i], "arc entry");
			}
			rt_free((char *) arc, "arc table");
			return 0;
		}
		pp->name = arc[arc_last];
		/*
		 * NOTE: we are not freeing the last entry which should be
		 * a joint as we save it's name in pp->name.
		 */
		for (i=0; i< arc_last; i++) {
			rt_free(arc[i], "arc entry");
		}
		rt_free((char *) arc, "arc table");
		return 1;
	default:
		rt_log("joint load constrain: Bad type (%d) for %s.\n",
		    pp->type, name);
		pp->type = HOLD_PT_FIXED;
		return 0;
	}
	/*NEVERREACHED*/
}
#endif /* 0 */
#if 0
	if (!read_hold_point(&hp->effector,"effector", fip)) {
		free_hold(hp);
		return (struct hold*)0;
	}
	/*
	 * objective.
	 */
	if (!read_hold_point(&hp->objective, "objective", fip)) {
		free_hold(hp);
		return (struct hold *)0;
	}
#endif /* 0 */

void
f_junload(argc, argv)
int argc;
char **argv;
{
	register struct joint *jp;
	register struct hold *hp;
	int joints, holds;

	db_free_anim(dbip);
	holds = 0;
	while (RT_LIST_WHILE(hp, hold, &hold_head)) {
		holds++;
		RT_LIST_DEQUEUE(&hp->l);
		free_hold(hp);
	}
	joints = 0;
	while (RT_LIST_WHILE(jp, joint, &joint_head)) {
		joints++;
		RT_LIST_DEQUEUE(&(jp->l));
		if (joint_debug & DEBUG_J_LOAD) {
			rt_log("joint unload: unloading '%s'.\n", jp->name);
		}
		free_joint(jp);
	}
	if (joint_debug & DEBUG_J_LOAD) {
		rt_log("joint unload: unloaded %d joints, %d constraints.\n",
		    joints, holds);
	}
}
#define	KEY_JOINT	1
#define KEY_CON		2
#define	KEY_ARC		3
#define KEY_LOC		4
#define KEY_TRANS	5
#define KEY_ROT		6
#define KEY_LIMIT	7
#define KEY_UP		8
#define KEY_LOW		9
#define KEY_CUR		10
#define KEY_ACC		11
#define KEY_DIR		12
#define KEY_UNITS	13
#define KEY_JOINTS	14
#define KEY_START	15
#define KEY_PATH	16
#define KEY_WEIGHT	17
#define KEY_PRI		18
#define KEY_EFF		19
#define KEY_POINT	20
#define KEY_EXCEPT	21
#define KEY_INF		22
#define KEY_VERTEX	23

static struct rt_lex_key keys[] = {
	{KEY_JOINT, "joint"},
	{KEY_CON,   "constraint"},
	{KEY_ARC,   "arc"},
	{KEY_LOC,   "location"},
	{KEY_TRANS, "translate"},
	{KEY_ROT,   "rotate"},
	{KEY_LIMIT, "limits"},
	{KEY_UP,    "upper"},
	{KEY_LOW,   "lower"},
	{KEY_CUR,   "current"},
	{KEY_ACC,   "accepted"},
	{KEY_DIR,   "direction"},
	{KEY_UNITS, "units"},
	{KEY_JOINTS,"joints"},
	{KEY_START, "start"},
	{KEY_PATH,  "path"},
	{KEY_WEIGHT,"weight"},
	{KEY_PRI,   "priority"},
	{KEY_EFF,   "effector"},
	{KEY_POINT, "point"},
	{KEY_EXCEPT,"except"},
	{KEY_INF,   "INF"},
	{KEY_VERTEX,"vertex"},
	{0,0}};

#define UNIT_INCH	1
#define UNIT_METER	2
#define UNIT_FEET	3
#define UNIT_CM		4
#define UNIT_MM		5

static struct rt_lex_key units[] = {
	{UNIT_INCH,	"inches"},
	{UNIT_INCH,	"in"},
	{UNIT_METER,	"m"},
	{UNIT_METER,	"meters"},
	{UNIT_FEET,	"ft"},
	{UNIT_FEET,	"feet"},
	{UNIT_CM,	"cm"},
	{UNIT_MM,	"mm"},
	{0,0}};

#define ID_FIXED	-1
static struct rt_lex_key lex_solids[] = {
	{ID_FIXED,	"fixed"},
	{ID_GRIP,	"grip"},
	{ID_SPH,	"sphere"},
	{ID_JOINT,	"joint"},
	{0,0}};

#define SYM_OP_GROUP	1
#define SYM_CL_GROUP	2
#define SYM_OP_PT	3
#define SYM_CL_PT	4
#define SYM_EQ		5
#define SYM_ARC		6
#define SYM_END		7
#define SYM_COMMA	8
#define SYM_MINUS	9
#define SYM_PLUS	10

static struct rt_lex_key syms[] = {
	{SYM_OP_GROUP,	"{"},
	{SYM_CL_GROUP,	"}"},
	{SYM_OP_PT,	"("},
	{SYM_CL_PT,	")"},
	{SYM_EQ,	"="},
	{SYM_ARC,	"/"},
	{SYM_END,	";"},
	{SYM_COMMA,	","},
	{SYM_MINUS,	"-"},
	{SYM_PLUS,	"+"},
	{0,0}};

static int lex_line;
static char *lex_name;
static double mm2base, base2mm;

static void
parse_error(str, error)
struct rt_vls *str;
char *error;
{
	char *text;
	int i;

	if (!str->vls_str) {
		rt_log("%s:%d %s\n",lex_name, lex_line,error);
		return;
	}
	text = rt_malloc(str->vls_offset+2, "error pointer");
	for (i=0; i<str->vls_offset; i++) {
		text[i]=(str->vls_str[i] == '\t')? '\t' : '-';
	}
	text[str->vls_offset] = '^';
	text[str->vls_offset+1] = '\0';

	rt_log("%s:%d %s\n%s\n%s\n",lex_name, lex_line,error,str->vls_str,text);
	rt_free(text, "error pointer");
}

int
get_token(token, fip, str, keys, syms)
union rt_lex_token *token;
FILE *fip;
struct rt_vls *str;
struct rt_lex_key *keys;
struct rt_lex_key *syms;
{
	int	used;
	for (;;) {
		used = rt_lex(token, str, keys, syms);
		if (used) break;
		rt_vls_free(str);
		lex_line++;
		used = rt_vls_gets(str,fip);
		if (used == EOF) return used;
	}

	rt_vls_nibble(str, used);

	if (joint_debug & DEBUG_J_LEX) {
		register int i;
		switch (token->type) {
		case RT_LEX_KEYWORD:
			for (i=0; keys[i].tok_val != token->t_key.value; i++);
			rt_log("lex: key(%d)='%s'\n", token->t_key.value,
			    keys[i].string);
			break;
		case RT_LEX_SYMBOL:
			for (i=0; syms[i].tok_val != token->t_key.value; i++);
			rt_log("lex: symbol(%d)='%c'\n", token->t_key.value,
			    *(syms[i].string));
			break;
		case RT_LEX_DOUBLE:
			rt_log("lex: double(%g)\n", token->t_dbl.value);
			break;
		case RT_LEX_INT:
			rt_log("lex: int(%d)\n", token->t_int.value);
			break;
		case RT_LEX_IDENT:
			rt_log("lex: id(%s)\n", token->t_id.value);
			break;
		}
	}

	return used;
}
static int
gobble_token(type_wanted, value_wanted, fip, str)
int type_wanted;
int value_wanted;
FILE *fip;
struct rt_vls *str;
{
	static char *types[] = {
		"any",
		"integer",
		"double",
		"symbol",
		"keyword",
		"identifier",
		"number"};
	union rt_lex_token token;
	char error[160];

	if (get_token(&token, fip, str, keys, syms) == EOF) {
		sprintf(error,"parse: Unexpected EOF while getting %s",
		    types[type_wanted]);
		parse_error(str, error);
		return 0;
	}

	if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");

	switch (type_wanted) {
	case RT_LEX_ANY:
		 return 1;
	case RT_LEX_INT:
		return (token.type == RT_LEX_INT);
	case RT_LEX_DOUBLE:
		return (token.type == RT_LEX_DOUBLE);
	case RT_LEX_NUMBER:
		return (token.type == RT_LEX_INT || token.type == RT_LEX_DOUBLE);
	case RT_LEX_SYMBOL:
		return (token.type == RT_LEX_SYMBOL &&
		    value_wanted == token.t_key.value);
	case RT_LEX_KEYWORD:
		return (token.type == RT_LEX_KEYWORD &&
		    value_wanted == token.t_key.value);
	case RT_LEX_IDENT:
		return (token.type == RT_LEX_IDENT);
	}
	return 0;
}
static void
skip_group(fip, str)
FILE * fip;
struct rt_vls *str;
{
	union rt_lex_token tok;
	int count = 1;
	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("skip_group: Skipping....\n");
	}

	while (count) {
		if (get_token(&tok, fip, str, keys,syms) == EOF) {
			parse_error(str,"skip_group: Unexpect EOF while searching for group end.");
			return;
		}
		if (tok.type == RT_LEX_IDENT) rt_free(tok.t_id.value, "unit token");
		if (tok.type != RT_LEX_SYMBOL) continue;
		if (tok.t_key.value == SYM_OP_GROUP) {
			count++;
		} else if (tok.t_key.value == SYM_CL_GROUP) {
			count--;
		}
	}
	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("skip_group: Done....\n");
	}

}
static int
parse_units(fip, str)
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;

	if (get_token(&token,fip, str, units, syms) == EOF) {
		parse_error(str, "parse_units: Unexpect EOF reading units.");
		return 0;
	}
	if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
	if (token.type != RT_LEX_KEYWORD) {
		parse_error(str, "parse_units: syntax error getting unit type.");
		return 0;
	}

	switch (token.t_key.value) {
	case UNIT_INCH:
		base2mm = 25.4; break;
	case UNIT_METER:
		base2mm = 1000.0; break;
	case UNIT_FEET:
		base2mm = 304.8; break;
	case UNIT_CM:
		base2mm = 10.0; break;
	case UNIT_MM:
		base2mm = 1; break;
	}
	mm2base = 1.0 / base2mm;
	(void) gobble_token(RT_LEX_SYMBOL, SYM_END, fip,str);
	return 1;
}
static int
parse_path(ap, fip, str)
struct arc *ap;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int max;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_path: open.\n");
	}
	/*
 	 * clear the arc if there is anything there.
	 */
	free_arc(ap);
	if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) return 0;
	max = 32;
	ap->arc = (char **)rt_malloc(sizeof(char *)*max, "arc table");
	ap->arc_last = -1;
	ap->type = ARC_PATH;
	for (;;) {
		if (get_token(&token, fip, str, 0, syms) == EOF) {
			parse_error(str,"parse_path: Unexpect EOF.");
			free_arc(ap);
			return 0;
		}
		if (token.type != RT_LEX_IDENT) {
			parse_error(str,"parse_path: syntax error. Missing identifier.");
			free_arc(ap);
			return 0;
		}
		if (++ap->arc_last >= max) {
			max +=32;
			ap->arc = (char **) rt_realloc((char *) ap->arc,
			    sizeof(char *)*max, "arc table");
		}
		ap->arc[ap->arc_last] = token.t_id.value;
		if (get_token(&token, fip, str, 0, syms) == EOF) {
			parse_error(str, "parse_path: Unexpect EOF while getting '/' or '-'");
			free_arc(ap);
			return 0;
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
		if (token.type != RT_LEX_SYMBOL) {
			parse_error(str, "parse_path: syntax error.");
			free_arc(ap);
			return 0;
		}
		if (token.t_key.value == SYM_ARC) {
			continue;
		}else if (token.t_key.value == SYM_MINUS) {
			break;
		} else {
			parse_error(str,"parse_path: syntax error.");
			free_arc(ap);
			return 0;
		}
	}
	/*
	 * Just got the '-' so this is the "destination" part.
	 */
	if (get_token(&token, fip, str, 0, syms) == EOF) {
		parse_error(str,"parse_path: Unexpected EOF while getting destination.");
		free_arc(ap);
		return 0;
	}
	if (token.type != RT_LEX_IDENT) {
		parse_error(str,"parse_path: syntax error, expecting destination.");
		free_arc(ap);
		return 0;
	}
	if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str)) {
		free_arc(ap);
		return 0;
	}
	return 1;
}
static int
parse_list(ap, fip, str)
struct arc *ap;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int max;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_path: open.\n");
	}
	/*
 	 * clear the arc if there is anything there.
	 */
	free_arc(ap);

	if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) return 0;
	max = 32;
	ap->arc = (char **)rt_malloc(sizeof(char *)*max, "arc table");
	ap->arc_last = -1;
	ap->type = ARC_LIST;
	for (;;) {
		if (get_token(&token, fip, str, 0, syms) == EOF) {
			parse_error(str,"parse_path: Unexpect EOF.");
			free_arc(ap);
			return 0;
		}
		if (token.type != RT_LEX_IDENT) {
			parse_error(str,"parse_path: syntax error. Missing identifier.");
			free_arc(ap);
			return 0;
		}
		if (++ap->arc_last >= max) {
			max +=32;
			ap->arc = (char **) rt_realloc((char *) ap->arc,
			    sizeof(char *)*max, "arc table");
		}
		ap->arc[ap->arc_last] = token.t_id.value;
		if (get_token(&token, fip, str, 0, syms) == EOF) {
			parse_error(str, "parse_path: Unexpect EOF while getting ',' or ';'");
			free_arc(ap);
			return 0;
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
		if (token.type != RT_LEX_SYMBOL) {
			parse_error(str, "parse_path: syntax error.");
			free_arc(ap);
			return 0;
		}
		if (token.t_key.value == SYM_COMMA) {
			continue;
		}else if (token.t_key.value == SYM_END) {
			break;
		} else {
			parse_error(str,"parse_path: syntax error.");
			free_arc(ap);
			return 0;
		}
	}
	return 1;
}
static int
parse_ARC(ap, fip, str)
struct arc *ap;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int max;
	char *error;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_ARC: open.\n");
	}

	free_arc(ap);
	max = 32;
	if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) return 0;

	ap->arc = (char **) rt_malloc(sizeof(char *)*max, "arc table");
	ap->arc_last = -1;

	error = "parse_ARC: Unexpected EOF while getting arc.";
	while (get_token(&token, fip, str, 0, syms) != EOF) {
		if (token.type != RT_LEX_IDENT) {
			error = "parse_ARC: syntax error. Missing identifier.";
			break;
		}
		if (++ap->arc_last >= max) {
			max+=32;
			ap->arc = (char **) rt_realloc((char *)ap->arc,
			    sizeof(char *)*max, "arc table");
		}
		ap->arc[ap->arc_last] = token.t_id.value;
		if (get_token(&token, fip, str, 0, syms) == EOF) {
			error = "parse_ARC: Unexpected EOF while getting '/' or ';'.";
			break;
		}
		if (token.type != RT_LEX_SYMBOL) {
			if (token.type == RT_LEX_IDENT) {
				rt_free(token.t_id.value, "unit token");
			}
			error = "parse_ARC: syntax error.  Expected '/' or ';'";
			break;
		}
		if (token.t_key.value == SYM_END) {
			if (joint_debug & DEBUG_J_PARSE) {
				rt_log("parse_ARC: close.\n");
			}

			return 1;
		}
		if (token.t_key.value != SYM_ARC) {
			error = "parse_ARC: Syntax error.  Expecting ';' or '/'";
			break;
		}
		error = "parse_ARC: Unexpected EOF while getting arc.";
	}
	parse_error(str, error);
	free_arc(ap);
	return 0;
}
static int
parse_double(dbl, fip, str)
double *dbl;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	double sign;
	sign = 1.0;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_double: open\n");
	}

	if (get_token(&token, fip, str, keys, syms) == EOF) {
		parse_error(str, "parse_double: Unexpect EOF while getting number.");
		return 0;
	}
	if (token.type == RT_LEX_SYMBOL && token.t_key.value == SYM_MINUS) {
		sign = -1;
		if (get_token(&token, fip, str, keys, syms) == EOF) {
			parse_error(str,"parse_double: Unexpect EOF while getting number.");
			return 0;
		}
	}
	if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");

	if (token.type == RT_LEX_INT) {
		*dbl = token.t_int.value * sign;
	} else if (token.type == RT_LEX_DOUBLE) {
		*dbl = token.t_dbl.value * sign;
	} else if (token.type == RT_LEX_KEYWORD && token.t_key.value == KEY_INF) {
		*dbl = MAX_FASTF * sign;
	} else {
		parse_error(str,"parse_double: syntax error.  Expecting number.");
		return 0;
	}

	return 1;
}
static int
parse_assign(dbl, fip, str)
double *dbl;
FILE *fip;
struct rt_vls *str;
{
	if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
		skip_group(fip, str);
		return 0;
	}
	if (!parse_double(dbl, fip, str)) {
		skip_group(fip, str);
		return 0;
	}
	if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str)) {
		skip_group(fip, str);
		return 0;
	}
	return 1;
}
static int
parse_vect(vect, fip, str)
vect_t vect;
FILE *fip;
struct rt_vls *str;
{
	int i;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_vect: open.\n");
	}

	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_PT, fip, str)) return 0;
	for (i=0; i < 3; i++) {
		if (!parse_double(&vect[i], fip, str)) return 0;
		if (i < 2) {
			if (!gobble_token(RT_LEX_SYMBOL, SYM_COMMA, fip, str)) return 0;
		} else {
			if (!gobble_token(RT_LEX_SYMBOL, SYM_CL_PT, fip, str)) return 0;
		}
	}
	return 1;
}
static int
parse_trans(jp, index, fip, str)
struct joint *jp;
int index;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int dirfound=0, upfound = 0, lowfound=0, curfound=0, accfound=0;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_trans: open\n");
	}

	if (index >= 3) {
		parse_error(str,"parse_trans: To many translations for this joint.");
		if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0 ;
		skip_group(fip,str);
		return 0;
	}
	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0;

	while (get_token(&token, fip, str, keys, syms) != EOF) {
		if (token.type == RT_LEX_IDENT) {
			rt_free(token.t_id.value, "unit token");
		}
		if (token.type == RT_LEX_SYMBOL && 
		    token.t_key.value == SYM_CL_GROUP ) {
		    	if (joint_debug & DEBUG_J_PARSE) {
		    		rt_log("parse_trans: closing.\n");
		    	}

		    	if (!dirfound) {
		    		parse_error(str,"parse_trans: Direction vector not given.");
		    		return 0;
		    	}
		    	VUNITIZE(jp->dirs[index].unitvec);
		    	if (!lowfound) {
		    		parse_error(str,"parse_trans: lower bound not given.");
		    		return 0;
		    	}
		    	if (!upfound) {
		    		parse_error(str,"parse_trans: upper bound not given.");
		    		return 0;
		    	}
		    	if (jp->dirs[index].lower > jp->dirs[index].upper) {
		    		double tmp;
		    		tmp = jp->dirs[index].lower;
		    		jp->dirs[index].lower = jp->dirs[index].upper;
		    		jp->dirs[index].upper = tmp;
		    		parse_error(str,"parse_trans: lower > upper, exchanging.");
		    	}
		    	if (!accfound) jp->dirs[index].accepted = 0.0;
		    	if (!curfound) jp->dirs[index].current = 0.0;
			jp->dirs[index].lower *= base2mm;
		    	jp->dirs[index].upper *= base2mm;
		    	jp->dirs[index].current *= base2mm;
		    	jp->dirs[index].accepted *= base2mm;

		    	if (jp->dirs[index].accepted < jp->dirs[index].lower) {
		    		jp->dirs[index].accepted = jp->dirs[index].lower;
		    	}
		    	if (jp->dirs[index].accepted > jp->dirs[index].upper) {
		    		jp->dirs[index].accepted = jp->dirs[index].upper;
		    	}
		    	if (jp->dirs[index].current < jp->dirs[index].lower) {
		    		jp->dirs[index].current = jp->dirs[index].lower;
		    	}
		    	if (jp->dirs[index].current > jp->dirs[index].upper) {
		    		jp->dirs[index].current = jp->dirs[index].upper;
		    	}
		    	return 1;
		}

		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str,"parse_trans: Syntax error.");
			skip_group(fip,str);
			return 0;
		}
		switch (token.t_key.value) {
		case KEY_LIMIT:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->dirs[index].lower, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			lowfound = 1;
			if (!gobble_token(RT_LEX_SYMBOL, SYM_COMMA, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->dirs[index].upper, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			upfound = 1;
			if (!gobble_token(RT_LEX_SYMBOL, SYM_COMMA, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->dirs[index].current, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			curfound = 1;
			(void) gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str);
			break;
		case KEY_UP:
			if (!parse_assign(&jp->dirs[index].upper, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			upfound = 1;
			break;
		case KEY_LOW:
			if (!parse_assign(&jp->dirs[index].lower, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			lowfound = 1;
			break;
		case KEY_CUR:
			if (!parse_assign(&jp->dirs[index].current, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			curfound = 1;
			break;
		case KEY_ACC:
			if (!parse_assign(&jp->dirs[index].accepted, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			curfound = 1;
			break;
		case KEY_DIR:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_vect(jp->dirs[index].unitvec, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			dirfound = 1;
			break;
		default:
			parse_error(str,"parse_trans: syntax error.");
			skip_group(fip, str);
			return 0;
		}
	}
	parse_error(str,"parse_trans:Unexpected EOF.");
	return 0;
}
static int
parse_rots(jp, index, fip, str)
struct joint *jp;
int index;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int dirfound=0, upfound = 0, lowfound=0, curfound=0, accfound=0;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_rots: open\n");
	}

	if (index >= 3) {
		parse_error(str,"parse_rot: To many rotations for this joint.");
		if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0 ;
		skip_group(fip,str);
		return 0;
	}
	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0;

	while (get_token(&token, fip, str, keys, syms) != EOF) {
		if (token.type == RT_LEX_IDENT) {
			rt_free(token.t_id.value, "unit token");
		}
		if (token.type == RT_LEX_SYMBOL && 
		    token.t_key.value == SYM_CL_GROUP ) {
		    	if (joint_debug & DEBUG_J_PARSE) {
		    		rt_log("parse_rots: closing.\n");
		    	}

		    	if (!dirfound) {
		    		parse_error(str,"parse_rots: Direction vector not given.");
		    		return 0;
		    	}
		    	VUNITIZE(jp->rots[index].quat);
		    	jp->rots[index].quat[W] = 0.0;
		    	if (!lowfound) {
		    		parse_error(str,"parse_rots: lower bound not given.");
		    		return 0;
		    	}
		    	if (!upfound) {
		    		parse_error(str,"parse_rots: upper bound not given.");
		    		return 0;
		    	}
		    	if (jp->rots[index].lower > jp->rots[index].upper) {
		    		double tmp;
		    		tmp = jp->rots[index].lower;
		    		jp->rots[index].lower = jp->rots[index].upper;
		    		jp->rots[index].upper = tmp;
		    		parse_error(str,"parse_rots: lower > upper, exchanging.");
		    	}
		    	if (!accfound) {
		    		jp->rots[index].accepted = 0.0;
		    	}
		    	if (jp->rots[index].accepted < jp->rots[index].lower) {
		    		jp->rots[index].accepted = jp->rots[index].lower;
		    	}
		    	if (jp->rots[index].accepted > jp->rots[index].upper) {
		    		jp->rots[index].accepted = jp->rots[index].upper;
		    	}
		    	if (!curfound) {
		    		jp->rots[index].current = 0.0;
		    	}
		    	if (jp->rots[index].current < jp->rots[index].lower) {
		    		jp->rots[index].current = jp->rots[index].lower;
		    	}
		    	if (jp->rots[index].current > jp->rots[index].upper) {
		    		jp->rots[index].current = jp->rots[index].upper;
		    	}
		    	return 1;
		}

		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str,"parse_rots: Syntax error.");
			skip_group(fip,str);
			return 0;
		}
		switch (token.t_key.value) {
		case KEY_LIMIT:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->rots[index].lower, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			lowfound = 1;
			if (!gobble_token(RT_LEX_SYMBOL, SYM_COMMA, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->rots[index].upper, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			upfound = 1;
			if (!gobble_token(RT_LEX_SYMBOL, SYM_COMMA, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_double(&jp->rots[index].current, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			curfound = 1;
			(void) gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str);
			break;
		case KEY_UP:
			if (!parse_assign(&jp->rots[index].upper, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			upfound = 1;
			break;
		case KEY_LOW:
			if (!parse_assign(&jp->rots[index].lower, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			lowfound = 1;
			break;
		case KEY_CUR:
			if (!parse_assign(&jp->rots[index].current, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			curfound = 1;
			break;
		case KEY_ACC:
			if (!parse_assign(&jp->rots[index].accepted, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			curfound = 1;
			break;
		case KEY_DIR:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!parse_vect(jp->rots[index].quat, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			dirfound = 1;
			break;
		default:
			parse_error(str,"parse_rots: syntax error.");
			skip_group(fip, str);
			return 0;
		}
	}
	parse_error(str,"parse_rots:Unexpected EOF.");
	return 0;
}
static int
parse_joint(fip, str)
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	struct joint *jp;
	int trans;
	int rots;
	int arcfound, locfound;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_joint: reading joint.\n");
	}

	GETSTRUCT(jp, joint);
	jp->l.magic = MAGIC_JOINT_STRUCT;
	jp->anim = (struct animate *) 0;
	jp->path.type = ARC_UNSET;
	jp->name = NULL;

	if (get_token(&token, fip, str, 0, syms) == EOF) {
		parse_error(str,"parse_joint: Unexpected EOF getting name.");
		free_joint(jp);
		return 0;
	}
	jp->name = token.t_id.value;	/* Name */
	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) {
		free_joint(jp);
		return 0;
	}
	/*
	 * With in the group, we need at least one rotate or translate,
	 * a location and an arc or path.
	 */
	arcfound = 0;
	locfound = 0;
	rots = trans = 0;
	for(;;) {
		if (get_token(&token, fip, str, keys, syms) == EOF) {
			parse_error(str,"parse_joint: Unexpected EOF getting joint contents.");
			skip_group(fip, str);
			free_joint(jp);
			return 0;
		}
		if (token.type == RT_LEX_SYMBOL &&
		    token.t_key.value == SYM_CL_GROUP) {
		    	if (joint_debug & DEBUG_J_PARSE) {
		    		rt_log("parse_joint: closing.\n");
		    	}
		    	if (!arcfound) {
		    		parse_error(str,"parse_joint: Arc not defined.");
		    		free_joint(jp);
		    		return 0;
		    	}
		    	if (!locfound) {
		    		parse_error(str,"parse_joint: location not defined.");
		    		free_joint(jp);
		    		return 0;
		    	}
		    	if (trans + rots == 0) {
		    		parse_error(str,"parse_joint: no translations or rotations defined.");
		    		free_joint(jp);
		    		return 0;
		    	}
		    	for(;trans<3;trans++) {
		    		jp->dirs[trans].lower = -1.0;
		    		jp->dirs[trans].upper = -2.0;
		    		jp->dirs[trans].current = 0.0;
		    	}
		    	for(;rots<3;rots++) {
		    		jp->rots[rots].lower = -1.0;
		    		jp->rots[rots].upper = -2.0;
		    		jp->rots[rots].current = 0.0;
		    	}
		    	jp->location[X] *= base2mm;
		    	jp->location[Y] *= base2mm;
		    	jp->location[Z] *= base2mm;

		    	RT_LIST_INSERT(&joint_head, &(jp->l));
		    	gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str);
		    	return 1;
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");

		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str,"parse_joint: syntax error.");
			skip_group(fip, str);
			free_joint(jp);
			return 0;
		}
		switch (token.t_key.value) {
		case KEY_ARC: 
			if (arcfound) {
				parse_error(str,"parse_joint: more than one arc or path given");
			}
			if (!parse_ARC(&jp->path, fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			arcfound = 1;
			break;
		case KEY_PATH:
			if (arcfound) {
				parse_error(str,"parse_joint: more than one arc or path given.");
			}
			if (!parse_path(&jp->path, fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			arcfound = 1;
			break;
		case KEY_LOC: 
			if (locfound) {
				parse_error(str,"parse_joint: more than one location given.");
			}
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			if (!parse_vect(&jp->location[0], fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			locfound=1;
			break;
		case KEY_TRANS:
			if (!parse_trans(jp, trans, fip, str)) {
				skip_group(fip,str);
				free_joint(jp);
				return 0;
			}
			trans++;
			break;
		case KEY_ROT:
			if (!parse_rots(jp, rots, fip, str)) {
				skip_group(fip, str);
				free_joint(jp);
				return 0;
			}
			rots++;
			break;
		default:
			parse_error(str,"parse_joint: syntax error.");
			skip_group(fip, str);
			free_joint(jp);
			return 0;
		}
	}
	/* NOTREACHED */
}
static int
parse_jset(hp,fip, str)
struct hold *hp;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int jointfound=0, listfound=0, arcfound=0, pathfound=0;

	if(joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_jset: open\n");
	}

	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0;

	for (;;) {
		if (get_token(&token, fip, str, keys, syms) == EOF) {
			parse_error(str, "parse_jset: Unexpect EOF getting contents of joint set");
			return 0;
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
		if (token.type == RT_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
			if (!jointfound) hp->j_set.joint = 0;
			if (!listfound && !arcfound && !pathfound) {
				parse_error(str, "parse_jset: no list/arc/path given.");
				return 0;
			}
			if(joint_debug & DEBUG_J_PARSE) {
				rt_log("parse_jset: close\n");
			}
 			return 1;
		}
		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str, "parse_jset: syntax error.");
			return 0;
		}
		switch (token.t_key.value) {
		case KEY_START:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			if (get_token(&token, fip, str, keys, syms) == EOF) {
				parse_error(str,"parse_jset: Unexpect EOF getting '='");
				return 0;
			}
			if (token.type != RT_LEX_IDENT) {
				parse_error(str,"parse_jset: syntax error, expecting joint name.");
				skip_group(fip,str);
				return 0;
			}
			hp->j_set.joint = token.t_id.value;
			if (!gobble_token(RT_LEX_SYMBOL, SYM_END, fip,str)) {
				skip_group(fip,str);
				return 0;
			}
			jointfound = 1;
			break;
		case KEY_ARC:
			if (!parse_ARC(&hp->j_set.path, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			arcfound = 1;
			break;
		case KEY_PATH:
			if (!parse_path(&hp->j_set.path, fip, str)) {
				skip_group(fip, str);
				return 0;
			}
			pathfound = 1;
			break;
		case KEY_JOINTS:
			if (!parse_list(&hp->j_set.path, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			listfound=1;
			break;
		case KEY_EXCEPT:
			if (!parse_list(&hp->j_set.exclude, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			break;
		default:
			parse_error(str,"parse_jset: syntax error.");
			skip_group(fip,str);
			return 0;
		}
	}
}
static int
parse_solid(pp, fip, str)
struct hold_point *pp;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;
	int vertexfound = 0, arcfound = 0;
	double vertex;

	if(joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_solid: open\n");
	}

	if (!gobble_token(RT_LEX_SYMBOL, SYM_OP_GROUP, fip, str)) return 0;

	for (;;) {
		if (get_token(&token, fip, str, keys, syms) == EOF) {
			parse_error(str, "parse_solid: Unexpect EOF.");
			return 0;
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
		if (token.type == RT_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
			if (!arcfound) {
				parse_error(str,"parse_solid: path/arc missing.");
				return 0;
			}
			if (!vertexfound) pp->vertex_number = 1;
			if(joint_debug & DEBUG_J_PARSE) {
				rt_log("parse_solid: close\n");
			}

			return 1;
		}
		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str, "parse_solid: syntax error getting solid information.");
			skip_group(fip,str);
			return 0;
		}
		switch (token.t_key.value) {
		case KEY_VERTEX:
			if (!parse_assign(&vertex, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			pp->vertex_number = vertex;	/* double to int */
			vertexfound = 1;
			break;
		case KEY_PATH:
			if (!parse_path(&pp->arc, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			arcfound = 1;
			break;
		case KEY_ARC:
			if (!parse_ARC(&pp->arc, fip, str)) {
				skip_group(fip,str);
				return 0;
			}
			arcfound =1 ;
			break;
		default:
			parse_error(str, "parse_solid: syntax error.");
		}
	}
}
static int
parse_point(pp, fip, str)
struct hold_point *pp;
FILE *fip;
struct rt_vls *str;
{
	union rt_lex_token token;

	if (get_token(&token, fip, str, lex_solids, syms) == EOF) {
		parse_error(str, "parse_point: Unexpect EOF getting solid type.");
		return 0;
	}
	if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
	if (token.type != RT_LEX_KEYWORD) {
		parse_error(str,"parse_point: syntax error getting solid type.");
		return 0;
	}
	switch (token.t_key.value) {
	case ID_FIXED:
		pp->type = ID_FIXED;
		if (!parse_vect(&pp->point[0], fip, str)) return 0;
		return gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str);
	case ID_SPH:
		pp->type = ID_SPH;
		break;
	case ID_GRIP:
		pp->type = ID_GRIP;
		break;
	case ID_JOINT:
		pp->type = ID_JOINT;
		break;
	default:
		parse_error(str, "parse_point: Syntax error-XXX.");
		skip_group(fip,str);
		return 0;
	}
	if (!parse_solid(pp, fip, str)) {
		skip_group(fip, str);
		return 0;
	}
	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_point: close.\n");
	}
	return 1;
}
static int
parse_hold(fip, str)
FILE *fip;
struct rt_vls *str;
{
	struct hold *hp;
	union rt_lex_token token;
	int jsetfound = 0, efffound=0, goalfound=0, weightfound=0, prifound=0;

	if (joint_debug & DEBUG_J_PARSE) {
		rt_log("parse_hold: reading constraint\n");
	}
	GETSTRUCT(hp, hold);
	hp->l.magic = MAGIC_HOLD_STRUCT;
	hp->name = NULL;
	hp->joint = NULL;
	RT_LIST_INIT(&hp->j_head);
	hp->effector.type = ID_FIXED;
	hp->effector.arc.type = ARC_UNSET;
	db_init_full_path(&hp->effector.path);
	hp->effector.flag = 0;
	hp->objective.type = ID_FIXED;
	hp->objective.arc.type = ARC_UNSET;
	db_init_full_path(&hp->objective.path);
	hp->objective.flag = 0;
	hp->j_set.joint = NULL;
	hp->j_set.path.type = ARC_UNSET;
	hp->j_set.exclude.type = ARC_UNSET;

	if ( get_token(&token, fip, str, 0, syms) == EOF) {
		parse_error(str, "parse_hold: Unexpected EOF getting name.");
		free_hold(hp);
		return 0;
	}
	if (token.type == RT_LEX_IDENT) {
		hp->name = token.t_id.value;
		if ( get_token(&token, fip, str, 0, syms) == EOF) {
			parse_error(str, "parse_hold: Unexpected EOF getting open group.");
			free_hold(hp);
			return 0;
		}
	}
	if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");
	if (token.type != RT_LEX_SYMBOL || token.t_key.value != SYM_OP_GROUP) {
		parse_error(str, "parse_hold: syntax error, expecting open group.");
		free_hold(hp);
		return 0;
	}
	
	for (;;) {
		if (get_token(&token, fip, str, keys, syms) == EOF) {
			parse_error(str, "parse_hold: Unexpected EOF getting constraint contents.");
			skip_group( fip, str);
			free_hold(hp);
		}
		if (token.type == RT_LEX_IDENT) rt_free(token.t_id.value, "unit token");

		if (token.type == RT_LEX_SYMBOL && token.t_key.value == SYM_CL_GROUP) {
			if (joint_debug & DEBUG_J_PARSE) {
				rt_log("parse_hold: closing.\n");
			}

			if (!jsetfound) {
				parse_error(str,"parse_hold: no joint set given.");
				free_hold(hp);
				skip_group(fip, str);
				return 0;
			}
			if (!efffound) {
				parse_error(str, "parse_hold: no effector given.");
				free_hold(hp);
				skip_group(fip,str);
				return 0;
			}
			if (!goalfound) {
				parse_error(str, "parse_hold: no goal given.");
				free_hold(hp);
				skip_group(fip, str);
				return 0;
			}
			if (!weightfound) {
				hp->weight = 1.0;
			}
			if (!prifound) {
				hp->priority = 50;
			}
			RT_LIST_INSERT(&hold_head, &(hp->l));

			gobble_token(RT_LEX_SYMBOL, SYM_END, fip, str);
			return 1;
		}
		if (token.type != RT_LEX_KEYWORD) {
			parse_error(str, "parse_hold: syntax error");
			skip_group(fip, str);
			free_hold(hp);
			return 0;
		}

		switch (token.t_key.value) {
/* effector, goal */
		case KEY_WEIGHT:
			if (!parse_assign(&hp->weight, fip, str)) {
				free_hold(hp);
				skip_group(fip, str);
				return 0;
			}
			weightfound = 1;
			break;
		case KEY_PRI:
			if (!parse_assign(&hp->priority, fip, str)) {
				free_hold(hp);
				skip_group(fip,str);
				return 0;
			}
			prifound=1;
			break;
		case KEY_JOINTS:
			if (jsetfound) {
				parse_error(str,"parse_hold: joint set redefined.");
				free_hold(hp);
				skip_group(fip,str);
				return 0;
			}
			if (!parse_jset(hp, fip, str)) {
				free_hold(hp);
				skip_group(fip,str);
				return 0;
			}
			jsetfound = 1;
			break;
		case KEY_EFF:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip,str);
				free_hold(hp);
				return 0;
			}
			if (!parse_point(&hp->effector, fip, str)) {
				skip_group(fip, str);
				free_hold(hp);
				return 0;
			}
			efffound = 1;
			break;
		case KEY_POINT:
			if (!gobble_token(RT_LEX_SYMBOL, SYM_EQ, fip, str)) {
				skip_group(fip,str);
				free_hold(hp);
				return 0;
			}
			if (!parse_point(&hp->objective, fip, str)) {
				skip_group(fip, str);
				free_hold(hp);
				return 0;
			}
			goalfound=1;
			break;
		default:
			parse_error(str,"parse_hold: syntax error.");
			break;
		}
	}
	/* NOTREACHED */
}
static struct rt_list path_head;
void
f_jload(argc, argv)
int argc;
char **argv;
{
	FILE *fip;
	struct rt_vls	*instring;
	union rt_lex_token token;
	int	no_unload = 0, no_apply=0, no_mesh=0;
	int	c;
	struct	joint *jp;
	struct	hold *hp;

	optind = 1;
	while ((c=getopt(argc,argv,"uam")) != EOF ) {
		switch (c) {
		case 'u': no_unload = 1;break;
		case 'a': no_apply = 1; break;
		case 'm': no_mesh = 1; break;
		default:
			rt_log("Usage: joint load [-uam] file_name [files]\n");
			break;
		}
	}
	argv += optind;
	argc -= optind;
	if (!no_unload) f_junload(0,0);

	base2mm = dbip->dbi_base2local;
	mm2base = dbip->dbi_local2base;

	GETSTRUCT(instring,rt_vls);
	rt_vls_init(instring);

	while (argc) {
		fip = fopen(*argv, "r");
		if (fip == NULL) {
			rt_log("joint load: unable to open '%s'.\n", *argv);
			++argv;
			--argc;
			continue;
		}
		if (joint_debug & DEBUG_J_LOAD) {
			rt_log("joint load: loading from '%s'.\n", *argv);
		}
		lex_line = 0;
		lex_name = *argv;

		while (get_token(&token, fip, instring, keys, syms) != EOF ) {
			if (token.type == RT_LEX_KEYWORD) {
				if (token.t_key.value == KEY_JOINT ) {
					if (parse_joint(fip,instring)) {
						jp = RT_LIST_LAST(joint, &joint_head);
						if (!no_apply) joint_move(jp);
					}
				} else if (token.t_key.value == KEY_CON) {
					(void)parse_hold( fip, instring);
				} else if (token.t_key.value == KEY_UNITS) {
					(void)parse_units(fip, instring);
				} else {
					parse_error(instring,"joint load: syntax error.");
				}
			} else {
				parse_error(instring,"joint load: syntax error.");
			}
			if (token.type == RT_LEX_IDENT) {
				rt_free(token.t_id.value, "unit token");
			}
		}
		fclose(fip);
		argc--;
		argv++;
	}
/* CTJ */
	/*
	 * For each "struct arc" in joints or constraints, build a linked
	 * list of all ARC_PATHs and a control list of all unique tops.
	 */
	RT_LIST_INIT(&path_head);
	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		if (jp->path.type == ARC_PATH) {
			RT_LIST_INSERT(&path_head, &(jp->path.l));
		}
	}
	for (RT_LIST_FOR(hp, hold, &hold_head)) {
		if (hp->j_set.path.type == ARC_PATH) {
			RT_LIST_INSERT(&path_head, &(hp->j_set.path.l));
		}
		if (hp->effector.arc.type == ARC_PATH) {
			RT_LIST_INSERT(&path_head, &(hp->effector.arc.l));
		}
		if (hp->objective.arc.type == ARC_PATH) {
			RT_LIST_INSERT(&path_head, &(hp->objective.arc.l));
		}
	}
	/*
	 * call the tree walker to search for these paths.
	 */
	/*
	 * All ARC_PATHS have been translated into ARC_ARC.
	 *
	 * Constraints need to have ARC_ARCs translated to ARC_LISTS, this
	 * can be done at a latter time, such as when the constraint is
	 * evaluated. ??? XXX
	 */
	for (RT_LIST_FOR(hp, hold, &hold_head)) {
		register struct directory *dp;
		register int i;

		if (hp->effector.arc.type == ARC_ARC) {
			db_init_full_path(&hp->effector.path);
			hp->effector.path.fp_len = hp->effector.arc.arc_last+1;
			hp->effector.path.fp_maxlen = hp->effector.arc.arc_last+1;
			hp->effector.path.fp_names = (struct directory **)
			    rt_malloc(sizeof(struct directory **) * hp->effector.path.fp_maxlen,
			    "full path");
			for (i=0; i<= hp->effector.arc.arc_last; i++) {
				dp = hp->effector.path.fp_names[i] = 
				    db_lookup(dbip, hp->effector.arc.arc[i], 
				    LOOKUP_NOISY);
				if (!dp) {
					hp->effector.path.fp_len = i;
					db_free_full_path(&hp->effector.path);
					break;
				}
			}
		}
		if (hp->objective.arc.type == ARC_ARC) {
			db_init_full_path(&hp->objective.path);
			hp->objective.path.fp_len = hp->objective.arc.arc_last+1;
			hp->objective.path.fp_maxlen = hp->objective.arc.arc_last+1;
			hp->objective.path.fp_names = (struct directory **)
			    rt_malloc(sizeof(struct directory **) * hp->objective.path.fp_maxlen,
			    "full path");
			for (i=0; i<= hp->objective.arc.arc_last; i++) {
				dp = hp->objective.path.fp_names[i] = 
				    db_lookup(dbip, hp->objective.arc.arc[i], 
				    LOOKUP_NOISY);
				if (!dp) {
					hp->objective.path.fp_len = i;
					db_free_full_path(&hp->objective.path);
					break;
				}
			}
		}
	}
	if (!no_mesh) (void) f_jmesh(0,0);
	return;
}
void
f_jtest(argc, argv)
int argc;
char **argv;
{
}
void
f_jsave(argc, argv)
int argc;
char **argv;
{
	register struct joint *jp;
	register int i;
	FILE *fop;

	--argc;
	++argv;

	if (argc <1) {
		rt_log("joint save: missing file name");
		return;
	}
	fop = fopen(*argv,"w");
	if (!fop) {
		rt_log("joint save: unable to open '%s' for writing.\n",
		    *argv);
		return;
	}
	fprintf(fop,"# joints and constraints for '%s'\n",
	    dbip->dbi_title);

	switch (dbip->dbi_localunit) {
	case ID_CM_UNIT:
		fputs("units cm;\n",fop); break;
	case ID_M_UNIT:
		fputs("units meters;\n",fop); break;
	case ID_IN_UNIT:
		fputs("units inches;\n",fop); break;
	case ID_FT_UNIT:
		fputs("units feet;\n",fop); break;
	case ID_NO_UNIT:
	case ID_MM_UNIT:
	default:
		fputs("units mm;\n",fop);
	}
	mm2base = dbip->dbi_local2base;
	base2mm = dbip->dbi_base2local;

	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		fprintf(fop,"joint %s {\n",jp->name);
		/* } for jove */
		if (jp->path.type == ARC_PATH) {
			fprintf(fop,"\tpath = %s",jp->path.arc[0]);
			for(i=1;i<jp->path.arc_last;i++) {
				fprintf(fop,"/%s",jp->path.arc[i]);
			}
			fprintf(fop,"-%s;\n",jp->path.arc[i]);
		} else if (jp->path.type & ARC_BOTH) {
			fprintf(fop,"\tpath = %s", jp->path.original[0]);
			for (i=1; i < jp->path.org_last; i++) {
				fprintf(fop,"/%s",jp->path.original[i]);
			}
			fprintf(fop,"-%s;\n",jp->path.original[i]);
		} else { /* ARC_ARC */
			fprintf(fop,"\tarc = %s", jp->path.arc[0]);
			for(i=1;i<jp->path.arc_last;i++) {
				fprintf(fop,"/%s", jp->path.arc[i]);
			}
			fprintf(fop,"/%s;\n", jp->path.arc[i]);
		}
		fprintf(fop,"\tlocation = (%.15e, %.15e, %.15e);\n", 
		    jp->location[X]*mm2base, jp->location[Y]*mm2base,
		    jp->location[Z]*mm2base);

		for (i=0;i<3;i++) {
			if (jp->rots[i].upper < jp->rots[i].lower) break;
			fprintf(fop,
"\trotate {\n\t\tdirection = (%.15e, %.15e, %.15e);\n\t\tlimits = %.15e, %.15e, %.15e;\n\t}\n",
			    jp->rots[i].quat[X], jp->rots[i].quat[Y],
			    jp->rots[i].quat[Z], 
			    jp->rots[i].lower, jp->rots[i].upper,
			    jp->rots[i].current);
		}
		for (i=0;i<3;i++) {
			if (jp->dirs[i].upper < jp->dirs[i].lower) break;
			fprintf(fop,
"\ttranslate {\n\t\tdirection = (%.15e, %.15e, %.15e);\n\t\tlimits = %.15e, %.15e, %.15e;\n\t}\n",
			    jp->dirs[i].unitvec[X], jp->dirs[i].unitvec[Y],
			    jp->dirs[i].unitvec[Z], jp->dirs[i].lower*mm2base,
			    jp->dirs[i].upper*mm2base, jp->dirs[i].current*mm2base);
		}
		fprintf(fop,"};\n");
	}
	fclose(fop);
}
void
f_jaccept(argc, argv)
int argc;
char **argv;
{
	register struct joint *jp;
	register int i;
	int c;
	int no_mesh = 0;

	optind=1;
	while ( (c=getopt(argc,argv, "m")) != EOF) {
		switch (c) {
		case 'm': no_mesh=1;break;
		default:
			rt_log("Usage: joint accept [-m] [joint_names]\n");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		if (argc) {
			for (i=0; i<argc; i++) {
				if (strcmp(argv[i], jp->name) == 0) break;
			}
			if (i>=argc) continue;
		}
		for(i=0; i<3; i++) {
			jp->dirs[i].accepted = jp->dirs[i].current;
			jp->rots[i].accepted = jp->rots[i].current;
		}
	}
	if (!no_mesh) f_jmesh(0,0);
}
void
f_jreject(argc, argv)
int argc;
char **argv;
{
	register struct joint *jp;
	register int i;
	int c;
	int no_mesh = 0;

	optind=1;
	while ( (c=getopt(argc,argv, "m")) != EOF) {
		switch (c) {
		case 'm': no_mesh=1;break;
		default:
			rt_log("Usage: joint accept [-m] [joint_names]\n");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		if (argc) {
			for (i=0; i<argc; i++) {
				if (strcmp(argv[i], jp->name) == 0) break;
			}
			if (i>=argc) continue;
		}

		for (i=0; i<3; i++) {
			jp->rots[i].current = jp->rots[i].accepted;
			jp->dirs[i].current = jp->dirs[i].accepted;
		}
		joint_move(jp);
	}
	if (!no_mesh) f_jmesh(0,0);
}
static int
hold_point_location(loc, hp)
vect_t loc;
struct hold_point *hp;
{
	mat_t mat;
	struct joint *jp;
	struct rt_grip_internal *gip;
	struct rt_external	es_ext;
	struct rt_db_internal	es_int;
	int id;

	VSETALL(loc, 0.0);	/* default is the origin. */
	switch (hp->type) {
	case ID_FIXED:
		VMOVE(loc, hp->point);
		return 1;
	case ID_GRIP:
		if (hp->flag & HOLD_PT_GOOD) {
			db_path_to_mat(dbip, &hp->path, mat, hp->path.fp_len-2);
			MAT4X3PNT(loc, mat, hp->point);
			return 1;
		}
		mat_idn(mat);
		RT_INIT_EXTERNAL(&es_ext);
		RT_INIT_DB_INTERNAL(&es_int);
		if (db_get_external( &es_ext,
		    hp->path.fp_names[hp->path.fp_len-1], dbip) < 0) return 0;
		id = rt_id_solid( &es_ext);
		if (id != ID_GRIP) return 0;
		if (rt_functab[id].ft_import( &es_int, &es_ext, mat) < 0) {
			db_free_external(&es_ext);
			return 0;
		}
		RT_CK_DB_INTERNAL(&es_int);
		gip = (struct rt_grip_internal *)es_int.idb_ptr;
		VMOVE(hp->point, gip->center);
		hp->flag |= HOLD_PT_GOOD;
		rt_functab[id].ft_ifree( &es_int);
		db_free_external(&es_ext);
		db_path_to_mat(dbip, &hp->path, mat, hp->path.fp_len-2);
		MAT4X3PNT(loc, mat, hp->point);
		return 1;
	case ID_JOINT:
		db_path_to_mat(dbip, &hp->path, mat, hp->path.fp_len-3);
		if (hp->flag & HOLD_PT_GOOD) {
			MAT4X3VEC(loc, mat, hp->point);
			return 1;
		}
		jp = joint_lookup(hp->arc.arc[hp->arc.arc_last]);
		if (!jp) {
			rt_log("hold_eval: Lost joint!  %s not found!\n",
			    hp->arc.arc[hp->arc.arc_last]);
			/* rt_bomb(); */
			return 0;
		}
		VMOVE(hp->point, jp->location);
		hp->flag |= HOLD_PT_GOOD;
		MAT4X3VEC(loc, mat, hp->point);
		return 1;
	}
	/* NEVER REACHED */
	return 1;	/* For the picky compilers */
}
double
hold_eval(hp)
struct hold *hp;
{
	vect_t	e_loc, o_loc;
	double	value;

	/* 
	 * get the current location of the effector.
	 */
	if (!hold_point_location(e_loc, &hp->effector)) {
		if (joint_debug & DEBUG_J_EVAL) {
			rt_log("hold_eval: unable to find location of effector for %s.\n",
			    hp->name);
		}
		return 0.0;
	}
	if (!hold_point_location(o_loc, &hp->objective)) {
		if (joint_debug & DEBUG_J_EVAL) {
			rt_log("hold_eval: unable to find location of objective for %s.\n",
			    hp->name);
		}
		return 0.0;
	}
	value = hp->weight * DIST_PT_PT(e_loc, o_loc);
	if (joint_debug & DEBUG_J_EVAL) {
		rt_log("hold_eval: PT->PT of %s is %g\n", hp->name, value);
	}
	return value;
}
struct solve_stack {
	struct rt_list	l;
	struct joint	*jp;
	int		freedom;
	double		old;
	double		new;
};
#define	SOLVE_STACK_MAGIC	0x76766767
struct rt_list solve_head = {
	RT_LIST_HEAD_MAGIC,
	&solve_head,
	&solve_head
};

joint_clear()
{
	register struct stack_solve *ssp;
	RT_LIST_POP(stack_solve, &solve_head, ssp);
	while (ssp) {
		rt_free((char *)ssp, "struct stack_solve");
		RT_LIST_POP(stack_solve, &solve_head, ssp);
	}
}

part_solve(hp, limits, tol)
struct hold *hp;
double limits;
double tol;
{
	register struct joint *jp;
	double f0,f1,f2;
	double ax,bx,cx;
	double x0,x1,x2,x3;
	double besteval, bestvalue, origvalue;
	int bestfreedom;
	register struct joint *bestjoint;
	register struct jointH *jh;

	if (joint_debug & DEBUG_J_SOLVE) {
		rt_log("part_solve: solving for %s.\n", hp->name);
	}

	if (RT_LIST_IS_EMPTY(&hp->j_head)) {
		register int i,j;
		int startjoint;
		startjoint = -1;
		if (joint_debug & DEBUG_J_SOLVE) {
			rt_log("part_solve: looking for joints on arc.\n");
		}
		for(RT_LIST_FOR(jp,joint,&joint_head)) {
			if (hp->j_set.path.type == ARC_LIST) {
				for (i=0; i<= hp->j_set.path.arc_last; i++) {
					if (strcmp(jp->name, hp->j_set.path.arc[i]) == 0) {
						GETSTRUCT(jh, jointH);
						jh->l.magic = MAGIC_JOINT_HANDLE;
						jh->p = jp;
						jp->uses++;
						jh->arc_loc = -1;
						jh->flag = 0;
						RT_LIST_APPEND(&hp->j_head, &jh->l);
						break;
					}
				}
				continue;
			}
			for (i=0;i<hp->effector.path.fp_len; i++) {
				if (strcmp(jp->path.arc[0],
				    hp->effector.path.fp_names[i]->d_namep)==0) break;
			}
			if (i+jp->path.arc_last >= hp->effector.path.fp_len) continue;
			for (j=1; j<=jp->path.arc_last;j++){
				if (strcmp(jp->path.arc[j],
				    hp->effector.path.fp_names[i+j]->d_namep)
				    != 0) break;
			}
			if (j>jp->path.arc_last) {
				if (joint_debug & DEBUG_J_SOLVE) {
					rt_log("part_solve: found %s\n",
					    jp->name);
				}
				GETSTRUCT(jh, jointH);
				jh->l.magic = MAGIC_JOINT_HANDLE;
				jh->p = jp;
				jp->uses++;
				jh->arc_loc = i+j-1;
				jh->flag = 0;
				RT_LIST_APPEND(&hp->j_head, &jh->l);
				if (strcmp(hp->joint, jp->name) == 0) {
					startjoint = jh->arc_loc;
				}
			}
		}
		if (startjoint < 0) {
			rt_log("part_solve: %s, joint %s not on arc.\n",
			    hp->name, hp->joint);
		}
		for (RT_LIST_FOR(jh, jointH, &hp->j_head)) {
			/*
			 * XXX - Coming to a source module near you  RSN.
			 * Not only joint location, but drop joints that
			 * are "locked"
			 */
			if (jh->arc_loc < startjoint) {
				register struct jointH *hold;
				if (joint_debug & DEBUG_J_SOLVE) {
					rt_log("part_solve: dequeuing %s from %s\n",
					    jh->p->name, hp->name);
				}
				hold=(struct jointH *)jh->l.back;
				RT_LIST_DEQUEUE(&jh->l);
				jh->p->uses--;
				jh=hold;
			}
		}
	}
	origvalue = besteval = hold_eval(hp);
	if (fabs(origvalue) < tol) {
		if (joint_debug & DEBUG_J_SOLVE) {
			rt_log("part_solve: solved, original(%g) < tol(%g)\n",
			    origvalue, tol);
		}
		return 0;
	}
	bestjoint = (struct joint *)0;
	/*
	 * From here, we try each joint to try and find the best movement
	 * if any.
	 */
	for (RT_LIST_FOR(jh, jointH, &hp->j_head)) {
		register int i;
		double hold;
		jp= jh->p;
		for (i=0;i<3;i++) {
			if ( (jh->flag & (1<<i)) ||
			    jp->rots[i].upper < jp->rots[i].lower) {
			    	jh->flag |= (1<<i);
				continue;
			}
			hold = bx =jp->rots[i].current;
#define EPSI	1e-6
#define R	0.61803399
#define C	(1.0-R)
			/*
			 * find the min in the range ax-bx-cx where ax is
			 * bx-limits-0.001 or lower and cx = bx+limits+0.001
			 * or upper.
			 */
			ax=bx-limits-EPSI;
			if (ax < jp->rots[i].lower) ax=jp->rots[i].lower;
			cx=bx+limits+EPSI;
			if (cx > jp->rots[i].upper) cx=jp->rots[i].upper;
			x0=ax;
			x3=cx;
			if (fabs(cx-bx) > fabs(bx-ax)) {
				x1=bx;
				x2=bx+C*(cx-bx);
			} else {
				x2=bx;
				x1=bx-C*(bx-ax);
			}
			jp->rots[i].current = x1;
			joint_move(jp);
			f1=hold_eval(hp);
			jp->rots[i].current = x2;
			joint_move(jp);
			f2=hold_eval(hp);
			while (fabs(x3-x0) > EPSI*(fabs(x1)+fabs(x2))) {
				if (f2 < f1) {
					x0 = x1;
					x1 = x2;
					x2 = R*x1+C*x3;
					f1=f2;
					jp->rots[i].current = x2;
					joint_move(jp);
					f2=hold_eval(hp);
				} else {
					x3=x2;
					x2=x1;
					x1=R*x2+C*x0;
					f2=f1;
					jp->rots[i].current = x1;
					joint_move(jp);
					f1=hold_eval(hp);
				}
			}
			if (f1 < f2) {
				x0=x1;
				f0=f1;
			} else {
				x0=x2;
				f0=f2;
			}
			jp->rots[i].current = hold;
			joint_move(jp);
			if (f0 < besteval) {
				if (joint_debug & DEBUG_J_SOLVE) {
					rt_log("part_solve: NEW min %s(%d,%g) %g <%g\n",
					    jp->name, i, x0,f0, besteval);
				}
				besteval = f0;
				bestjoint = jp;
				bestfreedom = i;
				bestvalue = x0;
			} else 	if (joint_debug & DEBUG_J_SOLVE) {
				rt_log("part_solve: OLD min %s(%d,%g)%g >= %g\n",
				    jp->name, i, x0, f0, besteval);
			}
		}
		/*
		 * Now we do the same thing but for directional movements.
		 */
		for (i=0;i<3;i++) {
			if ( (jh->flag & (1<<(i+3))) ||
			    (jp->dirs[i].upper < jp->dirs[i].lower)) {
			    	jh->flag |= (1<<(i+3));
				continue;
			}
			hold = bx =jp->dirs[i].current;
			/*
			 * find the min in the range ax-bx-cx where ax is
			 * bx-limits-0.001 or lower and cx = bx+limits+0.001
			 * or upper.
			 */
			ax=bx-limits-EPSI;
			if (ax < jp->dirs[i].lower) ax=jp->dirs[i].lower;
			cx=bx+limits+EPSI;
			if (cx > jp->dirs[i].upper) cx=jp->dirs[i].upper;
			x0=ax;
			x3=cx;
			if (fabs(cx-bx) > fabs(bx-ax)) {
				x1=bx;
				x2=bx+C*(cx-bx);
			} else {
				x2=bx;
				x1=bx-C*(bx-ax);
			}
			jp->dirs[i].current = x1;
			joint_move(jp);
			f1=hold_eval(hp);
			jp->dirs[i].current = x2;
			joint_move(jp);
			f2=hold_eval(hp);
			while (fabs(x3-x0) > EPSI*(fabs(x1)+fabs(x2))) {
				if (f2 < f1) {
					x0 = x1;
					x1 = x2;
					x2 = R*x1+C*x3;
					f1=f2;
					jp->dirs[i].current = x2;
					joint_move(jp);
					f2=hold_eval(hp);
				} else {
					x3=x2;
					x2=x1;
					x1=R*x2+C*x0;
					f2=f1;
					jp->dirs[i].current = x1;
					joint_move(jp);
					f1=hold_eval(hp);
				}
			}
			if (f1 < f2) {
				x0=x1;
				f0=f1;
			} else {
				x0=x2;
				f0=f2;
			}
			jp->dirs[i].current = hold;
			joint_move(jp);
			if (f0 < besteval-SQRT_SMALL_FASTF) {
				if (joint_debug & DEBUG_J_SOLVE) {
					rt_log("part_solve: NEW min %s(%d,%g) %g <%g delta=%g\n",
					    jp->name, i+3, x0,f0, besteval, besteval-f0);
				}
				besteval = f0;
				bestjoint = jp;
				bestfreedom = i + 3;
				bestvalue = x0;
			} else 	if (joint_debug & DEBUG_J_SOLVE) {
				rt_log("part_solve: OLD min %s(%d,%g)%g >= %g\n",
				    jp->name, i, x0, f0, besteval);
			}

		}
	}
	/*
	 * Did we find a better joint?
	 */
	if (!bestjoint) {
		if (joint_debug & DEBUG_J_SOLVE) {
			rt_log("part_solve: No joint configuration found to be better.\n");
		}
		return 0;
	}
	if (origvalue - besteval < (tol/100.0)) {
		if (joint_debug & DEBUG_J_SOLVE) {
			rt_log("part_solve: No reasonable improvement found.\n");
		}
		return 0;
	}
	{
		struct solve_stack *ssp;
		GETSTRUCT(ssp, solve_stack);
		ssp->jp = bestjoint;
		ssp->freedom = bestfreedom;
		ssp->old = (bestfreedom<3) ? bestjoint->rots[bestfreedom].current :
		    bestjoint->dirs[bestfreedom-3].current;
		ssp->new = bestvalue;
		RT_LIST_PUSH(&solve_head,ssp);
	}
	if (bestfreedom < 3 ) {
		bestjoint->rots[bestfreedom].current = bestvalue;
	} else {
		bestjoint->dirs[bestfreedom-3].current = bestvalue;
	}
	joint_move(bestjoint);
	return 1;
}
void
reject_move()
{
	register struct solve_stack *ssp;
	RT_LIST_POP(solve_stack, &solve_head, ssp);
	if (!ssp) return;
	if (joint_debug & DEBUG_J_SYSTEM) {
		rt_log("reject_move: rejecting %s(%d,%g)->%g\n", ssp->jp->name,
		    ssp->freedom, ssp->new, ssp->old);
	}
	if (ssp->freedom<3) { 
		ssp->jp->rots[ssp->freedom].current = ssp->old;
	} else {
		ssp->jp->dirs[ssp->freedom-3].current = ssp->old;
	}
	joint_move(ssp->jp);
	rt_free((char *)ssp, "struct solve_stack");
}
/*	Constraint system solver.
 *
 * The basic idea is that we are called with some priority level.
 * We will attempted to solve all constraints at that level with out
 * permenently damaging a joint of a higher priority.
 *
 * Returns:
 *	-1	This system could not be made better without damage
 *		to a higher priority system.
 *	0	All systems at a higher priority stayed stable or
 *		got better and thise priority level got better.
 *	1	All systems at a higher priority stayed stable or
 *		got better and this system is at at min.
 * Method:
 *	while all constraints at this level are not solved:
 *		try to solve the select constraint.
 *		if joint change and this priority level got better then
 *			result = system_solve(current_priority - 1);
 *			if (result == worse) then
 *				reject this joint change
 *			fi
 *		else
 *			mark this constraint as "solved"
 *		fi
 *	endwhile
 */
#define SOLVE_MAX_PRIORITY	100
int
system_solve(pri,delta, epsilon)
int pri;
double delta;
double epsilon;
{
	double	pri_weights[SOLVE_MAX_PRIORITY+1];
	double	new_weights[SOLVE_MAX_PRIORITY+1];
	double	new_eval;
	register int i;
	int	j;
	register struct hold *hp;
	struct hold *test_hold;
	struct jointH *jh;
	struct solve_stack *ssp;

	if (pri < 0) return 1;

	for (i=0; i<=pri; i++) 	pri_weights[i]=0.0;
	for (RT_LIST_FOR(hp,hold,&hold_head)) {
		hp->eval = hold_eval(hp);
		pri_weights[hp->priority] += hp->eval;
	}

	if (joint_debug & DEBUG_J_SYSTEM) {
		for (i=0; i <= pri; i++ ) {
			if (pri_weights[i] > 0.0) {
				rt_log("system_solve: priority %d has system weight of %g.\n",
				    i, pri_weights[i]);
			}
		}
	}
	/*
	 * sort constraints by priority then weight from the evaluation
	 * we just did.
	 */
	for (hp=(struct hold *)hold_head.forw; hp->l.forw != &hold_head;) {
		register struct hold *tmp;
		tmp = (struct hold *)hp->l.forw;

	    	if ((tmp->priority < hp->priority) ||
		    ((tmp->priority == hp->priority) &&
		     (tmp->eval > hp->eval))) {
		     	RT_LIST_DEQUEUE(&tmp->l);
		     	RT_LIST_INSERT(&hp->l,&tmp->l);
		     	if (tmp->l.back != &hold_head) {
		     		hp = (struct hold*)tmp->l.back;
		     	}
	     	} else {
	     		hp = (struct hold*)hp->l.forw;
		}
	}
Middle:
	/*
	 * now we find the constraint(s) we will be working with.
	 */
	for (; pri>=0 && pri_weights[pri] < epsilon; pri--);
	if (pri <0) {
		if (joint_debug & DEBUG_J_SYSTEM) {
			rt_log("system_solve: returning 1\n");
		}
		return 1;	/* solved */
	}
	for (RT_LIST_FOR(hp,hold,&hold_head)) {
		if (hp->priority != pri) continue;
		if (hp->flag & HOLD_FLAG_TRIED) continue;
		if (part_solve(hp,delta,epsilon)==0) continue;
		test_hold = hp;
		break;
	}
	/*
	 * Now check to see if a) anything happened, b) that it was good
	 * for the entire system.
	 */
	if (hp==(struct hold*)&hold_head) {
		/*
		 * There was nothing we could do at this level.  Try
		 * again at a higher level.
		 */
		pri--;
		goto Middle;
	}
	/*
	 * We did something so lets re-evaluated and see if it got any
	 * better at THIS level only.  breaking things of lower priority
	 * does not bother us.  If things got worse at a lower priority
	 * we'll know that in a little bit.
	 */
	new_eval = 0.0;
	for (RT_LIST_FOR(hp,hold,&hold_head)) {
		if (hp->priority != pri) continue;
		new_eval += hold_eval(hp);
	}
	if (joint_debug & DEBUG_J_SYSTEM) {
		rt_log("system_solve: old eval = %g, new eval = %g\n",
		    pri_weights[pri], new_eval);
	}
	/*
	 * if the new evaluation is worse then the origianal, back off
	 * this modification, set the constraint such that this freedom
	 * of this joint won't be used next time through part_solve.
	 */
	if (new_eval > pri_weights[pri]+epsilon) {
		/*
		 * now we see if there is anything we can do with this
		 * constraint.
		 */
		ssp = (struct solve_stack *) solve_head.forw;

		i = (2<<6) - 1;		/* Six degrees of freedom */
		for (RT_LIST_FOR(jh,jointH, &test_hold->j_head)) {
			if (ssp->jp != jh->p) {
				i &= jh->flag;
				continue;
			}
			jh->flag |= (1 << ssp->freedom);
			i &= jh->flag;
		}
		if (i == ((2<<6)-1)) {	/* All joints, all freedoms */
			test_hold->flag |= HOLD_FLAG_TRIED;
		}
		reject_move();
		goto Middle;
	}
	/*
	 * Ok, we've got a constraint that makes this priority system
	 * better, now we've got to make sure all the constraints below
	 * this are better or solve also.
	 */
	ssp = (struct solve_stack *) solve_head.forw;
	for (j=0; (i = system_solve(pri-1, delta, epsilon)) == 0; j++);
	/*
	 * All constraints at a higher priority are stabilized.
	 *
	 * If system_solve() returned "1" then every thing higher is
	 * happy and we only have to worry about his one.  If -1 was
	 * returned then all higher priorities have to be check to
	 * make sure they did not get any worse.
	 */
	for (j=0; j<=pri; j++) new_weights[j] = 0.0;
	for (RT_LIST_FOR(hp, hold, &hold_head)) {
		new_weights[hp->priority] += hold_eval(hp);
	}
	for (j=0; j<=pri; j++) {
		if (new_weights[j] > pri_weights[j] + epsilon) break;
	}
	/*
	 * if j <= pri, then that priority got worse.  Since it is
	 * worse, we need to clean up what's been done before and
	 * exit out of here.
	 */
	if (j <= pri) {
		while (ssp != (struct solve_stack *) solve_head.forw) {
			reject_move();
		}
		i = (2 << 6) - 1;
		for (RT_LIST_FOR(jh, jointH, &test_hold->j_head)) {
			if (ssp->jp != jh->p) {
				i &= jh->flag;
				continue;
			}
			jh->flag |= (1 << ssp->freedom);
			i &= jh->flag;
		}
		if (i == ((2<<6) - 1)) {
			test_hold->flag |= HOLD_FLAG_TRIED;
		}
		reject_move();
		if (joint_debug & DEBUG_J_SYSTEM) {
			rt_log("system_solve: returning -1\n");
		}
		return -1;
	}
	if (joint_debug & DEBUG_J_SYSTEM) {
		rt_log("system_solve: new_weights[%d] = %g, returning ", pri,
		    new_weights[pri]);
	}
	if (new_weights[pri] < epsilon) {
		if (joint_debug & DEBUG_J_SYSTEM) {
			rt_log("1\n");
		}
		return 1;
	}
	if (joint_debug & DEBUG_J_SYSTEM) {
		rt_log("0\n");
	}
	return 0;

}
void
f_jsolve(argc, argv)
int argc;
char **argv;
{
	register struct hold *hp;
	int loops, count;
	double delta, epsilon;
	int	domesh;
	int	found;
	char **myargv;
	int myargc;
	int result;

	/*
	 * because this routine calls "mesh" in the middle, the command
	 * arguements can be reused.  We cons up a new argv vector and
	 * copy all of the arguements before we do any processing.
	 */
	myargc = argc;
	myargv = (char **)rt_malloc(sizeof(char *)*argc, "param pointers");

	for (count=0; count<myargc; count++) {
		myargv[count] = (char *)rt_malloc(strlen(argv[count])+1,"param");
		strcpy(myargv[count], argv[count]);
	}

	argv=myargv;
	/* argc = myargc; */

	/*
	 * these are the defaults.  Domesh will change to not at a later
	 * time.
	 */
	loops = 20;
	delta = 5.0;
	epsilon = 0.1;
	domesh = 1;

	/*
	 * reset getopt.
	 */
	optind=1;
	while ((count=getopt(argc,argv,"l:e:d:m")) != EOF) {
		switch (count) {
		case 'l': loops = atoi(optarg);break;
		case 'e': epsilon = atof(optarg);break;
		case 'd': delta =  atof(optarg);break;
		case 'm': domesh = 1-domesh;
		}
	}

	/*
	 * skip the command and any options that getopt ate.
	 */
	argc -= optind;
	argv += optind;

	for (RT_LIST_FOR(hp, hold, &hold_head)) hold_clear_flags(hp);
	found = -1;
	while (argc) {
		found = 0;
		for(RT_LIST_FOR(hp,hold,&hold_head)) {
			if (strcmp(*argv, hp->name)==0) {
				found = 1;
				for (count=0; count<loops; count++) {
					if (!part_solve(hp,delta,epsilon)) break;
					if (domesh) {
						f_jmesh(0,0);
						refresh();
					}
					joint_clear();
				}
				rt_log("joint solve: finished %d loops of %s.\n",
				    count, hp->name);
				continue;
			}
		}
		if (!found) {
			rt_log("joint solve: constraint %s not found.\n",
				*argv);
		}
		--argc;
		++argv;
	}

	for (count=0; count<myargc; count++) {
		rt_free(myargv[count],"params");
	}
	rt_free((char *)myargv,"param pointers");

	if (found >= 0) return;

	/*
	 * solve the whole system of constraints.
	 */

	joint_clear();	/* make sure the system is empty. */

	for (count=0; count < loops; count++) {
		/*
		 * Clear all constrain flags.
		 */
		for (RT_LIST_FOR(hp,hold,&hold_head)) {
			register struct jointH *jh;
			hp->flag &= ~HOLD_FLAG_TRIED;
			hp->eval = hold_eval(hp);
			for (RT_LIST_FOR(jh, jointH, &hp->j_head)) {
				jh->flag = 0;
			}
		}
		result = system_solve(0,delta,epsilon);
		if (result == 1) {
			break;
		} else if (result == -1) {
			delta /= 2.0;
			if (joint_debug & DEBUG_J_SYSTEM) {
				rt_log("joint solve: spliting delta (%g)\n",
				    delta);
			}
			if (delta < epsilon) break;
		}
		joint_clear();
		if (domesh) {
			f_jmesh(0,0);
			refresh();
		}
	}
	if (count < loops) {
		for(count = 0; count < loops; count++) {
			/*
			 * Clear all constrain flags.
			 */
			for (RT_LIST_FOR(hp,hold,&hold_head)) {
				register struct jointH *jh;
				hp->flag &= ~HOLD_FLAG_TRIED;
				hp->eval = hold_eval(hp);
				for (RT_LIST_FOR(jh, jointH, &hp->j_head)) {
					jh->flag = 0;
				}
			}
			result =system_solve(SOLVE_MAX_PRIORITY,delta,epsilon);
			if (result == 1) {
				break;
			} else if (result == -1) {
				delta /= 2.0;
				if (joint_debug & DEBUG_J_SYSTEM) {
					rt_log("joint solve: spliting delta (%g)\n",
					    delta);
				}
				if (delta < epsilon) break;
			}
			joint_clear();
			if (domesh) {
				f_jmesh(0,0);
				refresh();
			}
		}
	}
	if (result == 1) {
		rt_log("joint solve: system has convereged to a result.\n");
	} else if (result == 0) {
		rt_log("joint solve: system has not converged in %d loops.\n",
		    count);
	} else {
		rt_log("joint solve: system will not converge.\n");
	}
	joint_clear();
	if (domesh) {
		f_jmesh(0,0);
		refresh();
	}

	return;
}
static char *
hold_point_to_string(hp)
struct hold_point *hp;
{
#define HOLD_POINT_TO_STRING_LEN	1024
	char *text = rt_malloc(HOLD_POINT_TO_STRING_LEN, "hold_point_to_string");
	char *path;
	vect_t loc;

	switch (hp->type) {
	case ID_FIXED:
		sprintf(text,"(%g %g %g)", hp->point[X],
		    hp->point[Y], hp->point[Z]);
		break;
	case ID_GRIP:
	case ID_JOINT:
		(void)hold_point_location(loc,hp);
		path = db_path_to_string(&hp->path);
		sprintf(text,"%s (%g %g %g)", path, loc[X], loc[Y], loc[Z]);
		rt_free(path, "full path");
		break;
	}
	if (strlen(text) > (unsigned)HOLD_POINT_TO_STRING_LEN) {
		rt_bomb("hold_point_to_string: over wrote memory!\n");
	}
	return text;
}
void
print_hold(hp)
struct hold *hp;
{
	char *t1, *t2;

	t1 = hold_point_to_string(&hp->effector);
	t2 = hold_point_to_string(&hp->objective);
	rt_log("holds:\t%s with %s\n\tfrom:%s\n\tto: %s", (hp->name) ?
	    hp->name : "UNNAMED", hp->joint,
	    t1,t2);
	rt_free(t1, "hold_point_to_string");
	rt_free(t2, "hold_point_to_string");
	rt_log("\n\twith a weight: %g, pull %g\n",
	    hp->weight, hold_eval(hp) );
}
void
f_jhold(argc, argv)
int argc;
char **argv;
{
	register struct hold *hp;
	++argv;
	--argc;
	for (RT_LIST_FOR(hp, hold, &hold_head)) {
		if (argc) {
			register int i;
			for (i=0; i<argc; i++) {
				if (strcmp(argv[i], hp->name) == 0) break;
			}
			if (i>=argc) continue;
		}
		hold_clear_flags(hp);
		print_hold(hp);
	}
}
void
f_jlist(argc, argv)
int argc;
char **argv;
{
	register struct joint *jp;
	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		col_item(jp->name);
	}
	col_eol();
	return;
}
void
joint_move(jp)
struct joint *jp;
{
	register struct animate *anp;
	double tmp;
	mat_t	m1,m2;
	quat_t	q1;
	int i;

	/*
	 * If no animate structure, cons one up.
	 */
	anp=jp->anim;
	if (!anp || anp->magic != ANIMATE_MAGIC) {
		char *sofar;
		struct directory *dp;
		GETSTRUCT(anp, animate);
		anp->magic = ANIMATE_MAGIC;
		db_init_full_path(&anp->an_path);
		anp->an_path.fp_len = jp->path.arc_last+1;
		anp->an_path.fp_maxlen= jp->path.arc_last+1;
		anp->an_path.fp_names = (struct directory **)
		    rt_malloc(sizeof(struct directory **)*anp->an_path.fp_maxlen,
		    "full path");
		for (i=0; i<= jp->path.arc_last; i++) {
			dp = anp->an_path.fp_names[i] = db_lookup(dbip,
			    jp->path.arc[i], LOOKUP_NOISY);
			if (!dp) {
				anp->an_path.fp_len = i;
				db_free_full_path(&anp->an_path);
				rt_free((char *)anp, "struct animate");
				return;
			}
		}
		jp->anim=anp;
		db_add_anim(dbip, anp, 0);
		if (joint_debug & DEBUG_J_MOVE) {
			sofar = db_path_to_string(&jp->anim->an_path);
			rt_log("joint move: %s added animate %s to %s(0x%x)\n",
			    jp->name, sofar, dp->d_namep, dp);
		}
	}


#define ANIM_MAT	(anp->an_u.anu_m.anm_mat)

	anp->an_type = RT_AN_MATRIX;
	anp->an_u.anu_m.anm_op = ANM_RMUL;

	/*
	 * Build the base matrix.  Ident with translate back to origin.
	 */
	mat_idn(ANIM_MAT);
	MAT_DELTAS_VEC_NEG(ANIM_MAT, jp->location);

	/*
	 * Do rotations.
	 */
	for (i=0; i<3; i++ ) {
		if (jp->rots[i].upper < jp->rots[i].lower) break;
		/*
		 * Build a quat from that.
		 */
		tmp = (jp->rots[i].current * rt_degtorad)/2.0;
		VMOVE(q1, jp->rots[i].quat);
		if (joint_debug & DEBUG_J_MOVE) {
			rt_log("joint move: rotating %g around (%g %g %g)\n",
			    tmp*2*rt_radtodeg, q1[X], q1[Y], q1[Z]);
		}
		{register double srot = sin(tmp);
			q1[X] *= srot;
			q1[Y] *= srot;
			q1[Z] *= srot;
		}
		q1[W] = cos(tmp);

		/*
		 * Build matrix.
		 */
		quat_quat2mat(m2,q1);
		mat_copy(m1, ANIM_MAT);
		mat_mul(ANIM_MAT, m2, m1);
		/*
		 * rmult matrix into the mat we are building.
		 */
	}
	/*
	 * do the translations.
	 */
	for (i=0; i<3; i++) {
		if (jp->dirs[i].upper < jp->dirs[i].lower) break;
		/*
		 * build matrix.
		 */
		tmp = jp->dirs[i].current;
		mat_idn(m2);
		MAT_DELTAS(m2, jp->dirs[i].unitvec[X]*tmp,
		    jp->dirs[i].unitvec[Y]*tmp,
		    jp->dirs[i].unitvec[Z]*tmp);

		if (joint_debug & DEBUG_J_MOVE) {
			rt_log("joint move: moving %g along (%g %g %g)\n",
			    tmp*base2local, m2[3], m2[7], m2[11]);
		}
		mat_copy(m1, ANIM_MAT);
		mat_mul(ANIM_MAT, m2, m1);
	}
	/*
	 * Now move the whole thing back to original location.
	 */
	mat_idn(m2);
	MAT_DELTAS_VEC(m2, jp->location);
	mat_copy(m1, ANIM_MAT);
	mat_mul(ANIM_MAT,m2,m1);
	if (joint_debug & DEBUG_J_MOVE) {
		mat_print("joint move: ANIM_MAT", ANIM_MAT);
	}
}
void
f_jmove(argc, argv)
int argc;
char **argv;
{
	struct joint *jp;
	int i;
	double tmp;

	/*
	 * find the joint.
	 */
	argv++;
	argc--;

	jp = joint_lookup(*argv);
	if (!jp) {
		rt_log("joint move: %s not found\n", *argv);
		return;
	}

	argv++;
	argc--;
	for (i=0; i<3 && argc; i++) {
		if (jp->rots[i].upper < jp->rots[i].lower) break;
		/*
		 * Eat a parameter, translate it from degrees to rads.
		 */
		if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
			++argv;
			--argc;
			continue;
		}
		tmp = atof(*argv);
		if (joint_debug & DEBUG_J_MOVE) {
			rt_log("joint move: %s rotate (%g %g %g) %g degrees.\n",
			    jp->name, jp->rots[i].quat[X],
			    jp->rots[i].quat[Y], jp->rots[i].quat[Z],
			    tmp);
			rt_log("joint move: %s lower=%g, upper=%g\n",
			    jp->name, jp->rots[i].lower, jp->rots[i].upper);
		}
		if (tmp <= jp->rots[i].upper &&
		    tmp >= jp->rots[i].lower) {
		    	jp->rots[i].current = tmp;
		} else {
			rt_log("joint move: %s, rotation %d, %s out of range.\n",
			    jp->name, i, *argv);
		}
		argv++;
		argc--;
	}
	for (i=0; i<3 && argc; i++) {
		if (jp->dirs[i].upper < jp->dirs[i].lower) break;
		/*
		 * eat a parameter.
		 */
		if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
			++argv;
			--argc;
			continue;
		}
		tmp = atof(*argv) * local2base;
		if (tmp <= jp->dirs[i].upper &&
		    tmp >= jp->dirs[i].lower) {
		    	jp->dirs[i].current = tmp;
		} else {
			rt_log("joint move: %s, vector %d, %s out of range.\n",
			    jp->name, i, *argv);
		}
	}
	joint_move(jp);
	f_jmesh(0,0);
}

struct artic_grips {
	struct rt_list	l;
	vect_t		vert;
	struct directory *dir;
};
#define	MAGIC_A_GRIP	0x414752aa
struct artic_joints {
	struct rt_list	l;
	struct rt_list	head;
	struct joint	*joint;
};
#define MAGIC_A_JOINT	0x414A4F55

struct rt_list artic_head = {
	RT_LIST_HEAD_MAGIC,
	&artic_head, &artic_head
};

struct joint *
findjoint(pathp)
struct db_full_path	*pathp;
{
	register int i,j;
	register struct joint *jp;
	int best;
	struct joint *bestjp;

	if (joint_debug & DEBUG_J_MESH) {
		char *sofar = db_path_to_string(pathp);
		rt_log("joint mesh: PATH = '%s'\n",sofar);
		rt_free(sofar, "path string");
	}

	best = -1;
	for (RT_LIST_FOR(jp, joint, &joint_head)) {
		for (i=0; i< pathp->fp_len; i++) {
			int good=1;
			if (jp->path.arc_last+i >= pathp->fp_len) break;
			for (j=0; j<=jp->path.arc_last;j++) {
				if ((*pathp->fp_names[i+j]->d_namep != *jp->path.arc[j]) ||
				    (strcmp(pathp->fp_names[i+j]->d_namep, jp->path.arc[j]) !=0)) {
				    	good=0;
				    	break;
				}
			}

			if (good && j>best) {
				best = j;
				bestjp = jp;
			}
		}
	}
	if (best > 0) {
	    	if (joint_debug & DEBUG_J_MESH) {
			rt_log("joint mesh: returning joint '%s'\n",
			    bestjp->name);
	    	}
		return bestjp;
 	}

	if (joint_debug & DEBUG_J_MESH) {
		rt_log("joint mesh: returning joint 'NULL'\n");
	}
	return (struct joint *) 0;
}

HIDDEN union tree *mesh_leaf( tsp, pathp, ep, id)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
struct rt_external	*ep;
int			id;
{
	struct	rt_db_internal	internal;
	struct rt_grip_internal *gip;
	struct	artic_joints	*newJoint;
	struct	artic_grips	*newGrip;
	struct	joint		*jp;
	union	tree		*curtree;
	struct	directory	*dp;

	GETUNION(curtree, tree);
	curtree->tr_op = OP_SOLID;

	if (id != ID_GRIP) {
		return curtree;
	}
	dp = pathp->fp_names[pathp->fp_len-1];
/*
 * get the grip information.
 */
	RT_INIT_DB_INTERNAL(&internal);
	if ( rt_functab[id].ft_import( &internal, ep, tsp->ts_mat) < 0 ) {
		rt_log("%s: solid import failure\n",
		    dp->d_namep);
		if (internal.idb_ptr) rt_functab[id].ft_ifree(&internal);
		return curtree;
	}
	gip = (struct rt_grip_internal *) internal.idb_ptr;
/*
 * find the joint that this grip belongs to.
 */
	jp = findjoint(pathp);
/*
 * Get the grip structure.
 */
	rt_functab[id].ft_ifree(&internal);

	newGrip = (struct artic_grips *)rt_malloc(sizeof(struct artic_grips),
	    "artic_grip");
	newGrip->l.magic = MAGIC_A_GRIP;
	VMOVE(newGrip->vert, gip->center);
	newGrip->dir = pathp->fp_names[pathp->fp_len-1];
	for (RT_LIST_FOR(newJoint, artic_joints, &artic_head)) {
		if (newJoint->joint == jp) {
			RT_LIST_APPEND(&newJoint->head, &(newGrip->l));
			return curtree;
		}
	}
/*
 * we need a new joint thingie.
 */
	newJoint = (struct artic_joints *)rt_malloc(sizeof(struct artic_joints),
	    "Artic Joint");
	newJoint->l.magic = MAGIC_A_JOINT;
	newJoint->joint = jp;
	RT_LIST_INIT(&newJoint->head);
	RT_LIST_APPEND(&artic_head, &(newJoint->l));
	RT_LIST_APPEND(&newJoint->head, &(newGrip->l));

	return curtree;
}
HIDDEN union tree *mesh_end_region (tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path		*pathp;
union tree			*curtree;
{
	return curtree;
}
static struct db_tree_state mesh_initial_tree_state = {
	0,			/* ts_dbip */
	0,			/* ts_sofar */
	0,0,0,			/* region, air, gmater */
	100,			/* GIFT los */
#if __STDC__
	{
#endif
		/* struct mater_info ts_mater */
		1.0, 0.0, 0.0,	/* color, RGB */
		0,		/* override */
		DB_INH_LOWER,	/* color inherit */
		DB_INH_LOWER,	/* mater inherit */
		"",		/* material name */
		""		/* material params */
#if __STDC__
	}
#endif
	,
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};
void
f_jmesh(argc, argv)
int argc;
char **argv;
{
	char			*name;
	struct rt_vlblock	*vbp;
	register struct rt_list *vhead;
	struct artic_joints	*jp;
	struct artic_grips	*gp, *gpp;
	int i;
	char			**topv[2000];
	int			topc;

	if( argc <= 2) {
		name = "_ANIM_";
	} else {
		name = argv[2];
	}

	topc = build_tops(topv, topv+2000);
	{
		register struct solid *sp;
		FOR_ALL_SOLIDS(sp) {
			sp->s_iflag=DOWN;
		}
	}

	i = db_walk_tree(dbip, topc, (CONST char **)topv,
	    1,			/* Number of cpus */
	    &mesh_initial_tree_state,
	    0,			/* Begin region */
	    mesh_end_region,	/* End region */
	    mesh_leaf);		/* node */

	/*
	 * Now we draw the the overlays.  We do this by building a 
	 * mesh from each grip to every other grip in that list.
	 */
	vbp = rt_vlblock_init();
	vhead = rt_vlblock_find( vbp, 0x00, 0xff, 0xff);

	for (RT_LIST_FOR(jp, artic_joints, &artic_head)) {
		i=0;
		for (RT_LIST_FOR(gp, artic_grips, &jp->head)){
			i++;
			for (gpp=RT_LIST_NEXT(artic_grips, &(gp->l));
			    RT_LIST_NOT_HEAD(gpp, &(jp->head));
			    gpp=RT_LIST_NEXT(artic_grips, &(gpp->l))) {
				RT_ADD_VLIST( vhead, gp->vert, RT_VLIST_LINE_MOVE);
				RT_ADD_VLIST( vhead, gpp->vert, RT_VLIST_LINE_DRAW);
			}
		}
		if (joint_debug & DEBUG_J_MESH) {
			rt_log("joint mesh: %s has %d grips.\n",
			    (jp->joint) ? jp->joint->name: "UNGROUPED",i);
		}
	}

	cvt_vlblock_to_solids(vbp, name, 0);
	rt_vlblock_free(vbp);
	while (RT_LIST_WHILE(jp, artic_joints, &artic_head)) {
		while (RT_LIST_WHILE(gp, artic_grips, &jp->head)) {
			RT_LIST_DEQUEUE(&gp->l);
			rt_free((char *)gp,"artic_grip");
		}
		RT_LIST_DEQUEUE(&jp->l);
		rt_free((char *)jp, "Artic Joint");
	}
}
