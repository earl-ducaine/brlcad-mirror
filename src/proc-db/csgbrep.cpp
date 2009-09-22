/*                           C S G B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file csgbrep.c
 * 
 *  
 * 
 */

#include "common.h"
#include "bu.h"
#include "opennurbs.h"
#include "rtgeom.h"
#include "wdb.h"

extern struct rt_sketch_internal *sketch_start();

#define OBJ_BREP
/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include "bio.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"
    extern void rt_arb_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol,const struct bn_tol *tol);
    extern void rt_arb8_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_arbn_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_nmg_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_sph_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_ell_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_eto_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_rhc_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_rpc_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_epa_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_ehy_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_tgc_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_tor_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_sketch_brep(ON_Brep **bi, struct rt_db_internal *ip, const struct bn_tol *tol);
    extern void rt_sketch_ifree(struct rt_db_internal *ip, struct resource *resp);

#ifdef __cplusplus
}
#endif

int
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    struct rt_db_internal *tmp_internal = (struct rt_db_internal *) bu_malloc(sizeof(struct rt_db_internal), "allocate structure");
    RT_INIT_DB_INTERNAL(tmp_internal);
    struct bn_tol tmptol;
    tmptol.magic = BN_TOL_MAGIC;
    tmptol.dist = 0.005;
    const struct bn_tol *tol = &tmptol;
    struct rt_tess_tol ttmptol;
    ttmptol.abs = 0;
    ttmptol.rel = 0.01;
    ttmptol.norm = 0;
    const struct rt_tess_tol *ttol = &ttmptol;
    point_t center, v1, v2;
    vect_t a, b, c, d, h, N;
    ON_TextLog error_log;

    ON::Begin();

    outfp = wdb_fopen("csgbrep.g");
    const char* id_name = "CSG B-Rep Examples";
    mk_id(outfp, id_name);
/*
    bu_log("Writing an ARB4 (via NMG) brep...\n");
    ON_Brep* arb4brep = ON_Brep::New();
    struct rt_arb_internal *arb4;
    BU_GETSTRUCT(arb4, rt_arb_internal);
    arb4->magic = RT_ARB_INTERNAL_MAGIC;
    point_t ptarb4[8];
    VSET(ptarb4[0], 1000, -1000, -1000);
    VSET(ptarb4[1], 1000, 1000, -1000);
    VSET(ptarb4[2], 1000, 1000, 1000);
    VSET(ptarb4[3], 1000, 1000, 1000);
    VSET(ptarb4[4], -1000, 1000, -1000);
    VSET(ptarb4[5], -1000, 1000, -1000);
    VSET(ptarb4[6], -1000, 1000, -1000);
    VSET(ptarb4[7], -1000, 1000, -1000);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arb4->pt[i], ptarb4[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arb4;
    rt_arb8_brep(&arb4brep, tmp_internal, tol);
    const char* arb4_name = "arb4_nurb.s";
    mk_brep(outfp, arb4_name, arb4brep);
    delete arb4brep;
 
    bu_log("Writing an ARB5 (via NMG) brep...\n");
    ON_Brep* arb5brep = ON_Brep::New();
    struct rt_arb_internal *arb5;
    BU_GETSTRUCT(arb5, rt_arb_internal);
    arb5->magic = RT_ARB_INTERNAL_MAGIC;
    point_t ptarb5[8];
    VSET(ptarb5[0], 1000, -1000, -1000);
    VSET(ptarb5[1], 1000, 1000, -1000);
    VSET(ptarb5[2], 1000, 1000, 1000);
    VSET(ptarb5[3], 1000, -1000, 1000);
    VSET(ptarb5[4], -1000, 0, 0);
    VSET(ptarb5[5], -1000, 0, 0);
    VSET(ptarb5[6], -1000, 0, 0);
    VSET(ptarb5[7], -1000, 0, 0);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arb5->pt[i], ptarb5[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arb5;
    rt_arb8_brep(&arb5brep, tmp_internal, tol);
    const char* arb5_name = "arb5_nurb.s";
    mk_brep(outfp, arb5_name, arb5brep);
    delete arb5brep;
 
    bu_log("Writing an ARB6 (via NMG) brep...\n");
    ON_Brep* arb6brep = ON_Brep::New();
    struct rt_arb_internal *arb6;
    BU_GETSTRUCT(arb6, rt_arb_internal);
    arb6->magic = RT_ARB_INTERNAL_MAGIC;
    point_t ptarb6[8];
    VSET(ptarb6[0], 1000, -1000, -1000);
    VSET(ptarb6[1], 1000, 1000, -1000);
    VSET(ptarb6[2], 1000, 1000, 1000);
    VSET(ptarb6[3], 1000, -1000, 1000);
    VSET(ptarb6[4], -1000, 0, -1000);
    VSET(ptarb6[5], -1000, 0, -1000);
    VSET(ptarb6[6], -1000, 0, 1000);
    VSET(ptarb6[7], -1000, 0, 1000);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arb6->pt[i], ptarb6[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arb6;
    rt_arb8_brep(&arb6brep, tmp_internal, tol);
    const char* arb6_name = "arb6_nurb.s";
    mk_brep(outfp, arb6_name, arb6brep);
    delete arb6brep;
 
    bu_log("Writing an ARB7 (via NMG) brep...\n");
    ON_Brep* arb7brep = ON_Brep::New();
    struct rt_arb_internal *arb7;
    BU_GETSTRUCT(arb7, rt_arb_internal);
    arb7->magic = RT_ARB_INTERNAL_MAGIC;
    point_t ptarb7[8];
    VSET(ptarb7[0], 1000, -1000, -500);
    VSET(ptarb7[1], 1000, 1000, -500);
    VSET(ptarb7[2], 1000, 1000, 1500);
    VSET(ptarb7[3], 1000, -1000, 500);
    VSET(ptarb7[4], -1000, -1000, -500);
    VSET(ptarb7[5], -1000, 1000, -500);
    VSET(ptarb7[6], -1000, 1000, 500);
    VSET(ptarb7[7], -1000, -1000, -500);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arb7->pt[i], ptarb7[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arb7;
    rt_arb8_brep(&arb7brep, tmp_internal, tol);
    const char* arb7_name = "arb7_nurb.s";
    mk_brep(outfp, arb7_name, arb7brep);
    delete arb7brep;
 
    bu_log("Writing an ARB8 (via NMG) b-rep...\n");
    ON_Brep* arb8brep = ON_Brep::New();
    struct rt_arb_internal *arb8;
    BU_GETSTRUCT(arb8, rt_arb_internal);
    arb8->magic = RT_ARB_INTERNAL_MAGIC;
    point_t pt8[8];
    VSET(pt8[0], 1015, -1000, -995);
    VSET(pt8[1], 1015, 1000, -995);
    VSET(pt8[2], 1015, 1000, 1005);
    VSET(pt8[3], 1015, -1000, 1005);
    VSET(pt8[4], -985, -1000, -995);
    VSET(pt8[5], -985, 1000, -995);
    VSET(pt8[6], -985, 1000, 1005);
    VSET(pt8[7], -985, -1000, 1005);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arb8->pt[i], pt8[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arb8;
    rt_arb8_brep(&arb8brep, tmp_internal, tol);
    const char* arb8_name = "arb8_nurb.s";
    mk_brep(outfp, arb8_name, arb8brep);
    delete arb8brep;
    
    bu_log("Writing an ARBN (via NMG) b-rep...\n");
    ON_Brep* arbnbrep = ON_Brep::New();
    struct rt_db_internal arbninternal;
    RT_INIT_DB_INTERNAL(&arbninternal);
    arbninternal.idb_type = ID_ARBN;
    arbninternal.idb_meth = &rt_functab[ID_ARBN];
    arbninternal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arbn_internal), "rt_arbn_internal");
    struct rt_arbn_internal *arbn;
    arbn = (struct rt_arbn_internal *)arbninternal.idb_ptr;
    arbn->magic = RT_ARBN_INTERNAL_MAGIC;
    arbn->neqn = 8;
    arbn->eqn = (plane_t *)bu_calloc(arbn->neqn,sizeof(plane_t), "arbn plane eqns");
    VSET(arbn->eqn[0], 1, 0, 0);
    arbn->eqn[0][3] = 1000;
    VSET(arbn->eqn[1], -1, 0, 0);
    arbn->eqn[1][3] = 1000;
    VSET(arbn->eqn[2], 0, 1, 0);
    arbn->eqn[2][3] = 1000;
    VSET(arbn->eqn[3], 0, -1, 0);
    arbn->eqn[3][3] = 1000;
    VSET(arbn->eqn[4], 0, 0, 1);
    arbn->eqn[4][3] = 1000;
    VSET(arbn->eqn[5], 0, 0, -1);
    arbn->eqn[5][3] = 1000;
    VSET(arbn->eqn[6], 0.57735, 0.57735, 0.57735);
    arbn->eqn[6][3] = 1000;
    VSET(arbn->eqn[7], -0.57735, -0.57735, -0.57735);
    arbn->eqn[7][3] = 1000;
    rt_arbn_brep(&arbnbrep, &arbninternal, tol);
    const char* arbn_name = "arbn_nurb.s";
    mk_brep(outfp, arbn_name, arbnbrep);
    bu_free(arbn->eqn, "free arbn eqn");
    bu_free(arbninternal.idb_ptr, "free arbn");
    delete arbnbrep;
  

    // This routine does explicitly what is done
    // by the previous ARB8 brep call internally.
    // Ideally a more general NMG will be created
    // and fed to rt_nmg_brep rather than using an
    // ARB as the starting point.
    bu_log("Writing an NMG brep...\n");
    ON_Brep* nmgbrep = ON_Brep::New();
    struct rt_arb_internal *arbnmg8;
    BU_GETSTRUCT(arbnmg8, rt_arb_internal);
    arbnmg8->magic = RT_ARB_INTERNAL_MAGIC;
    point_t ptnmg8[8];
    VSET(ptnmg8[0], 0,0,0);
    VSET(ptnmg8[1], 0,2000,0);
    VSET(ptnmg8[2], 0,2000,2000);
    VSET(ptnmg8[3], 0,0,2000);
    VSET(ptnmg8[4], -2000,0, 0);
    VSET(ptnmg8[5], -2000,2000,0);
    VSET(ptnmg8[6], -2000,2000,2000);
    VSET(ptnmg8[7], -2000,0,2000);
    for ( int i=0; i < 8; i++ )  {
	VMOVE( arbnmg8->pt[i], ptnmg8[i] );
    }
    tmp_internal->idb_ptr = (genptr_t)arbnmg8;
    // Now, need nmg form of the arb
    struct model *m = nmg_mm();
    struct nmgregion *r;
    rt_arb_tess(&r, m, tmp_internal, ttol, tol);
    tmp_internal->idb_ptr = (genptr_t)m;
    rt_nmg_brep(&nmgbrep, tmp_internal, tol);
    const char* nmg_name = "nmg_nurb.s";
    mk_brep(outfp, nmg_name, nmgbrep);
    FREE_MODEL(m);
    delete nmgbrep;
   
    
    bu_log("Writing a Spherical b-rep...\n");
    ON_Brep* sphbrep = ON_Brep::New();
    struct rt_ell_internal *sph;
    BU_GETSTRUCT(sph, rt_ell_internal);
    sph->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(center, 0, 0, 0);
    VSET(a, 5, 0, 0);
    VSET(b, 0, 5, 0);
    VSET(c, 0, 0, 5); 
    VMOVE(sph->v, center);
    VMOVE(sph->a, a);
    VMOVE(sph->b, b);
    VMOVE(sph->c, c);
    tmp_internal->idb_ptr = (genptr_t)sph;
    rt_sph_brep(&sphbrep, tmp_internal, tol);
    const char* sph_name = "sph_nurb.s";
    mk_brep(outfp, sph_name, sphbrep);
    delete sphbrep;

    bu_log("Writing an Ellipsoidal b-rep...\n");
    ON_Brep* ellbrep = ON_Brep::New();
    struct rt_ell_internal *ell;
    BU_GETSTRUCT(ell, rt_ell_internal);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(center, 0, 0, 0);
    VSET(a, 5, 0, 0);
    VSET(b, 0, 3, 0);
    VSET(c, 0, 0, 1); 
    VMOVE(ell->v, center);
    VMOVE(ell->a, a);
    VMOVE(ell->b, b);
    VMOVE(ell->c, c);
    tmp_internal->idb_ptr = (genptr_t)ell;
    rt_ell_brep(&ellbrep, tmp_internal, tol);
    const char* ell_name = "ell_nurb.s";
    mk_brep(outfp, ell_name, ellbrep);
    delete ellbrep;
    
    bu_log("Writing a RHC b-rep...\n");
    ON_Brep* rhcbrep = ON_Brep::New();
    struct rt_rhc_internal *rhc;
    BU_GETSTRUCT(rhc, rt_rhc_internal);
    rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc->rhc_V, 0, 0, 0);
    VSET(rhc->rhc_H, 0, 2000, 0);
    VSET(rhc->rhc_B, 0, 0, 2000);
    rhc->rhc_r = 100;
    rhc->rhc_c = 400;
    tmp_internal->idb_ptr = (genptr_t)rhc;
    rt_rhc_brep(&rhcbrep, tmp_internal, tol);
    const char* rhc_name = "rhc_nurb.s";
    mk_brep(outfp, rhc_name, rhcbrep);
    delete rhcbrep;

    bu_log("Writing a RPC b-rep...\n");
    ON_Brep* rpcbrep = ON_Brep::New();
    struct rt_rpc_internal *rpc;
    BU_GETSTRUCT(rpc, rt_rpc_internal);
    rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc->rpc_V, 24, -1218, -1000);
    VSET(rpc->rpc_H, 60, 2000, -100);
    VCROSS(rpc->rpc_B, rpc->rpc_V, rpc->rpc_H);
    VUNITIZE(rpc->rpc_B);
    VSCALE(rpc->rpc_B, rpc->rpc_B, 2000);
    rpc->rpc_r = 1000;
    tmp_internal->idb_ptr = (genptr_t)rpc;
    rt_rpc_brep(&rpcbrep, tmp_internal, tol);
    const char* rpc_name = "rpc_nurb.s";
    mk_brep(outfp, rpc_name, rpcbrep);
    delete rpcbrep;

    bu_log("Writing an EPA b-rep...\n");
    ON_Brep* epabrep = ON_Brep::New();
    struct rt_epa_internal *epa;
    BU_GETSTRUCT(epa, rt_epa_internal);
    epa->epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa->epa_V, 0, 0, 0);
    VSET(epa->epa_H, 0, 0, 2000);
    VSET(epa->epa_Au, 1, 0, 0);
    epa->epa_r1 = 1000;
    epa->epa_r2 = 500;
    tmp_internal->idb_ptr = (genptr_t)epa;
    rt_epa_brep(&epabrep, tmp_internal, tol);
    const char* epa_name = "epa_nurb.s";
    mk_brep(outfp, epa_name, epabrep);
    delete epabrep;
 
    bu_log("Writing an EHY b-rep...\n");
    ON_Brep* ehybrep = ON_Brep::New();
    struct rt_ehy_internal *ehy;
    BU_GETSTRUCT(ehy, rt_ehy_internal);
    ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy->ehy_V, 0, 0, 0);
    VSET(ehy->ehy_H, 0, 0, 2000);
    VSET(ehy->ehy_Au, 1, 0, 0);
    ehy->ehy_r1 = 1000;
    ehy->ehy_r2 = 500;
    ehy->ehy_c = 400;
    tmp_internal->idb_ptr = (genptr_t)ehy;
    rt_ehy_brep(&ehybrep, tmp_internal, tol);
    const char* ehy_name = "ehy_nurb.s";
    mk_brep(outfp, ehy_name, ehybrep);
    delete ehybrep;
  
    bu_log("Writing a TGC b-rep...\n");
    ON_Brep* tgcbrep = ON_Brep::New();
    struct rt_tgc_internal *tgc;
    BU_GETSTRUCT(tgc, rt_tgc_internal);
    tgc->magic = RT_TGC_INTERNAL_MAGIC;
    VSET(tgc->v, 0, 0, -1000);
    VSET(tgc->h, 0, 0, 2000);
    VSET(tgc->a, 500, 0, 0);
    VSET(tgc->b, 0, 250, 0);
    VSET(tgc->c, 250, 0, 0);
    VSET(tgc->d, 0, 500, 0);
    tmp_internal->idb_ptr = (genptr_t)tgc;
    rt_tgc_brep(&tgcbrep, tmp_internal, tol);
    const char* tgc_name = "tgc_nurb.s";
    mk_brep(outfp, tgc_name, tgcbrep);
    delete tgcbrep;
   
    bu_log("Writing a Torus b-rep...\n");
    ON_Brep* torbrep = ON_Brep::New();
    struct rt_tor_internal *tor;
    BU_GETSTRUCT(tor, rt_tor_internal);
    tor->magic = RT_TOR_INTERNAL_MAGIC;
    VSET(center, 0, 0, 0);
    VSET(a, 0, 0, 1);
    VMOVE(tor->v, center);
    VMOVE(tor->h, a);
    tor->r_a = 5.0;
    tor->r_h = 2.0;
    tmp_internal->idb_ptr = (genptr_t)tor;
    rt_tor_brep(&torbrep, tmp_internal, tol);
    const char* tor_name = "tor_nurb.s";
    mk_brep(outfp, tor_name, torbrep);
    delete torbrep;

    bu_log("Writing an Elliptical Torus b-rep...\n");
    ON_Brep* etobrep = ON_Brep::New();
    struct rt_eto_internal *eto;
    BU_GETSTRUCT(eto, rt_eto_internal);
    eto->eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(center, 0, 0, 0);
    VSET(N, 0, 0, 1);
    VSET(a, 200, 0, 200);
    VMOVE(eto->eto_V, center);
    VMOVE(eto->eto_N, N);
    VMOVE(eto->eto_C, a);
    eto->eto_r = 800;
    eto->eto_rd = 100;
    tmp_internal->idb_ptr = (genptr_t)eto;
    rt_eto_brep(&etobrep, tmp_internal, tol);
    const char* eto_name = "eto_nurb.s";
    mk_brep(outfp, eto_name, etobrep);
    delete etobrep;
*/
    bu_log("Writing a Sketch (non-solid) b-rep...\n");
    int cnti;
    ON_Brep* sketchbrep = ON_Brep::New();
    struct rt_sketch_internal *skt;
    struct bezier_seg *bsg;
    struct line_seg *lsg;
    struct carc_seg *csg;
    point_t V;
    vect_t u_vec, v_vec;
    point2d_t verts[] = {
        { 250, 0 },     /* 0 */
	{ 500, 0 },     /* 1 */
	{ 500, 500 },   /* 2 */
	{ 0, 500 },     /* 3 */
	{ 0, 250 },     /* 4 */
	{ 250, 250 },   /* 5 */
	{ 125, 125 },   /* 6 */
	{ 0, 125 },     /* 7 */
	{ 125, 0 },     /* 8 */
	{ 200, 200 }    /* 9 */
    };
    VSET(V, 10, 20, 30);
    VSET(u_vec, 1, 0, 0);
    VSET(v_vec, 0, 1, 0);
    skt = (struct rt_sketch_internal *)bu_calloc(1, sizeof(struct rt_sketch_internal), "sketch");
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VMOVE(skt->V, V);
    VMOVE(skt->u_vec, u_vec);
    VMOVE(skt->v_vec, v_vec);
    skt->vert_count = 10;
    skt->verts = (point2d_t *)bu_calloc(skt->vert_count, sizeof(point2d_t), "verts");
    for (cnti=0; cnti < skt->vert_count; cnti++) {
	V2MOVE(skt->verts[cnti], verts[cnti]);
    }
    skt->skt_curve.seg_count = 6;
    skt->skt_curve.reverse = (int *)bu_calloc(skt->skt_curve.seg_count, sizeof(int), "sketch: reverse");
    skt->skt_curve.segments = (genptr_t *)bu_calloc(skt->skt_curve.seg_count, sizeof(genptr_t), "segs");
    bsg = (struct bezier_seg *)bu_malloc(sizeof(struct bezier_seg), "sketch: bsg");
    bsg->magic = CURVE_BEZIER_MAGIC;
    bsg->degree = 4;
    bsg->ctl_points = (int *)bu_calloc(bsg->degree+1, sizeof(int), "sketch: bsg->ctl_points");
    bsg->ctl_points[0] = 4;
    bsg->ctl_points[1] = 7;
    bsg->ctl_points[2] = 9;
    bsg->ctl_points[3] = 8;
    bsg->ctl_points[4] = 0;
    skt->skt_curve.segments[0] = (genptr_t)bsg;
    
    lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 0;
    lsg->end = 1;
    
    skt->skt_curve.segments[1] = (genptr_t)lsg;
    
    lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 1;
    lsg->end = 2;
    
    skt->skt_curve.segments[2] = (genptr_t)lsg;
    
    lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 2;
    lsg->end = 3;
    
    skt->skt_curve.segments[3] = (genptr_t)lsg;
    
    lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 3;
    lsg->end = 4;
    
    skt->skt_curve.segments[4] = (genptr_t)lsg;
    
    csg = (struct carc_seg *)bu_malloc(sizeof(struct carc_seg), "sketch: csg");
    
    csg->magic = CURVE_CARC_MAGIC;
    csg->radius = -1.0;
    csg->start = 6;
    csg->end = 5;
    
    skt->skt_curve.segments[5] = (genptr_t)csg;
    
    const char* sketch_name_csg = "sketch.s";
    mk_sketch(outfp, sketch_name_csg, skt);
        
    tmp_internal->idb_ptr = (genptr_t)skt;
    rt_sketch_brep(&sketchbrep, tmp_internal, tol);
    const char* sketch_name = "sketch_nurb.s";
    mk_brep(outfp, sketch_name, sketchbrep);
    delete sketchbrep;

    bu_free(tmp_internal, "free tmp_internal");
    wdb_close(outfp);

    ON::End();

    return 0;
}

#else /* !OBJ_BREP */

int
main(int argc, char *argv[])
{
    bu_log("ERROR: Boundary Representation object support is not available with\n"
	   "       this compilation of BRL-CAD.\n");
    return 1;
}

#endif /* OBJ_BREP */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
