#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "vmath.h"
#include "bu/avs.h"
#include "bu/path.h"
#include "brep.h"
#include "wdb.h"
#include "analyze.h"
#include "ged.h"
#include "../libbrep/shape_recognition.h"

#define ptout(p)  p.x << " " << p.y << " " << p.z

#define INFORMATION_ATTRS_ON 0

HIDDEN void
set_attr_key(struct rt_wdb *wdbp, const char *obj_name, const char *key, int array_cnt, int *array)
{
    struct bu_vls val = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_set avs;
    struct directory *dp;

    if (!wdbp || !obj_name || !key || !array) return;

    dp = db_lookup(wdbp->dbip, obj_name, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) return;

    set_key(&val, array_cnt, array);

    bu_avs_init_empty(&avs);

    if (db5_get_attributes(wdbp->dbip, &avs, dp)) return;

    (void)bu_avs_add(&avs, key, bu_vls_addr(&val));

#ifdef INFORMATION_ATTRS_ON
    (void)db5_replace_attributes(dp, &avs, wdbp->dbip);
#endif

    bu_avs_free(&avs);
    bu_vls_free(&val);
}

HIDDEN void
subbrep_obj_name(int type, int id, const char *root, struct bu_vls *name)
{
    if (!root || !name) return;
    switch (type) {
	case ARB6:
	    bu_vls_printf(name, "%s-arb6_%d.s", root, id);
	    return;
	case ARB8:
	    bu_vls_printf(name, "%s-arb8_%d.s", root, id);
	    return;
	case ARBN:
	    bu_vls_printf(name, "%s-arbn_%d.s", root, id);
	    return;
	case PLANAR_VOLUME:
	    bu_vls_printf(name, "%s-bot_%d.s", root, id);
	    return;
	case CYLINDER:
	    bu_vls_printf(name, "%s-rcc_%d.s", root, id);
	    return;
	case CONE:
	    bu_vls_printf(name, "%s-trc_%d.s", root, id);
	    return;
	case SPHERE:
	    bu_vls_printf(name, "%s-sph_%d.s", root, id);
	    return;
	case ELLIPSOID:
	    bu_vls_printf(name, "%s-ell_%d.s", root, id);
	    return;
	case TORUS:
	    bu_vls_printf(name, "%s-tor_%d.s", root, id);
	    return;
	case COMB:
	    bu_vls_printf(name, "%s-comb_%d.c", root, id);
	    return;
	case BREP:
	    bu_vls_printf(name, "%s-brep_%d.s", root, id);
	    break;
	default:
	    bu_vls_printf(name, "%s-%d.c", root, id);
	    break;
    }
}


HIDDEN int
subbrep_to_csg_arbn(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == ARBN) {
	/* Make the arbn, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_arbn(wdbp, bu_vls_addr(&prim_name), data->plane_cnt, data->planes)) {
	    if (msgs) bu_vls_printf(msgs, "mk_arbn failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_planar(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == PLANAR_VOLUME) {
	/* Make the bot, using the data key for a unique name */
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_bot(wdbp, bu_vls_addr(&prim_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, data->csg_vert_cnt, data->csg_face_cnt, (fastf_t *)data->csg_verts, data->csg_faces, (fastf_t *)NULL, (struct bu_bitv *)NULL)) {
	    if (msgs) bu_vls_printf(msgs, "mk_bot failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}

HIDDEN int
subbrep_to_csg_cylinder(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_rcc(wdbp, bu_vls_addr(&prim_name), data->origin, data->hv, data->radius)) {
	    if (msgs) bu_vls_printf(msgs, "mk_rcc failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_cone(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == CONE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_cone(wdbp, bu_vls_addr(&prim_name), data->origin, data->hv, data->height, data->radius, data->r2)) {
	    if (msgs) bu_vls_printf(msgs, "mk_trc failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN int
subbrep_to_csg_sph(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
    if (!msgs || !data || !wdbp || !pname) return 0;
    if (data->csg_type == SPHERE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
	if (mk_sph(wdbp, bu_vls_addr(&prim_name), data->origin, data->radius)) {
	    if (msgs) bu_vls_printf(msgs, "mk_sph failed for %s\n", bu_vls_addr(&prim_name));
	} else {
	    set_attr_key(wdbp, bu_vls_addr(&prim_name), "loops", data->s->shoal_loops_cnt, data->s->shoal_loops);
	}
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

HIDDEN void
csg_obj_process(struct bu_vls *msgs, struct csg_object_params *data, struct rt_wdb *wdbp, const char *pname)
{
#if 0
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    subbrep_obj_name(data->csg_type, data->csg_id, pname, &prim_name);
    struct directory *dp = db_lookup(wdbp->dbip, bu_vls_addr(&prim_name), LOOKUP_QUIET);
    // Don't recreate it
    if (dp != RT_DIR_NULL) {
	bu_log("already made %s\n", bu_vls_addr(data->obj_name));
	return;
    }
#endif

    switch (data->csg_type) {
	case ARB6:
	    //subbrep_to_csg_arb6(data, wdbp, pname, wcomb);
	    break;
	case ARB8:
	    //subbrep_to_csg_arb8(data, wdbp, pname, wcomb);
	    break;
	case ARBN:
	    subbrep_to_csg_arbn(msgs, data, wdbp, pname);
	    break;
	case PLANAR_VOLUME:
	    subbrep_to_csg_planar(msgs, data, wdbp, pname);
	    break;
	case CYLINDER:
	    subbrep_to_csg_cylinder(msgs, data, wdbp, pname);
	    break;
	case CONE:
	    subbrep_to_csg_cone(msgs, data, wdbp, pname);
	    break;
	case SPHERE:
	    subbrep_to_csg_sph(msgs, data, wdbp, pname);
	    break;
	case ELLIPSOID:
	    break;
	case TORUS:
	    break;
	default:
	    break;
    }
}

#define BOOL_RESOLVE(_a, _b) (_b == '+') ? &isect : ((_a == '-' && _b == '-') || (_a == 'u' && _b == 'u')) ? &un : &sub

HIDDEN int
make_shoal(struct bu_vls *msgs, struct subbrep_shoal_data *data, struct rt_wdb *wdbp, const char *rname)
{
    char un = 'u';
    char sub = '-';
    char isect = '+';

    struct wmember wcomb;
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    if (!data || !data->params) {
	if (msgs) bu_vls_printf(msgs, "Error! invalid shoal.\n");
	return 0;
    }

    //struct bu_vls key = BU_VLS_INIT_ZERO;
    //set_key(&key, data->shoal_loops_cnt, data->shoal_loops);
    //bu_log("Processing shoal %s from island %s\n", bu_vls_addr(&key), bu_vls_addr(data->i->key));

    if (data->shoal_type == COMB) {
	BU_LIST_INIT(&wcomb.l);
	subbrep_obj_name(data->shoal_type, data->shoal_id, rname, &comb_name);
	//bu_log("Created %s\n", bu_vls_addr(&comb_name));
	csg_obj_process(msgs, data->params, wdbp, rname);
	subbrep_obj_name(data->params->csg_type, data->params->csg_id, rname, &prim_name);
	//bu_log("  %c %s\n", data->params->bool_op, bu_vls_addr(&prim_name));
	(void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(&(data->params->bool_op)));
	for (unsigned int i = 0; i < BU_PTBL_LEN(data->shoal_children); i++) {
	    struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(data->shoal_children, i);
	    char *bool_op = BOOL_RESOLVE(data->params->bool_op, c->bool_op);
	    csg_obj_process(msgs, c, wdbp, rname);
	    bu_vls_trunc(&prim_name, 0);
	    subbrep_obj_name(c->csg_type, c->csg_id, rname, &prim_name);
	    //bu_log("     %c(%c) %s\n", c->bool_op, *bool_op, bu_vls_addr(&prim_name));
	    (void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, db_str2op(bool_op));
	}
	mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);
	set_attr_key(wdbp, bu_vls_addr(&comb_name), "loops", data->shoal_loops_cnt, data->shoal_loops);
    } else {
	subbrep_obj_name(data->params->csg_type, data->params->csg_id, rname, &prim_name);
	csg_obj_process(msgs, data->params, wdbp, rname);
	//bu_log("Created %s\n", bu_vls_addr(&prim_name));
    }
    return 1;
}

HIDDEN int
make_island(struct bu_vls *msgs, struct subbrep_island_data *data, struct rt_wdb *wdbp, const char *rname, struct wmember *pcomb)
{
    struct wmember icomb;
    struct wmember ucomb;
    struct wmember scomb;

    int failed = 0;
    char un = 'u';
    char sub = '-';
    char isect = '+'; // UNUSED here, for BOOL_RESOLVE macro.

    char *n_bool_op;
    if (data->island_type == BREP) {
	n_bool_op = &(data->local_brep_bool_op);
    } else {
	n_bool_op = &(data->nucleus->params->bool_op);
    }
    struct bu_vls island_name = BU_VLS_INIT_ZERO;
    subbrep_obj_name(-1, data->island_id, rname, &island_name);

#if 0
    if (*n_bool_op == 'u') {
	bu_log("Processing island %s\n", bu_vls_addr(&island_name));
    } else {
	bu_log("Making negative island %s\n", bu_vls_addr(&island_name));
    }
#endif

    struct bu_vls shoal_name = BU_VLS_INIT_ZERO;
    struct bu_vls union_name = BU_VLS_INIT_ZERO;
    struct bu_vls sub_name = BU_VLS_INIT_ZERO;

    BU_LIST_INIT(&icomb.l);
    BU_LIST_INIT(&ucomb.l);
    bu_vls_sprintf(&union_name, "%s-unions", bu_vls_addr(&island_name));
    BU_LIST_INIT(&scomb.l);
    bu_vls_sprintf(&sub_name, "%s-subtractions", bu_vls_addr(&island_name));

    bu_vls_trunc(&shoal_name, 0);
    if (data->island_type == BREP) {
	subbrep_obj_name(data->island_type, data->island_id, rname, &shoal_name);
	mk_brep(wdbp, bu_vls_addr(&shoal_name), (void *)(data->local_brep));
	n_bool_op = &(data->local_brep_bool_op);
    } else {
	subbrep_obj_name(data->nucleus->shoal_type, data->nucleus->shoal_id, rname, &shoal_name);
	if (!make_shoal(msgs, data->nucleus, wdbp, rname)) failed++;
    }

    // In a comb, the first element is always unioned.  The nucleus bool op is applied
    // to the overall shoal in the pcomb assembly.
    (void)mk_addmember(bu_vls_addr(&shoal_name), &(ucomb.l), NULL, db_str2op(&un));
    char *bool_op;
    int union_shoal_cnt = 1;
    int subtraction_shoal_cnt = 0;

    // Find and handle union shoals first.
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	bool_op = BOOL_RESOLVE(*n_bool_op, d->params->bool_op);
	if (*bool_op == 'u') {
	    bu_vls_trunc(&shoal_name, 0);
	    subbrep_obj_name(d->shoal_type, d->shoal_id, rname, &shoal_name);
	    //if (*n_bool_op == 'u') {
	    //  bu_log("  unioning: %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&shoal_name));
	    //}
	    if (!make_shoal(msgs, d, wdbp, rname)) failed++;
	    (void)mk_addmember(bu_vls_addr(&shoal_name), &(ucomb.l), NULL, db_str2op(bool_op));
	    union_shoal_cnt++;
	}
    }

    if (union_shoal_cnt == 1) {
	bu_vls_sprintf(&union_name, "%s", bu_vls_addr(&shoal_name));
	//bu_log("single union name: %s\n", bu_vls_addr(&union_name));
    }

    // Have unions, get subtractions.
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	bool_op = BOOL_RESOLVE(*n_bool_op, d->params->bool_op);
	if (*bool_op == '-') {
	    bu_vls_trunc(&shoal_name, 0);
	    subbrep_obj_name(d->shoal_type, d->shoal_id, rname, &shoal_name);
	    //if (*n_bool_op == 'u') {
		//bu_log("  subtracting: %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&shoal_name));
	    //}
	    if (!make_shoal(msgs, d, wdbp, rname)) failed++;
	    (void)mk_addmember(bu_vls_addr(&shoal_name), &(scomb.l), NULL, db_str2op(&un));
	    subtraction_shoal_cnt++;
	}
    }
    // Handle subtractions
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->subtractions); i++) {
	struct subbrep_island_data *n = (struct subbrep_island_data *)BU_PTBL_GET(data->subtractions, i);
	struct bu_vls subtraction_name = BU_VLS_INIT_ZERO;
	subbrep_obj_name(-1, n->island_id, rname, &subtraction_name);
	//if (*n_bool_op == 'u') {
	//  bu_log("  subtraction found for %s: %s\n", bu_vls_addr(&island_name), bu_vls_addr(&subtraction_name));
	//}
	(void)mk_addmember(bu_vls_addr(&subtraction_name), &(scomb.l), NULL, db_str2op(&un));
	bu_vls_free(&subtraction_name);
	subtraction_shoal_cnt++;
    }

    if (union_shoal_cnt > 1) {
	mk_lcomb(wdbp, bu_vls_addr(&union_name), &ucomb, 0, NULL, NULL, NULL, 0);
    }
    (void)mk_addmember(bu_vls_addr(&union_name), &(icomb.l), NULL, db_str2op(&un));
    if (subtraction_shoal_cnt) {
	mk_lcomb(wdbp, bu_vls_addr(&sub_name), &scomb, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&sub_name), &(icomb.l), NULL, db_str2op(&sub));
    }
    mk_lcomb(wdbp, bu_vls_addr(&island_name), &icomb, 0, NULL, NULL, NULL, 0);
    set_attr_key(wdbp, bu_vls_addr(&island_name), "loops", data->island_loops_cnt, data->island_loops);
    set_attr_key(wdbp, bu_vls_addr(&island_name), "faces", data->island_faces_cnt, data->island_faces);

    if (*n_bool_op == 'u')
	(void)mk_addmember(bu_vls_addr(&island_name), &(pcomb->l), NULL, db_str2op(n_bool_op));

    // Debugging B-Reps - generates a B-Rep object for each island
#if 0
    if (data->local_brep) {
	unsigned char rgb[3];
	struct wmember bcomb;
	struct bu_vls bcomb_name = BU_VLS_INIT_ZERO;
	struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bcomb_name, "%s-brep_obj_%d.r", rname, data->island_id);
	BU_LIST_INIT(&bcomb.l);

	if (*n_bool_op == 'u') {
	    rgb[0] = static_cast<unsigned char>(0);
	    rgb[1] = static_cast<unsigned char>(0);
	    rgb[2] = static_cast<unsigned char>(255.0);
	} else {
	    rgb[0] = static_cast<unsigned char>(255.0);
	    rgb[1] = static_cast<unsigned char>(0);
	    rgb[2] = static_cast<unsigned char>(0);
	}
	//for (int i = 0; i < 3; ++i)
	//    rgb[i] = static_cast<unsigned char>(255.0 * drand48() + 0.5);

	bu_vls_sprintf(&brep_name, "%s-brep_obj_%d.s", rname, data->island_id);
	mk_brep(wdbp, bu_vls_addr(&brep_name), (void *)(data->local_brep));

	(void)mk_addmember(bu_vls_addr(&brep_name), &(bcomb.l), NULL, db_str2op((const char *)&un));
	mk_lcomb(wdbp, bu_vls_addr(&bcomb_name), &bcomb, 1, "plastic", "di=.8 sp=.2", rgb, 0);

	bu_vls_free(&brep_name);
	bu_vls_free(&bcomb_name);
    }
#endif


    bu_vls_free(&island_name);
    return 0;
}

/* return codes:
 * -1 get internal failure
 *  0 success
 *  1 not a brep
 *  2 not a valid brep
 */
int
_obj_brep_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *UNUSED(ito), struct directory *dp, int verify, struct bu_vls *retname)
{
    /* Unpack B-Rep */
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    RT_DB_INTERNAL_INIT(&intern)
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return -1;
    }
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_vls_printf(log, "%s is not a B-Rep - aborting\n", dp->d_namep);
	return 1;
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);
#if 0
    if (!rt_brep_valid(&intern, NULL)) {
	bu_vls_printf(log, "%s is not a valid B-Rep - aborting\n", dp->d_namep);
	return 2;
    }
#endif
    ON_Brep *brep = brep_ip->brep;

    struct wmember pcomb;
    struct bu_vls core_name = BU_VLS_INIT_ZERO;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    struct bu_vls root_name = BU_VLS_INIT_ZERO;
    bu_path_component(&core_name, dp->d_namep, BU_PATH_BASENAME_EXTLESS);
    bu_vls_sprintf(&root_name, "%s-csg", bu_vls_addr(&core_name));
    bu_vls_sprintf(&comb_name, "csg_%s.c", bu_vls_addr(&core_name));
    if (retname) bu_vls_sprintf(retname, "%s", bu_vls_addr(&comb_name));

    // Only do this if we haven't already done it - tree walking may
    // result in multiple references to a single object
    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&comb_name), LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_printf(log, "Converting %s to %s\n", dp->d_namep, bu_vls_addr(&comb_name));

	BU_LIST_INIT(&pcomb.l);

	struct bu_ptbl *subbreps = brep_to_csg(log, brep);

	if (subbreps) {
	    int have_non_breps = 0;
	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		struct subbrep_island_data *sb = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
		if (sb->island_type != BREP) have_non_breps++;
	    }
	    if (!have_non_breps) return 2;

	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		struct subbrep_island_data *sb = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
		make_island(log, sb, wdbp, bu_vls_addr(&root_name), &pcomb);
	    }
	    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++) {
		// free islands;
	    }

	    // Only do a combination if the comb structure has more than one entry in the list.
	    struct bu_list *olist;
	    int comb_objs = 0;
	    for (BU_LIST_FOR(olist, bu_list, &pcomb.l)) comb_objs++;
	    if (comb_objs > 1) {
		// We're not setting the region flag here in case there is a hierarchy above us that
		// takes care of it.  TODO - support knowing whether that's true and doing the right thing.
		mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);
	    } else {
		// TODO - Fix up name of first item in list to reflect top level naming to
		// avoid an unnecessary level of hierarchy.
		//bu_log("only one level... - %s\n", ((struct wmember *)pcomb.l.forw)->wm_name);
		/*
		int ac = 3;
		const char **av = (const char **)bu_calloc(4, sizeof(char *), "killtree argv");
		av[0] = "mv";
		av[1] = ((struct wmember *)pcomb.l.forw)->wm_name;
		av[2] = bu_vls_addr(&comb_name);
		av[3] = (char *)0;
		(void)ged_move(gedp, ac, av);
		bu_free(av, "free av array");
		bu_vls_sprintf(&comb_name, "%s", ((struct wmember *)pcomb.l.forw)->wm_name);
		*/
		mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);
	    }

	    // Verify that the resulting csg tree and the original B-Rep pass a difference test.
	    if (verify) {
		ON_BoundingBox bbox;
		struct bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
		brep->GetBoundingBox(bbox);
		tol.dist = (bbox.Diagonal().Length() / 100.0);
		bu_vls_printf(log, "Analyzing %s csg conversion, tol %f...\n", dp->d_namep, tol.dist);
		if (analyze_raydiff(NULL, gedp->ged_wdbp->dbip, dp->d_namep, bu_vls_addr(&comb_name), &tol, 1)) {
		    /* remove generated tree if debugging flag isn't passed - not valid */
		    int ac = 3;
		    const char **av = (const char **)bu_calloc(4, sizeof(char *), "killtree argv");
		    av[0] = "killtree";
		    av[1] = "-f";
		    av[2] = bu_vls_addr(&comb_name);
		    av[3] = (char *)0;
		    (void)ged_killtree(gedp, ac, av);
		    bu_free(av, "free av array");
		    bu_vls_printf(log, "Error: %s did not pass diff test at tol %f, rejecting\n", bu_vls_addr(&comb_name), tol.dist);
		    return 2;
		}
	    }
	} else {
	    return 2;
	}
    } else {
	bu_vls_printf(log, "Conversion object %s for %s already exists, skipping.\n", bu_vls_addr(&comb_name), dp->d_namep);
    }
    return 0;
}







/*************************************************/
/*               Tree walking, etc.              */
/*************************************************/

int comb_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, struct directory *dp, int verify);

int
brep_csg_conversion_tree(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, const union tree *oldtree, union tree *newtree, int verify)
{
    int ret = 0;
    int brep_c;
    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct bu_vls newname = BU_VLS_INIT_ZERO;
    char *oldname = NULL;
    *newtree = *oldtree;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    //bu_log("convert right\n");
	    newtree->tr_b.tb_right = new tree;
	    RT_TREE_INIT(newtree->tr_b.tb_right);
	    ret = brep_csg_conversion_tree(gedp, log, ito, oldtree->tr_b.tb_right, newtree->tr_b.tb_right, verify);
#if 0
	    if (ret) {
		delete newtree->tr_b.tb_right;
		break;
	    }
#endif
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    //bu_log("convert left\n");
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret = brep_csg_conversion_tree(gedp, log, ito, oldtree->tr_b.tb_left, newtree->tr_b.tb_left, verify);
#if 0
	    if (ret) {
		delete newtree->tr_b.tb_left;
		delete newtree->tr_b.tb_right;
	    }
#endif
	    break;
	case OP_DB_LEAF:
	    oldname = oldtree->tr_l.tl_name;
	    bu_vls_sprintf(&tmpname, "csg_%s", oldname);
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) == RT_DIR_NULL) {
		struct directory *dir = db_lookup(gedp->ged_wdbp->dbip, oldname, LOOKUP_QUIET);

		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			ret = comb_to_csg(gedp, log, ito, dir, verify);
			if (!ret) {
			    newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
			    bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
			}
			bu_vls_free(&tmpname);
		    } else {
			// It's a primitive. If it's a b-rep object, convert it. Otherwise,
			// just duplicate it. Might need better error codes from _obj_brep_to_csg for this...
			brep_c = _obj_brep_to_csg(gedp, log, ito, dir, verify, &newname);
			switch (brep_c) {
			    case 0:
				bu_vls_printf(log, "processed brep %s.\n", bu_vls_addr(&newname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&newname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&newname), strlen(bu_vls_addr(&newname))+1);
				bu_vls_free(&newname);
				break;
			    case 1:
				bu_vls_printf(log, "non brep solid %s.\n", bu_vls_addr(&tmpname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
				break;
			    case 2:
				bu_vls_printf(log, "unconverted brep %s.\n", bu_vls_addr(&tmpname));
				newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
				bu_strlcpy(newtree->tr_l.tl_name, oldname, strlen(oldname)+1);
				break;
			    default:
				bu_vls_printf(log, "what?? %s.\n", bu_vls_addr(&tmpname));
				break;
			}
		    }
		} else {
		    bu_vls_printf(log, "Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret = -1;
		}
	    } else {
		bu_vls_printf(log, "%s already exists.\n", bu_vls_addr(&tmpname));
		newtree->tr_l.tl_name = (char*)bu_malloc(strlen(bu_vls_addr(&tmpname))+1, "char");
		bu_strlcpy(newtree->tr_l.tl_name, bu_vls_addr(&tmpname), strlen(bu_vls_addr(&tmpname))+1);
	    }
	    bu_vls_free(&tmpname);
	    break;
	default:
	    bu_log("huh??\n");
	    break;
    }
    return 1;
}


int
comb_to_csg(struct ged *gedp, struct bu_vls *log, struct bu_attribute_value_set *ito, struct directory *dp, int verify)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb_internal = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "csg_%s", dp->d_namep);

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return -1;
    }

    RT_CK_COMB(intern.idb_ptr);
    comb_internal = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb_internal->tree == NULL) {
	// Empty tree
	(void)wdb_export(wdbp, bu_vls_addr(&comb_name), comb_internal, ID_COMBINATION, 1);
	return 0;
    }

    RT_CK_TREE(comb_internal->tree);
    union tree *oldtree = comb_internal->tree;
    struct rt_comb_internal *new_internal;

    BU_ALLOC(new_internal, struct rt_comb_internal);
    *new_internal = *comb_internal;
    BU_ALLOC(new_internal->tree, union tree);
    RT_TREE_INIT(new_internal->tree);

    union tree *newtree = new_internal->tree;

    (void)brep_csg_conversion_tree(gedp, log, ito, oldtree, newtree, verify);
    (void)wdb_export(wdbp, bu_vls_addr(&comb_name), (void *)new_internal, ID_COMBINATION, 1);

    return 0;
}

extern "C" int
_ged_brep_to_csg(struct ged *gedp, const char *dp_name, int verify)
{
    struct bu_attribute_value_set ito = BU_AVS_INIT_ZERO; /* islands to objects */
    int ret = 0;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct directory *dp = db_lookup(wdbp->dbip, dp_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return GED_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	ret = comb_to_csg(gedp, &log, &ito, dp, verify) ? GED_ERROR : GED_OK;
    } else {
	ret = _obj_brep_to_csg(gedp, &log, &ito, dp, verify, NULL) ? GED_ERROR : GED_OK;
    }

    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_addr(&log));
    bu_vls_free(&log);
    return ret;
}



void tikz_comb(struct ged *gedp, struct bu_vls *tikz, struct directory *dp, struct bu_vls *color, int *cnt);

int
tikz_tree(struct ged *gedp, struct bu_vls *tikz, const union tree *oldtree, struct bu_vls *color, int *cnt)
{
    int ret = 0;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    ret = tikz_tree(gedp, tikz, oldtree->tr_b.tb_right, color, cnt);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    ret = tikz_tree(gedp, tikz, oldtree->tr_b.tb_left, color, cnt);
	    break;
	case OP_DB_LEAF:
	    {
		struct directory *dir = db_lookup(gedp->ged_wdbp->dbip, oldtree->tr_l.tl_name, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    if (dir->d_flags & RT_DIR_COMB) {
			tikz_comb(gedp, tikz, dir, color, cnt);
		    } else {
			// It's a primitive. If it's a brep, get the wireframe.
			// TODO - support wireframes from other primitive types...
			struct rt_db_internal bintern;
			struct rt_brep_internal *b_ip = NULL;
			RT_DB_INTERNAL_INIT(&bintern)
			if (rt_db_get_internal(&bintern, dir, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
			    return GED_ERROR;
			}
			if (bintern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
			    ON_String s;
			    struct bu_vls cntstr = BU_VLS_INIT_ZERO;
			    (*cnt)++;
			    bu_vls_sprintf(&cntstr, "OBJ%d", *cnt);
			    b_ip = (struct rt_brep_internal *)bintern.idb_ptr;
			    (void)ON_BrepTikz(s, b_ip->brep, bu_vls_addr(color), bu_vls_addr(&cntstr));
			    const char *str = s.Array();
			    bu_vls_strcat(tikz, str);
			    bu_vls_free(&cntstr);
			}
		    }
		}
	    }
	    break;
	default:
	    bu_log("huh??\n");
	    break;
    }
    return ret;
}

void
tikz_comb(struct ged *gedp, struct bu_vls *tikz, struct directory *dp, struct bu_vls *color, int *cnt)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb_internal = NULL;
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct bu_vls color_backup = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&color_backup, "%s", bu_vls_addr(color));

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return;
    }

    RT_CK_COMB(intern.idb_ptr);
    comb_internal = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb_internal->tree == NULL) {
	// Empty tree
	return;
    }
    RT_CK_TREE(comb_internal->tree);
    union tree *t = comb_internal->tree;


    // Get color
    if (comb_internal->rgb[0] > 0 || comb_internal->rgb[1] > 0 || comb_internal->rgb[1] > 0) {
	bu_vls_sprintf(color, "color={rgb:red,%d;green,%d;blue,%d}", comb_internal->rgb[0], comb_internal->rgb[1], comb_internal->rgb[2]);
    }

    (void)tikz_tree(gedp, tikz, t, color, cnt);

    bu_vls_sprintf(color, "%s", bu_vls_addr(&color_backup));
    bu_vls_free(&color_backup);
}



extern "C" int
_ged_brep_tikz(struct ged *gedp, const char *dp_name, const char *outfile)
{
    int cnt = 0;
    struct bu_vls color = BU_VLS_INIT_ZERO;
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;
    RT_DB_INTERNAL_INIT(&intern)
    struct rt_wdb *wdbp = gedp->ged_wdbp;
    struct directory *dp = db_lookup(wdbp->dbip, dp_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return GED_ERROR;
    struct bu_vls tikz = BU_VLS_INIT_ZERO;

    /* Unpack B-Rep */
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	return GED_ERROR;
    }

    bu_vls_printf(&tikz, "\\documentclass{article}\n");
    bu_vls_printf(&tikz, "\\usepackage{tikz}\n");
    bu_vls_printf(&tikz, "\\usepackage{tikz-3dplot}\n\n");
    bu_vls_printf(&tikz, "\\begin{document}\n\n");
    // Translate view az/el into tikz-3dplot variation
    bu_vls_printf(&tikz, "\\tdplotsetmaincoords{%f}{%f}\n", 90 + -1*gedp->ged_gvp->gv_aet[1], -1*(-90 + -1 * gedp->ged_gvp->gv_aet[0]));

    // Need bbox dimensions to determine proper scale factor - do this with db_search so it will
    // work for combs as well, so long as there are no matrix transformations in the hierarchy.
    // Properly speaking this should be a bbox call in librt, but in this case we want the bbox of
    // everything - subtractions and unions.  Need to check if that's an option, and if not how
    // to handle it properly...
    ON_BoundingBox bbox;
    ON_MinMaxInit(&(bbox.m_min), &(bbox.m_max));
    struct bu_ptbl breps = BU_PTBL_INIT_ZERO;
    const char *brep_search = "-type brep";
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);
    (void)db_search(&breps, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, brep_search, 1, &dp, gedp->ged_wdbp->dbip, NULL);
    for(size_t i = 0; i < BU_PTBL_LEN(&breps); i++) {
	struct rt_db_internal bintern;
	struct rt_brep_internal *b_ip = NULL;
	RT_DB_INTERNAL_INIT(&bintern)
	struct directory *d = (struct directory *)BU_PTBL_GET(&breps, i);
	if (rt_db_get_internal(&bintern, d, wdbp->dbip, NULL, &rt_uniresource) < 0) {
	    return GED_ERROR;
	}
	b_ip = (struct rt_brep_internal *)bintern.idb_ptr;
	b_ip->brep->GetBBox(bbox[0], bbox[1], true);
    }
    // Get things roughly down to page size - not perfect, but establishes a ballpark that can be fine tuned
    // by hand after generation
    double scale = 100/bbox.Diagonal().Length();

    bu_vls_printf(&tikz, "\\begin{tikzpicture}[scale=%f,tdplot_main_coords]\n", scale);

    if (dp->d_flags & RT_DIR_COMB) {
	// Assign a default color
	bu_vls_sprintf(&color, "color={rgb:red,255;green,0;blue,0}");
	tikz_comb(gedp, &tikz, dp, &color, &cnt);
    } else {
	ON_String s;
	if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a B-Rep - aborting\n", dp->d_namep);
	    return 1;
	} else {
	    brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
	}
	RT_BREP_CK_MAGIC(brep_ip);
	const ON_Brep *brep = brep_ip->brep;
	(void)ON_BrepTikz(s, brep, NULL, NULL);
	const char *str = s.Array();
	bu_vls_strcat(&tikz, str);
    }

    bu_vls_printf(&tikz, "\\end{tikzpicture}\n\n");
    bu_vls_printf(&tikz, "\\end{document}\n");

    if (outfile) {
	FILE *fp = fopen(outfile, "w");
	fprintf(fp, "%s", bu_vls_addr(&tikz));
	fclose(fp);
	bu_vls_free(&tikz);
	bu_vls_sprintf(gedp->ged_result_str, "Output written to file %s", outfile);
    } else {

	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_addr(&tikz));
	bu_vls_free(&tikz);
    }
	bu_vls_free(&tikz);
    return GED_OK;
}

// TODO - this doesn't belong here, just convenient for now since we need to crack the ON_Brep for this
extern "C" int
_ged_brep_flip(struct ged *gedp, struct rt_brep_internal *bi, const char *obj_name)
{
    const char *av[3];
    if (!gedp || !bi || !obj_name) return GED_ERROR;
    bi->brep->Flip();

    // Delete the old object
    av[0] = "kill";
    av[1] = obj_name;
    av[2] = NULL;
    (void)ged_kill(gedp, 2, (const char **)av);

    // Make the new one
    if (mk_brep(gedp->ged_wdbp, obj_name, (void *)bi->brep)) {
	return GED_ERROR;
    }
    return GED_OK;
}


// TODO - this doesn't belong here, just convenient for now since we need to crack the ON_Brep for this
extern "C" int
_ged_brep_pick_face(struct ged *gedp, struct rt_brep_internal *bi, const char *obj_name)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    vect_t xlate;
    vect_t eye;
    vect_t dir;

    GED_CHECK_VIEW(gedp, GED_ERROR);
    VSET(xlate, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye, gedp->ged_gvp->gv_view2model, xlate);
    VSCALE(eye, eye, gedp->ged_wdbp->dbip->dbi_base2local);

    VMOVEN(dir, gedp->ged_gvp->gv_rotation + 8, 3);
    VSCALE(dir, dir, -1.0);

    bu_vls_sprintf(&log, "%s:\n", obj_name);
    if (ON_Brep_Report_Faces(&log, (void *)bi->brep, eye, dir)) {
	bu_vls_free(&log);
	return GED_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&log));
    bu_vls_free(&log);
    return GED_OK;
}

// TODO - this doesn't belong here, just convenient for now since we need to crack the ON_Brep for this
extern "C" int
_ged_brep_shrink_surfaces(struct ged *gedp, struct rt_brep_internal *bi, const char *obj_name)
{
    const char *av[3];
    if (!gedp || !bi || !obj_name) return GED_ERROR;
    bi->brep->ShrinkSurfaces();

    // Delete the old object
    av[0] = "kill";
    av[1] = obj_name;
    av[2] = NULL;
    (void)ged_kill(gedp, 2, (const char **)av);

    // Make the new one
    if (mk_brep(gedp->ged_wdbp, obj_name, (void *)bi->brep)) {
	return GED_ERROR;
    }
    return GED_OK;
}

// TODO - this doesn't belong here, just convenient for now since we need to crack the ON_Brep for this
extern "C" int
_ged_brep_to_bot(struct ged *gedp, const char *obj_name, const struct rt_brep_internal *bi, const char *bot_name, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!gedp || !bi || !bot_name || !ttol || !tol) return GED_ERROR;

    int fcnt, fncnt, ncnt, vcnt;
    int *faces = NULL;
    fastf_t *vertices = NULL;
    int *face_normals = NULL;
    fastf_t *normals = NULL;

    struct bg_tess_tol cdttol;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;
    ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, obj_name);
    ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
    ON_Brep_CDT_Tessellate(s_cdt, 0, NULL);
    ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, s_cdt, 0, NULL);
    ON_Brep_CDT_Destroy(s_cdt);

    struct rt_bot_internal *bot;
    BU_GET(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;
    bot->num_vertices = vcnt;
    bot->num_faces = fcnt;
    bot->vertices = vertices;
    bot->faces = faces;
    bot->thickness = NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->num_normals = ncnt;
    bot->num_face_normals = fncnt;
    bot->normals = normals;
    bot->face_normals = face_normals;

    if (wdb_export(gedp->ged_wdbp, bot_name, (void *)bot, ID_BOT, 1.0)) {
	return GED_ERROR;
    }

    return GED_OK;
}

// TODO - this doesn't belong here, just convenient for now since we need to crack the ON_Brep for this
//
// Right now this is just a quick and dirty function to exercise the libbrep logic...
extern "C" int
_ged_breps_to_bots(struct ged *gedp, int obj_cnt, const char **obj_names, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!gedp || obj_cnt <= 0 || !obj_names || !ttol || !tol) return GED_ERROR;

    struct bg_tess_tol cdttol;
    cdttol.abs = ttol->abs;
    cdttol.rel = ttol->rel;
    cdttol.norm = ttol->norm;

    std::vector<ON_Brep_CDT_State *> ss_cdt;
    std::vector<std::string> bot_names;
    std::vector<struct rt_brep_internal *> o_bi;

    // Set up
    for (int i = 0; i < obj_cnt; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, obj_names[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", obj_names[i]);
	    return GED_ERROR;
	}
	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", obj_names[i]);
	    return GED_ERROR;
	}

	std::string bname = std::string(obj_names[i]) + std::string("-bot");
	ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, obj_names[i]);
	ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
	o_bi.push_back(bi);
	ss_cdt.push_back(s_cdt);
	bot_names.push_back(bname);
    }

    // Do tessellations
    for (int i = 0; i < obj_cnt; i++) {
	ON_Brep_CDT_Tessellate(ss_cdt[i], 0, NULL);
    }

    // Do comparison/resolution
    struct ON_Brep_CDT_State **s_a = (struct ON_Brep_CDT_State **)bu_calloc(ss_cdt.size(), sizeof(struct ON_Brep_CDT_State *), "state array");
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	s_a[i] = ss_cdt[i];
    }
    if (ON_Brep_CDT_Ovlp_Resolve(s_a, obj_cnt) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: RESOLVE fail.");
	return GED_ERROR;
    }

    // Make final meshes
    for (int i = 0; i < obj_cnt; i++) {
	int fcnt, fncnt, ncnt, vcnt;
	int *faces = NULL;
	fastf_t *vertices = NULL;
	int *face_normals = NULL;
	fastf_t *normals = NULL;

	ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, ss_cdt[i], 0, NULL);
	ON_Brep_CDT_Destroy(ss_cdt[i]);

	struct bu_vls bot_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bot_name, "%s", bot_names[i].c_str());

	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = vcnt;
	bot->num_faces = fcnt;
	bot->vertices = vertices;
	bot->faces = faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = ncnt;
	bot->num_face_normals = fncnt;
	bot->normals = normals;
	bot->face_normals = face_normals;

	if (wdb_export(gedp->ged_wdbp, bu_vls_cstr(&bot_name), (void *)bot, ID_BOT, 1.0)) {
	    return GED_ERROR;
	}
	bu_vls_free(&bot_name);
    }

    return GED_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
