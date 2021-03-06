/*                   C D T _ O V L P S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * This file is logic specifically for refining meshes to clear
 * overlaps between sets of objects.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include <random>
#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

#define TREE_LEAF_FACE_3D(pf, valp, a, b, c, d)  \
    pdv_3move(pf, pt[a]); \
    pdv_3cont(pf, pt[b]); \
    pdv_3cont(pf, pt[c]); \
    pdv_3cont(pf, pt[d]); \
    pdv_3cont(pf, pt[a]); \

#define BBOX_PLOT(pf, bb) {                 \
    fastf_t pt[8][3];                       \
    point_t min, max;		    	    \
    min[0] = bb.Min().x;                    \
    min[1] = bb.Min().y;                    \
    min[2] = bb.Min().z;		    \
    max[0] = bb.Max().x;		    \
    max[1] = bb.Max().y;		    \
    max[2] = bb.Max().z;		    \
    VSET(pt[0], max[X], min[Y], min[Z]);    \
    VSET(pt[1], max[X], max[Y], min[Z]);    \
    VSET(pt[2], max[X], max[Y], max[Z]);    \
    VSET(pt[3], max[X], min[Y], max[Z]);    \
    VSET(pt[4], min[X], min[Y], min[Z]);    \
    VSET(pt[5], min[X], max[Y], min[Z]);    \
    VSET(pt[6], min[X], max[Y], max[Z]);    \
    VSET(pt[7], min[X], min[Y], max[Z]);    \
    TREE_LEAF_FACE_3D(pf, pt, 0, 1, 2, 3);      \
    TREE_LEAF_FACE_3D(pf, pt, 4, 0, 3, 7);      \
    TREE_LEAF_FACE_3D(pf, pt, 5, 4, 7, 6);      \
    TREE_LEAF_FACE_3D(pf, pt, 1, 5, 6, 2);      \
}


void
ovlp_plot_bbox(ON_BoundingBox &bb)
{
    FILE *plot = fopen ("bb.plot3", "w");
    BBOX_PLOT(plot, bb);
    fclose(plot);
}

double
tri_pnt_r(cdt_mesh::cdt_mesh_t &fmesh, long tri_ind)
{
    cdt_mesh::triangle_t tri = fmesh.tris_vect[tri_ind];
    ON_3dPoint *p3d = fmesh.pnts[tri.v[0]];
    ON_BoundingBox bb(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = fmesh.pnts[tri.v[i]];
	bb.Set(*p3d, true);
    }
    double bbd = bb.Diagonal().Length();
    return bbd * 0.01;
}

class omesh_t;
class overt_t {
    public:

	overt_t(omesh_t *om, long p)
	{
	    omesh = om;
	    p_id = p;
	    closest_uedge = -1;
	    t_ind = -1;
	}

	omesh_t *omesh;
	long p_id;

	bool edge_vert();

	double min_len();

	ON_BoundingBox bb;

	long closest_uedge;
	bool t_ind;

	ON_3dPoint vpnt();

	void plot(FILE *plot);

	double v_min_edge_len;
};

class omesh_t
{
    public:
	omesh_t(cdt_mesh::cdt_mesh_t *m)
	{
	    fmesh = m;
	    init_verts();
	};

	// The fmesh pnts array may have inactive vertices - we only want the
	// active verts for this portion of the processing.
	std::map<long, overt_t *> overts;
	RTree<long, double, 3> vtree;
	void add_vtree_vert(overt_t *v);
	void remove_vtree_vert(overt_t *v);
	void plot_vtree(const char *fname);
	bool validate_vtree();
	void save_vtree(const char *fname);
	void load_vtree(const char *fname);

	// Add an fmesh vertex to the overts array and tree.
	overt_t *vert_add(long, ON_BoundingBox *bb = NULL);

	// Find the closest uedge in the mesh to a point
	cdt_mesh::uedge_t closest_uedge(ON_3dPoint &p);
	// Find close (non-face-boundary) edges
	std::set<cdt_mesh::uedge_t> interior_uedges_search(ON_BoundingBox &bb);

	// Find close triangles
	std::set<size_t> tris_search(ON_BoundingBox &bb);

	// Find close vertices
	std::set<overt_t *> vert_search(ON_BoundingBox &bb);
	std::set<overt_t *> vert_search(overt_t *v);
	overt_t * vert_closest(double *vdist, overt_t *v);

	void retessellate(std::set<size_t> &ov);

	// Points from other meshes potentially needing refinement in this mesh
	std::map<overt_t *, std::set<long>> refinement_overts;

	// Points from this mesh inducing refinement in other meshes, and
	// triangles reported by tri_isect as intersecting from this mesh
	std::map<overt_t *, std::set<long>> intruding_overts;
	std::set<size_t> intruding_tris;

	void refinement_clear();
	std::set<long> refinement_split_tris();

	cdt_mesh::cdt_mesh_t *fmesh;

	void plot(const char *fname,
		std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
		std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);
	void plot(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
		std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap);

	void verts_one_ring_update(long p_id);

	void vupdate(overt_t *v);
    private:
	void init_verts();
	void rebuild_vtree();

	void edge_tris_remove(cdt_mesh::uedge_t &ue);

};

#define PPOINT 3.0550159446561063,7.5001153978277033,23.999999999999996
bool
PPCHECK(ON_3dPoint &p)
{
    ON_3dPoint problem(PPOINT);
    if (problem.DistanceTo(p) < 0.01) {
	std::cout << "Saw problem point\n";
	return true;
    }
    return false;
}

bool
VPCHECK(overt_t *v, const char *fname)
{
    if (fname && !BU_STR_EQUAL(v->omesh->fmesh->name, fname)) {
	return false;
    }
    ON_3dPoint p = v->vpnt();
    if (PPCHECK(p)) {
	std::cout << "Saw problem vert: " << v->omesh->fmesh->name << "-" << v->omesh->fmesh->f_id << "\n";
	return true;
    }
    return false;
}

double
overt_t::min_len() {
    return v_min_edge_len;
}

ON_3dPoint
overt_t::vpnt() {
    ON_3dPoint vp = *(omesh->fmesh->pnts[p_id]);
    return vp;
}

bool
overt_t::edge_vert() {
    return (omesh->fmesh->ep.find(p_id) != omesh->fmesh->ep.end());
}

void
omesh_t::vupdate(overt_t *v) {
    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = fmesh->v2edges[v->p_id];

    // 2.  find the shortest edge associated with pnt
    std::set<cdt_mesh::edge_t>::iterator e_it;
    double elen = DBL_MAX;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	ON_3dPoint *p1 = fmesh->pnts[(*e_it).v[0]];
	ON_3dPoint *p2 = fmesh->pnts[(*e_it).v[1]];
	double dist = p1->DistanceTo(*p2);
	elen = (dist < elen) ? dist : elen;
    }
    v->v_min_edge_len = elen;

    // create a bbox around pnt using length ~20% of the shortest edge length.
    ON_3dPoint vpnt = *fmesh->pnts[v->p_id];

    ON_BoundingBox init_bb(vpnt, vpnt);
    v->bb = init_bb;
    ON_3dPoint npnt = vpnt;
    double lfactor = 0.2;
    npnt.x = npnt.x + lfactor*elen;
    v->bb.Set(npnt, true);
    npnt = vpnt;
    npnt.x = npnt.x - lfactor*elen;
    v->bb.Set(npnt, true);
    npnt = vpnt;
    npnt.y = npnt.y + lfactor*elen;
    v->bb.Set(npnt, true);
    npnt = vpnt;
    npnt.y = npnt.y - lfactor*elen;
    v->bb.Set(npnt, true);
    npnt = vpnt;
    npnt.z = npnt.z + lfactor*elen;
    v->bb.Set(npnt, true);
    npnt = vpnt;
    npnt.z = npnt.z - lfactor*elen;
    v->bb.Set(npnt, true);

    add_vtree_vert(v);

}

void
overt_t::plot(FILE *plot)
{
    ON_3dPoint *i_p = omesh->fmesh->pnts[p_id];
    BBOX_PLOT(plot, bb);
    double r = 0.05*bb.Diagonal().Length();
    pl_color(plot, 0, 255, 0);
    plot_pnt_3d(plot, i_p, r, 0);
}

void
omesh_t::add_vtree_vert(overt_t *v)
{
    remove_vtree_vert(v);
    double fMin[3];
    fMin[0] = v->bb.Min().x;
    fMin[1] = v->bb.Min().y;
    fMin[2] = v->bb.Min().z;
    double fMax[3];
    fMax[0] = v->bb.Max().x;
    fMax[1] = v->bb.Max().y;
    fMax[2] = v->bb.Max().z;
    if (fMin[0] < -0.4*DBL_MAX || fMax[0] > 0.4*DBL_MAX) {
	std::cout << "Bad vert box!\n";
    }
    vtree.Insert(fMin, fMax, v->p_id);
    //rebuild_vtree();
}

void
omesh_t::remove_vtree_vert(overt_t *v)
{
    double fMin[3];
    fMin[0] = v->bb.Min().x-ON_ZERO_TOLERANCE;
    fMin[1] = v->bb.Min().y-ON_ZERO_TOLERANCE;
    fMin[2] = v->bb.Min().z-ON_ZERO_TOLERANCE;
    double fMax[3];
    fMax[0] = v->bb.Max().x+ON_ZERO_TOLERANCE;
    fMax[1] = v->bb.Max().y+ON_ZERO_TOLERANCE;
    fMax[2] = v->bb.Max().z+ON_ZERO_TOLERANCE;
    vtree.Remove(fMin, fMax, v->p_id);
}

void
omesh_t::init_verts()
{
    // Walk the fmesh's rtree holding the active triangles to get all
    // vertices active in the face
    std::set<long> averts;
    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    cdt_mesh::triangle_t tri;
    fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = fmesh->tris_vect[t_ind];
	averts.insert(tri.v[0]);
	averts.insert(tri.v[1]);
	averts.insert(tri.v[2]);
	++tree_it;
    }

    std::set<long>::iterator a_it;
    for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	overts[*a_it] = new overt_t(this, *a_it);
	vupdate(overts[*a_it]);
    }
}

void
omesh_t::rebuild_vtree()
{
    std::map<long, class overt_t *>::iterator o_it;

    vtree.RemoveAll();

    for (o_it = overts.begin(); o_it != overts.end(); o_it++) {
	long ind = o_it->first;
	overt_t *ov = o_it->second;
	double fMin[3];
	fMin[0] = ov->bb.Min().x;
	fMin[1] = ov->bb.Min().y;
	fMin[2] = ov->bb.Min().z;
	double fMax[3];
	fMax[0] = ov->bb.Max().x;
	fMax[1] = ov->bb.Max().y;
	fMax[2] = ov->bb.Max().z;
	if (fMin[0] < -0.4*DBL_MAX || fMax[0] > 0.4*DBL_MAX) {
	    std::cout << "Bad vert box!\n";
	}
	vtree.Insert(fMin, fMax, ind);
    }
}

void
omesh_t::plot_vtree(const char *fname)
{
    FILE *plot = fopen(fname, "w");
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	pl_color(plot, 255, 0, 0);
	overts[v_ind]->plot(plot);
	++tree_it;
    }
    fclose(plot);
}

bool
omesh_t::validate_vtree()
{
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	overt_t *ov = overts[v_ind];
	ON_3dPoint vp = ov->vpnt();
	std::set<overt_t *> search_vert_bb = vert_search(ov->bb);
	if (!search_vert_bb.size()) {
	    std::cout << "Error: no nearby verts for vert bb " << v_ind << "??\n";
	    return false;
	}
	if (search_vert_bb.find(ov) == search_vert_bb.end()) {
	    std::cout << "Error: vert in tree, but bb search couldn't find: " << v_ind << "\n";
	    std::set<overt_t *> s2 = vert_search(ov->bb);
	    if (s2.find(ov) == s2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	ON_BoundingBox pbb(vp, vp);
	std::set<overt_t *> search_vert = vert_search(pbb);
	if (!search_vert.size()) {
	    std::cout << "Error: vert point not contained by tree box?? " << v_ind << "??\n";
	    std::set<overt_t *> sv2 = vert_search(pbb);
	    if (sv2.find(ov) == sv2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	++tree_it;
    }
    return true;
}

void
omesh_t::load_vtree(const char *fname)
{
    vtree.RemoveAll();
    vtree.Load(fname);
}

void
omesh_t::save_vtree(const char *fname)
{
    vtree.Save(fname);
}

void
omesh_t::plot(const char *fname,
       	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    struct bu_color c = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {0,0,255};

    // Randomize the blue so (usually) subsequent
    // draws of different meshes won't erase the
    // previous wireframe
    std::random_device rd;
    std::mt19937 reng(rd());
    std::uniform_int_distribution<> rrange(100,250);
    int brand = rrange(reng);
    rgb[2] = (unsigned char)brand;

    FILE *plot = fopen(fname, "w");

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    cdt_mesh::triangle_t tri;

    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);

    fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = fmesh->tris_vect[t_ind];
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
	++tree_it;
    }

    double tri_r = 0;

    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot, &c);
    std::set<size_t>::iterator ts_it;
    for (ts_it = intruding_tris.begin(); ts_it != intruding_tris.end(); ts_it++) {
	tri = fmesh->tris_vect[*ts_it];
	double tr = tri_pnt_r(*fmesh, tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }

    std::map<overt_t *, std::set<long>>::iterator iv_it;


    if (ev) {
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
	pl_color(plot, 128, 0, 128);
	for (ev_it = ev->begin(); ev_it != ev->end(); ev_it++) {
	    cdt_mesh::bedge_seg_t *eseg = ev_it->first;
	    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
	    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
	    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
	    cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
	    cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
	    omesh_t *o1 = f2omap[&fmesh_f1];
	    omesh_t *o2 = f2omap[&fmesh_f2];
	    if (o1 == this || o2 == this) {
		std::set<overt_t *>::iterator v_it;
		for (v_it = ev_it->second.begin(); v_it != ev_it->second.end(); v_it++) {
		    overt_t *iv = *v_it;
		    if (iv->omesh == this) continue;
		    ON_3dPoint vp = iv->vpnt();
		    plot_pnt_3d(plot, &vp, tri_r, 0);
		}
	    }
	}
    }


    pl_color(plot, 255, 0, 0);
    for (iv_it = refinement_overts.begin(); iv_it != refinement_overts.end(); iv_it++) {
	if (iv_it->second.size() > 1) {
	    overt_t *iv = iv_it->first;
	    ON_3dPoint vp = iv->vpnt();
	    plot_pnt_3d(plot, &vp, tri_r, 0);
	}
    }

    fclose(plot);
}

void
omesh_t::plot(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *ev, 
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap)
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    bu_vls_sprintf(&fname, "%s-%d_ovlps.plot3", s_cdt->name, fmesh->f_id);
    plot(bu_vls_cstr(&fname), ev, f2omap);
    bu_vls_free(&fname);
}

overt_t *
omesh_t::vert_closest(double *vdist, overt_t *v)
{
    ON_BoundingBox fbbox = v->omesh->fmesh->bbox();
    ON_3dPoint opnt = v->bb.Center();

    // Find close verts, iterate through them and
    // find the closest
    ON_BoundingBox s_bb = v->bb;
    ON_3dPoint p = v->vpnt();
    std::set<overt_t *> nverts = vert_search(s_bb);
    bool last_try = false;
    while (!nverts.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	nverts = vert_search(s_bb);
    }

    double dist = DBL_MAX;
    overt_t *nv = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = nverts.begin(); v_it != nverts.end(); v_it++) {
	overt_t *ov = *v_it;
	ON_3dPoint np = ov->vpnt();
	if (np.DistanceTo(p) < dist) {
	    nv = ov;
	    dist = np.DistanceTo(p);
	}
    }
    if (vdist) {
	(*vdist) = dist;
    }
    return nv;
}

cdt_mesh::uedge_t
omesh_t::closest_uedge(ON_3dPoint &p)
{
    cdt_mesh::uedge_t uedge;

    if (!fmesh->pnts.size()) return uedge;

    ON_BoundingBox fbbox = fmesh->bbox();

    ON_BoundingBox bb(p, p);
    bb.m_min.x = bb.Min().x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.Min().y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.Min().z - ON_ZERO_TOLERANCE;
    bb.m_max.x = bb.Max().x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.Max().y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.Max().z + ON_ZERO_TOLERANCE;

    // Find close triangles, iterate through them and
    // find the closest interior edge
    ON_BoundingBox s_bb = bb;
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = p + vmin;
	s_bb.m_max = p + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    double uedist = DBL_MAX;
    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	cdt_mesh::triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = bb.Center();
	cdt_mesh::uedge_t ue = fmesh->closest_uedge(t, tp);
	double ue_cdist = fmesh->uedge_dist(ue, p);
	if (ue_cdist < uedist) {
	    uedge = ue;
	    uedist = ue_cdist;
	}
    }

    return uedge;
}


std::set<cdt_mesh::uedge_t>
omesh_t::interior_uedges_search(ON_BoundingBox &bb)
{
    std::set<cdt_mesh::uedge_t> uedges;
    uedges.clear();

    ON_3dPoint opnt = bb.Center();

    if (!fmesh->pnts.size()) return uedges;

    ON_BoundingBox fbbox = fmesh->bbox();

    // Find close triangles, iterate through them and
    // find the closest interior edge
    ON_BoundingBox s_bb = bb;
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	cdt_mesh::triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = bb.Center();
	cdt_mesh::uedge_t ue = fmesh->closest_interior_uedge(t, tp);
	uedges.insert(ue);
    }

    return uedges;
}

static bool NearTrisCallback(size_t data, void *a_context) {
    std::set<size_t> *ntris = (std::set<size_t> *)a_context;
    ntris->insert(data);
    return true;
}
std::set<size_t>
omesh_t::tris_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> near_tris;
    size_t nhits = fmesh->tris_tree.Search(fMin, fMax, NearTrisCallback, (void *)&near_tris);

    if (!nhits) {
	return std::set<size_t>();
    }

    return near_tris;
}

static bool NearVertsCallback(long data, void *a_context) {
    std::set<long> *nverts = (std::set<long> *)a_context;
    nverts->insert(data);
    return true;
}
std::set<overt_t *>
omesh_t::vert_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x - ON_ZERO_TOLERANCE;
    fMin[1] = bb.Min().y - ON_ZERO_TOLERANCE;
    fMin[2] = bb.Min().z - ON_ZERO_TOLERANCE;
    fMax[0] = bb.Max().x + ON_ZERO_TOLERANCE;
    fMax[1] = bb.Max().y + ON_ZERO_TOLERANCE;
    fMax[2] = bb.Max().z + ON_ZERO_TOLERANCE;
    std::set<long> near_verts;
    size_t nhits = vtree.Search(fMin, fMax, NearVertsCallback, (void *)&near_verts);

    if (!nhits) {
	return std::set<overt_t *>();
    }

    std::set<overt_t *> near_overts;
    std::set<long>::iterator n_it;
    for (n_it = near_verts.begin(); n_it != near_verts.end(); n_it++) {
	near_overts.insert(overts[*n_it]);    
    }

    return near_overts;
}

std::set<overt_t *>
omesh_t::vert_search(overt_t *v)
{
    ON_BoundingBox vbb = v->bb;
    return vert_search(vbb);
}

void
omesh_t::verts_one_ring_update(long p_id)
{
    std::set<long> mod_verts;

    // 1.  Get pnt's associated edges.
    std::set<cdt_mesh::edge_t> edges = fmesh->v2edges[p_id];

    // 2.  Collect all the vertices associated with all the edges
    // connected to the original point - these are the mvert_info
    // structures that may have a new minimum edge length after
    // the change.
    std::set<cdt_mesh::edge_t>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	mod_verts.insert((*e_it).v[0]);
	mod_verts.insert((*e_it).v[1]);
    }

    // 3.  Update each vertex (both itself and in the vtree
    std::set<long>::iterator v_it;
    for (v_it = mod_verts.begin(); v_it != mod_verts.end(); v_it++) {
	vupdate(overts[*v_it]);
    }
}

overt_t *
omesh_t::vert_add(long f3ind, ON_BoundingBox *bb)
{
    overt_t *nv = new overt_t(this, f3ind);
    if (bb) {
	nv->bb = *bb;
    }
    vupdate(nv);
    overts[f3ind] = nv;
    return nv;
}

void
omesh_t::refinement_clear()
{
    refinement_overts.clear();
    intruding_tris.clear();
}

double
tri_shortest_edge_len(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d < len) ? d : len;
    }

    return len;
}

cdt_mesh::uedge_t
tri_shortest_edge(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::uedge_t ue;
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d < len) {
	    len = d;
	    ue = cdt_mesh::uedge_t(v0, v1);
	}
    }
    return ue;
}

double
tri_longest_edge_len(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d > len) ? d : len;
    }

    return len;
}

cdt_mesh::uedge_t
tri_longest_edge(cdt_mesh::cdt_mesh_t *fmesh, long t_ind)
{
    cdt_mesh::uedge_t ue;
    cdt_mesh::triangle_t t = fmesh->tris_vect[t_ind];
    double len = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	ON_3dPoint *p1 = fmesh->pnts[v0];
	ON_3dPoint *p2 = fmesh->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d > len) {
	    len = d;
	    ue = cdt_mesh::uedge_t(v0, v1);
	}
    }
    return ue;
}



ON_BoundingBox
edge_bbox(cdt_mesh::bedge_seg_t *eseg)
{
    ON_3dPoint *p3d1 = eseg->e_start;
    ON_3dPoint *p3d2 = eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
        bb.m_min.x = bb.m_min.x - 0.5*bdist;
        bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
        bb.m_min.y = bb.m_min.y - 0.5*bdist;
        bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
        bb.m_min.z = bb.m_min.z - 0.5*bdist;
        bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    return bb;
}

class tri_dist {
public:
    tri_dist(double idist, long iind) {
	dist = idist;
	ind = iind;
    }
    bool operator< (const tri_dist &b) const {
	return dist < b.dist;
    }
    double dist;
    long ind;
};



bool
closest_surf_pnt(ON_3dPoint &s_p, ON_3dVector &s_norm, cdt_mesh::cdt_mesh_t &fmesh, ON_3dPoint *p, double tol)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh.p_cdt;
    ON_2dPoint surf_p2d;
    ON_3dPoint surf_p3d = ON_3dPoint::UnsetPoint;
    s_p = ON_3dPoint::UnsetPoint;
    s_norm = ON_3dVector::UnsetVector;
    double cdist;
    if (tol <= 0) {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist);
    } else {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist, 0, ON_ZERO_TOLERANCE, tol);
    }
    if (NEAR_EQUAL(cdist, DBL_MAX, ON_ZERO_TOLERANCE)) return false;
    bool seval = surface_EvNormal(s_cdt->brep->m_F[fmesh.f_id].SurfaceOf(), surf_p2d.x, surf_p2d.y, s_p, s_norm);
    if (!seval) return false;

    if (fmesh.m_bRev) {
	s_norm = s_norm * -1;
    }

    return true;
}

ON_3dPoint
lseg_closest_pnt(ON_Line &l, ON_3dPoint &p)
{
    double t;
    l.ClosestPointTo(p, &t);
    if (t > 0 && t < 1) {
	return l.PointAt(t);
    } else {
	double d1 = l.from.DistanceTo(p);
	double d2 = l.to.DistanceTo(p);
	return (d2 < d1) ? l.to : l.from;
    }
}

void
isect_plot(cdt_mesh::cdt_mesh_t *fmesh1, long t1_ind, cdt_mesh::cdt_mesh_t *fmesh2, long t2_ind, point_t *isectpt1, point_t *isectpt2)
{
    FILE *plot = fopen("tri_pair.plot3", "w");
    double fpnt_r = -1.0;
    double pnt_r = -1.0;
    pl_color(plot, 0, 0, 255);
    fmesh1->plot_tri(fmesh1->tris_vect[t1_ind], NULL, plot, 0, 0, 0);
    pnt_r = tri_pnt_r(*fmesh1, t1_ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plot, 255, 0, 0);
    fmesh2->plot_tri(fmesh2->tris_vect[t2_ind], NULL, plot, 0, 0, 0);
    pnt_r = tri_pnt_r(*fmesh2, t2_ind);
    fpnt_r = (pnt_r > fpnt_r) ? pnt_r : fpnt_r;
    pl_color(plot, 255, 255, 255);
    ON_3dPoint p1((*isectpt1)[X], (*isectpt1)[Y], (*isectpt1)[Z]);
    ON_3dPoint p2((*isectpt2)[X], (*isectpt2)[Y], (*isectpt2)[Z]);
    plot_pnt_3d(plot, &p1, fpnt_r, 0);
    plot_pnt_3d(plot, &p2, fpnt_r, 0);
    pdv_3move(plot, *isectpt1);
    pdv_3cont(plot, *isectpt2);
    fclose(plot);
}

static void
isect_process_edge_vert2(
	overt_t *v, omesh_t *omesh2, ON_3dPoint &sp,
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    cdt_mesh::uedge_t closest_edge = omesh2->closest_uedge(sp);
    if (omesh2->fmesh->brep_edges.find(closest_edge) != omesh2->fmesh->brep_edges.end()) {
	cdt_mesh::bedge_seg_t *bseg = omesh2->fmesh->ue2b_map[closest_edge];
	if (!bseg) {
	    std::cout << "couldn't find bseg pointer??\n";
	} else {
	    (*edge_verts)[bseg].insert(v);
	}
    }
}

static void
isect_process_vert(
	omesh_t *omesh1, overt_t *v, cdt_mesh::triangle_t &t1,
       	omesh_t *omesh2, cdt_mesh::triangle_t &t2,
	std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts
	)
{

    if (!v) {
	std::cerr << "ERROR - no valid vertex point supplied to isect_process_vert\n";
	return;
    }

    // If we're tracking this, check to see if the intruding vertex in question
    // is closest to a brep face edge in the opposite mesh.  If it is, we may
    // need an edge split.
    //
    // TODO - we also need to try the points to see if they project *inside* the
    // opposite face's polygon - if they do, we also need to split the edge even
    // if the point is closer to an interior edge, since the brep face edge segment
    // might be far away from the actual edge curve in a course approximation.
    if (vert_edge_cnts) {
	ON_3dPoint p = v->vpnt();
	cdt_mesh::uedge_t closest_edge = omesh2->fmesh->closest_uedge(t2, p);
	if (omesh2->fmesh->brep_edges.find(closest_edge) != omesh2->fmesh->brep_edges.end()) {
	    cdt_mesh::bedge_seg_t *bseg = omesh2->fmesh->ue2b_map[closest_edge];
	    if (!bseg) {
		std::cout << "couldn't find bseg pointer??\n";
	    } else {
		(*vert_edge_cnts)[v][bseg]++;
	    }
	}
    }

    // Note this vertex as a possible source of refinement necessity in omesh2
    omesh2->refinement_overts[v].insert(t1.ind);

    // Let omesh1 know that its triangle is interfering with another mesh
    omesh1->intruding_tris.insert(t1.ind);
}

int
remote_vert_process(bool process, bool pinside, omesh_t *omesh1, overt_t *v, cdt_mesh::triangle_t &t1, omesh_t *omesh2, cdt_mesh::triangle_t &t2, std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts)
{
    if (!v) {
	std::cout << "WARNING: - no overt for vertex??\n";
	return 0;
    }
    ON_3dPoint vp = v->vpnt();
    bool near_vert = false;
    std::set<overt_t *> cverts = omesh2->vert_search(v);
    std::set<overt_t *>::iterator v_it;
    for (v_it = cverts.begin(); v_it != cverts.end(); v_it++) {
	ON_3dPoint cvpnt = (*v_it)->vpnt();
	double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
	if (vp.DistanceTo(cvpnt) < cvbbdiag) {
	    near_vert = true;
	    break;
	}
    }

    if (!near_vert && pinside) {
	if (process) {
	    isect_process_vert(omesh1, v, t1, omesh2, t2, vert_edge_cnts);
	}
	return 1;
    }

    return 0;
}

int
near_edge_process(bool process, double t, double vtol, overt_t *v, omesh_t *omesh1, cdt_mesh::triangle_t &t1,
	omesh_t *omesh2, cdt_mesh::triangle_t &t2, std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts)
{
    if (t > 0  && t < 1 && !NEAR_ZERO(t, vtol) && !NEAR_EQUAL(t, 1, vtol)) {
	//VPCHECK(v, NULL);

	if (process) {
	    isect_process_vert(omesh2, v, t2, omesh1, t1, vert_edge_cnts);
	    // In these cases it is possible that the mated triangle to the edge
	    // doesn't intrude on the opposite mesh.  Never the less we still want
	    // to process these points near edges, so artificially bump the tri
	    // listing for refinement_overts high enough to force processing
	    omesh1->refinement_overts[v].insert(-1);
	}
	return 1;
    }
    return 0;
}

static int
edge_only_isect(
	bool process,
	omesh_t *omesh1, cdt_mesh::triangle_t &t1,
	omesh_t *omesh2, cdt_mesh::triangle_t &t2,
	ON_3dPoint &p1, ON_3dPoint &p2,
	ON_Line *t1_lines,
	ON_Line *t2_lines,
	double etol,
	std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts
	)
{
    double p1d1[3], p1d2[3];
    double p2d1[3], p2d2[3];
    for (int i = 0; i < 3; i++) {
	p1d1[i] = p1.DistanceTo(t1_lines[i].ClosestPointTo(p1));
	p2d1[i] = p2.DistanceTo(t1_lines[i].ClosestPointTo(p2));
    	p1d2[i] = p1.DistanceTo(t2_lines[i].ClosestPointTo(p1));
	p2d2[i] = p2.DistanceTo(t2_lines[i].ClosestPointTo(p2));
    }

    bool nedge_1 = false;
    ON_Line t1_nedge;
    cdt_mesh::edge_t t1_e;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d1[i], etol) &&  NEAR_ZERO(p2d1[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t1_nedge = t1_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t1_e = cdt_mesh::edge_t(t1.v[i], t1.v[v2]);
	    nedge_1 = true;
	}
    }

    bool nedge_2 = false;
    ON_Line t2_nedge;
    cdt_mesh::edge_t t2_e;
    for (int i = 0; i < 3; i++) {
	if (NEAR_ZERO(p1d2[i], etol) &&  NEAR_ZERO(p2d2[i], etol)) {
	    //std::cout << "edge-only intersect - e1\n";
	    t2_nedge = t2_lines[i];
	    int v2 = (i < 2) ? i + 1 : 0;
	    t2_e = cdt_mesh::edge_t(t2.v[i], t2.v[v2]);
	    nedge_2 = true;
	}
    }

    // If either triangle thinks this isn't an edge intersect, we're done
    if (!nedge_1 || !nedge_2) {
	return -1;
    }

    // If the finite chords' maximum distance is too large, the edges
    // aren't properly aligned and we need the points
    double llen = -DBL_MAX;
    double lt[4];

    t1_nedge.ClosestPointTo(t2_nedge.PointAt(0), &(lt[0]));
    t1_nedge.ClosestPointTo(t2_nedge.PointAt(1), &(lt[1]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(0), &(lt[2]));
    t2_nedge.ClosestPointTo(t1_nedge.PointAt(1), &(lt[3]));

    double ll[4];
    ll[0] = t1_nedge.PointAt(0).DistanceTo(t2_nedge.PointAt(lt[2]));
    ll[1] = t1_nedge.PointAt(1).DistanceTo(t2_nedge.PointAt(lt[3]));
    ll[2] = t2_nedge.PointAt(0).DistanceTo(t1_nedge.PointAt(lt[0]));
    ll[3] = t2_nedge.PointAt(1).DistanceTo(t1_nedge.PointAt(lt[1]));
    for (int i = 0; i < 4; i++) {
	llen = (ll[i] > llen) ? ll[i] : llen;
    }
    // Max dist from line too large
    if ( llen > etol ) {
	return -1;
    }

    // If both points are on the same edge, it's an edge-only intersect.

    // For any vertices from one triangle that are interior to the opposite
    // edge, flag them for processing - almost-but-not-on-the-edge points
    // are a possible source of overlaps

    int process_pnt = 0;
    double vtol = 0.01;
    overt_t *v = NULL;

    v = omesh2->overts[t2_e.v[0]];
    process_pnt += near_edge_process(process, lt[0], vtol, v, omesh1, t1, omesh2, t2, vert_edge_cnts);

    v = omesh2->overts[t2_e.v[1]];
    process_pnt += near_edge_process(process, lt[1], vtol, v, omesh1, t1, omesh2, t2, vert_edge_cnts);

    v = omesh1->overts[t1_e.v[0]];
    process_pnt += near_edge_process(process, lt[2], vtol, v, omesh2, t2, omesh1, t1, vert_edge_cnts);

    v = omesh1->overts[t1_e.v[1]];
    process_pnt += near_edge_process(process, lt[3], vtol, v, omesh2, t2, omesh1, t1, vert_edge_cnts);

    // If the non-edge point of one of the triangles is clearly inside
    // the other mesh, we need to flag it for processing as well - there
    // are geometric situations where edge intersection information isn't
    // sufficient to identify all the points to consider.

    long t1_vind, t2_vind;

    // If it is an edge intersect, identify the point from each triangle not
    // on the edge.
    double cdist = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint tp = *omesh1->fmesh->pnts[t1.v[i]];
	double tdist = tp.DistanceTo(t1_nedge.ClosestPointTo(tp));
	if (tdist > cdist) {
	    t1_vind = t1.v[i];
	    cdist = tdist;
	}
    }
    cdist = -DBL_MAX;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint tp = *omesh2->fmesh->pnts[t2.v[i]];
	double tdist = tp.DistanceTo(t2_nedge.ClosestPointTo(tp));
	if (tdist > cdist) {
	    t2_vind = t2.v[i];
	    cdist = tdist;
	}
    }

    // See if the triangles are inside the mesh (center point test).  If so,
    // we have more work to do.
    //
    // We use the center point for this because we are only in this logic
    // due to the triangle intersection reporting the intersection points close
    // to one of the edges.  That being the case, the center should be in the
    // direction of the inside/outside behavior of interest, and not being a
    // vertex it is not likely to be aligned with another unrelated vertex in
    // the opposite mesh due to prior refinement steps.
    struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
    ON_3dPoint t1_f = omesh1->fmesh->tcenter(t1);
    bool t1_pinside = on_point_inside(s_cdt2, &t1_f);
    struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
    ON_3dPoint t2_f = omesh2->fmesh->tcenter(t2);
    bool t2_pinside = on_point_inside(s_cdt1, &t2_f);

    // For each remote vertex, see if it is very close to a vertex on the
    // opposite mesh.  If not, and it is inside the opposite mesh, we
    // can use it as a refinement point.  Otherwise, we need to split
    // triangle edges.
    if (t1_pinside || t2_pinside) {
	v = omesh1->overts[t1_vind];
	process_pnt += remote_vert_process(process, t1_pinside, omesh1, v, t1, omesh2, t2, vert_edge_cnts);
	v = omesh2->overts[t2_vind];
	process_pnt += remote_vert_process(process, t2_pinside, omesh2, v, t2, omesh1, t1, vert_edge_cnts);
    }

    return (process_pnt > 0) ? 1 : 0;
}

/*****************************************************************************
 * We're only concerned with specific categories of intersections between
 * triangles, so filter accordingly.
 * Return 0 if no intersection, 1 if coplanar intersection, 2 if non-coplanar
 * intersection.
 *****************************************************************************/
static int
tri_isect(
	bool process,
	omesh_t *omesh1, cdt_mesh::triangle_t &t1,
       	omesh_t *omesh2, cdt_mesh::triangle_t &t2,
	std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> *vert_edge_cnts
	)
{
    cdt_mesh::cdt_mesh_t *fmesh1 = omesh1->fmesh;
    cdt_mesh::cdt_mesh_t *fmesh2 = omesh2->fmesh;
    int coplanar = 0;
    point_t T1_V[3];
    point_t T2_V[3];
    point_t isectpt1 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};
    point_t isectpt2 = {MAX_FASTF, MAX_FASTF, MAX_FASTF};


    VSET(T1_V[0], fmesh1->pnts[t1.v[0]]->x, fmesh1->pnts[t1.v[0]]->y, fmesh1->pnts[t1.v[0]]->z);
    VSET(T1_V[1], fmesh1->pnts[t1.v[1]]->x, fmesh1->pnts[t1.v[1]]->y, fmesh1->pnts[t1.v[1]]->z);
    VSET(T1_V[2], fmesh1->pnts[t1.v[2]]->x, fmesh1->pnts[t1.v[2]]->y, fmesh1->pnts[t1.v[2]]->z);
    VSET(T2_V[0], fmesh2->pnts[t2.v[0]]->x, fmesh2->pnts[t2.v[0]]->y, fmesh2->pnts[t2.v[0]]->z);
    VSET(T2_V[1], fmesh2->pnts[t2.v[1]]->x, fmesh2->pnts[t2.v[1]]->y, fmesh2->pnts[t2.v[1]]->z);
    VSET(T2_V[2], fmesh2->pnts[t2.v[2]]->x, fmesh2->pnts[t2.v[2]]->y, fmesh2->pnts[t2.v[2]]->z);
    int isect = bg_tri_tri_isect_with_line(T1_V[0], T1_V[1], T1_V[2], T2_V[0], T2_V[1], T2_V[2], &coplanar, &isectpt1, &isectpt2);

    if (!isect) {
	// No intersection - we're done
	return 0;
    }

    ON_3dPoint p1(isectpt1[X], isectpt1[Y], isectpt1[Z]);
    ON_3dPoint p2(isectpt2[X], isectpt2[Y], isectpt2[Z]);
    if (p1.DistanceTo(p2) < ON_ZERO_TOLERANCE) {
	// Intersection is a single point - not volumetric
	return 0;
    }

#if 0
    // Trigger if our problem point of the moment is near an edge
    bool t1_edge_problem = (
	    PPCHECK(*fmesh1->pnts[t1.v[0]]) || PPCHECK(*fmesh1->pnts[t1.v[1]]) || PPCHECK(*fmesh1->pnts[t1.v[2]]));
    bool t2_edge_problem = (
	    PPCHECK(*fmesh2->pnts[t2.v[0]]) || PPCHECK(*fmesh2->pnts[t2.v[1]]) || PPCHECK(*fmesh2->pnts[t2.v[2]]));

    if (t1_edge_problem || t2_edge_problem) {
	if (t1_edge_problem) {
	    std::cout << "t1 edge problem pnt\n";
	}
	if (t2_edge_problem) {
	    std::cout << "t2 edge problem pnt\n";
	} 
    }
#endif
    isect_plot(fmesh1, t1.ind, fmesh2, t2.ind, &isectpt1, &isectpt2);

    // Past the trivial cases - we're going to have to know something about the
    // triangles' dimensions and boundaries to characterize the intersection
    ON_Line t1_lines[3];
    t1_lines[0] = ON_Line(*fmesh1->pnts[t1.v[0]], *fmesh1->pnts[t1.v[1]]);
    t1_lines[1] = ON_Line(*fmesh1->pnts[t1.v[1]], *fmesh1->pnts[t1.v[2]]);
    t1_lines[2] = ON_Line(*fmesh1->pnts[t1.v[2]], *fmesh1->pnts[t1.v[0]]);

    ON_Line t2_lines[3];
    t2_lines[0] = ON_Line(*fmesh2->pnts[t2.v[0]], *fmesh2->pnts[t2.v[1]]);
    t2_lines[1] = ON_Line(*fmesh2->pnts[t2.v[1]], *fmesh2->pnts[t2.v[2]]);
    t2_lines[2] = ON_Line(*fmesh2->pnts[t2.v[2]], *fmesh2->pnts[t2.v[0]]);
    double elen_min = DBL_MAX;
    for (int i = 0; i < 3; i++) {
	elen_min = (elen_min > t1_lines[i].Length()) ? t1_lines[i].Length() : elen_min;
	elen_min = (elen_min > t2_lines[i].Length()) ? t2_lines[i].Length() : elen_min;
    }
    double etol = 0.0001*elen_min;

    // See if we're intersecting very close to triangle vertices - if we
    // are, just flag those points for consideration
    bool vert_isect = false;
    double t1_i1_vdists[3];
    double t1_i2_vdists[3];
    double t2_i1_vdists[3];
    double t2_i2_vdists[3];
    for (int i = 0; i < 3; i++) {
	t1_i1_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(p1);
	t2_i1_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(p1);
    	t1_i2_vdists[i] = fmesh1->pnts[t1.v[i]]->DistanceTo(p2);
	t2_i2_vdists[i] = fmesh2->pnts[t2.v[i]]->DistanceTo(p2);
    }
    for (int i = 0; i < 3; i++) {
	if (t1_i1_vdists[i] < etol && t1_i2_vdists[i] < etol) {
	    if (process) {
		overt_t *v = omesh1->overts[t1.v[i]];
		isect_process_vert(omesh1, v, t1, omesh2, t2, vert_edge_cnts);
	    }
	    vert_isect = true;
	}
	if (t2_i1_vdists[i] < etol && t2_i2_vdists[i] < etol) {
	    if (process) {
		overt_t *v = omesh2->overts[t2.v[i]];
		isect_process_vert(omesh2, v, t2, omesh1, t1, vert_edge_cnts);
	    }
	    vert_isect = true;
	}
    }

    if (vert_isect) {
	if (process) {
	    return 1;
	} else {
	    // If we're not vertex processing, this category of intersection
	    // isn't one that warrants a hit return.
	    return 0;
	}
    }

#if 0
    // Because the edge-only test is deliberately loose, we need to check if
    // we've got a situation where all three vertices are already closely
    // aligned with vertices in the opposite mesh.  In that case, we don't have
    // enough degrees of freedom to resolve the overlap, and we need to mark
    // the edges for splitting.
    int a1 = aligned_isect(omesh1, t1, omesh2, t2, etol);
    int a2 = aligned_isect(omesh2, t2, omesh1, t1, etol);
    // If they're both aligned we're done, otherwise we need to keep checking
    if (a1 && a2) {
	// We won't use this information for most of the processing, but at the end
	// we may need to re-triangulate using this particular category of triangle
	// intersects as a guide.  Make a record.
	omesh1->aligned_ovlps[t1.ind][omesh2].insert(t2.ind);
	omesh2->aligned_ovlps[t2.ind][omesh1].insert(t1.ind);
	return 1;
    }
#endif

    ON_Line nedge;
    int near_edge = edge_only_isect(process, omesh1, t1, omesh2, t2, p1, p2, (ON_Line *)t1_lines, (ON_Line *)t2_lines, 0.3*elen_min, vert_edge_cnts);


    if (near_edge >= 0) {
	return near_edge;
    }

    // If we're not aligned or in an edge intersect situation, all three
    // vertices go into the refinement pot.
    for (int i = 0; i < 3; i++) {
	overt_t *v = omesh1->overts[t1.v[i]];
	if (!v) {
	    std::cout << "WARNING: - no overt for vertex??\n";
	    continue;
	}
	if (process) {
	    isect_process_vert(omesh1, v, t1, omesh2, t2, vert_edge_cnts);
	}
    }
    for (int i = 0; i < 3; i++) {
	overt_t *v = omesh2->overts[t2.v[i]];
	if (!v) {
	    std::cout << "WARNING: - no overt for vertex??\n";
	    continue;
	}
	if (process) {
	    isect_process_vert(omesh2, v, t2, omesh1, t1, vert_edge_cnts);
	}
    }

    return 1;
}

/******************************************************************************
 * As a first step, use the face bboxes to narrow down where we have potential
 * interactions between breps
 ******************************************************************************/

struct nf_info {
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> *check_pairs;
    cdt_mesh::cdt_mesh_t *cmesh;
};

static bool NearFacesPairsCallback(void *data, void *a_context) {
    struct nf_info *nf = (struct nf_info *)a_context;
    cdt_mesh::cdt_mesh_t *omesh = (cdt_mesh::cdt_mesh_t *)data;
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p1(nf->cmesh, omesh);
    std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *> p2(omesh, nf->cmesh);
    if ((nf->check_pairs->find(p1) == nf->check_pairs->end()) && (nf->check_pairs->find(p1) == nf->check_pairs->end())) {
	nf->check_pairs->insert(p1);
    }
    return true;
}

std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>
possibly_interfering_face_pairs(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs;
    RTree<void *, double, 3> rtree_fmeshes;
    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    const ON_BrepFace *i_face = s_i->brep->Face(i_fi);
	    ON_BoundingBox bb = i_face->BoundingBox();
	    cdt_mesh::cdt_mesh_t *fmesh = &s_i->fmeshes[i_fi];
	    struct nf_info nf;
	    nf.cmesh = fmesh;
	    nf.check_pairs = &check_pairs;
	    double fMin[3];
	    fMin[0] = bb.Min().x;
	    fMin[1] = bb.Min().y;
	    fMin[2] = bb.Min().z;
	    double fMax[3];
	    fMax[0] = bb.Max().x;
	    fMax[1] = bb.Max().y;
	    fMax[2] = bb.Max().z;
	    // Check the new box against the existing tree, and add any new
	    // interaction pairs to check_pairs
	    rtree_fmeshes.Search(fMin, fMax, NearFacesPairsCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    return check_pairs;
}

size_t
face_omesh_ovlps(
	std::set<std::pair<omesh_t *, omesh_t *>> check_pairs,
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> &edge_verts,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    size_t tri_isects = 0;

    edge_verts.clear();

    std::set<omesh_t *> a_omesh;
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cp_it->first->refinement_clear();
	cp_it->second->refinement_clear();
	a_omesh.insert(cp_it->first);
	a_omesh.insert(cp_it->second);
    }
    std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>> vert_edge_cnts;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = omesh1->fmesh->tris_tree.Overlaps(omesh2->fmesh->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    cdt_mesh::triangle_t t1 = omesh1->fmesh->tris_vect[tb_it->first];
		    cdt_mesh::triangle_t t2 = omesh2->fmesh->tris_vect[tb_it->second];
		    int isect = tri_isect(true, omesh1, t1, omesh2, t2, &vert_edge_cnts);
		    if (isect) {
			tri_isects++;
		    }
		}
	    }
	}
    }

    // Anything refinement vertex candidate that doesn't meet the criteria, erase
    std::set<omesh_t *>::iterator a_it;
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *om = *a_it;
	std::set<overt_t *> ev;
	std::map<overt_t *, std::set<long>>::iterator r_it;
	for (r_it = om->refinement_overts.begin(); r_it != om->refinement_overts.end(); r_it++) {

	    VPCHECK(r_it->first, NULL);

	    if (r_it->second.size() == 1) {
		ev.insert(r_it->first);
	    }
	}
	std::set<overt_t *>::iterator ev_it;
	for (ev_it = ev.begin(); ev_it != ev.end(); ev_it++) {
	    om->refinement_overts.erase(*ev_it);
	}
    }

    // Process the close-to-edge cases
    edge_verts.clear();
    std::set<overt_t *> everts;
    std::map<overt_t *, std::map<cdt_mesh::bedge_seg_t *, int>>::iterator ov_it;
    for (ov_it = vert_edge_cnts.begin(); ov_it != vert_edge_cnts.end(); ov_it++) {
	std::map<cdt_mesh::bedge_seg_t *, int>::iterator s_it;
	for (s_it = ov_it->second.begin(); s_it != ov_it->second.end(); s_it++) {
	    if (s_it->second > 1) {
		VPCHECK(ov_it->first, NULL);
		edge_verts[s_it->first].insert(ov_it->first);
		everts.insert(ov_it->first);
	    }
	}
    }

    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cp_it->first->plot(&edge_verts, f2omap);
	cp_it->second->plot(&edge_verts, f2omap);
    }

    return tri_isects;
}


void
orient_tri(cdt_mesh::cdt_mesh_t &fmesh, cdt_mesh::triangle_t &t)
{
    ON_3dVector tdir = fmesh.tnorm(t);
    ON_3dVector bdir = fmesh.bnorm(t);
    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
    if (flipped_tri) {
	long tmp = t.v[2];
	t.v[2] = t.v[1];
	t.v[1] = tmp;
    }
}

class revt_pt_t {
    public:
	ON_3dPoint spnt;
	ON_3dVector sn;
	overt_t *ov;
};


int
refine_edge_vert_sets (
	omesh_t *omesh,
	std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>> *edge_sets
)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;

    std::cout << "Processing " << s_cdt->name << " face " << omesh->fmesh->f_id << ":\n";

    std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>>::iterator es_it;

    int new_tris = 0; 

    while (edge_sets->size()) {
	std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>> updated_esets;

	cdt_mesh::uedge_t ue = edge_sets->begin()->first;
	std::vector<revt_pt_t> epnts = edge_sets->begin()->second;
	edge_sets->erase(edge_sets->begin());

	// Find the two triangles that we will be using to form the outer polygon
	std::set<size_t> rtris = omesh->fmesh->uedges2tris[ue];
	if (rtris.size() != 2) {
	    std::cout << "Error - could not associate uedge with two triangles??\n";
	}
	// For the involved triangle edges that are not the edge to be removed, we will
	// need to reassess their closest edge assignment after new edges are added.
	// Build up the set of those uedges for later processing.  While we are looping,
	// get initial data for the polygon build.
	std::map<int, long> t_pts;
	std::map<long, int> t_pts_map;
	std::set<cdt_mesh::uedge_t> pnt_reassignment_edges;
	int pnts_ind = 0;
	std::set<size_t>::iterator r_it;
	for (r_it = rtris.begin(); r_it != rtris.end(); r_it++) {
	    cdt_mesh::triangle_t tri = omesh->fmesh->tris_vect[*r_it];
	    for (int i = 0; i < 3; i++) {
		if (t_pts_map.find(tri.v[i]) == t_pts_map.end()) {
		    pnts_ind = (int)t_pts_map.size();
		    t_pts[pnts_ind] = tri.v[i];
		    t_pts_map[tri.v[i]] = pnts_ind;
		}
	    }
	    std::set<cdt_mesh::uedge_t> tuedges = omesh->fmesh->uedges(tri);
	    pnt_reassignment_edges.insert(tuedges.begin(), tuedges.end());
	}
	pnt_reassignment_edges.erase(ue);

	if (t_pts.size() != 4) {
	    std::cout << "Error - found " << t_pts.size() << " triangle points??\n";
	}

	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(pnts_ind+1, sizeof(point_t), "fitting points");
	std::map<int, long>::iterator p_it;
	for (p_it = t_pts.begin(); p_it != t_pts.end(); p_it++) {
	    ON_3dPoint *p = omesh->fmesh->pnts[p_it->second];
	    fpnts[p_it->first][X] = p->x;
	    fpnts[p_it->first][Y] = p->y;
	    fpnts[p_it->first][Z] = p->z;
	}
	if (bn_fit_plane(&pcenter, &pnorm, pnts_ind+1, fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");

	ON_Plane fit_plane(pcenter, pnorm);

	// Build our polygon out of the two triangles.
	//
	// First step, add the 2D projection of the triangle vertices to the
	// polygon data structure.
	cdt_mesh::cpolygon_t *polygon = new cdt_mesh::cpolygon_t;
	polygon->pdir = fit_plane.Normal();
	polygon->tplane = fit_plane;
	for (p_it = t_pts.begin(); p_it != t_pts.end(); p_it++) {
	    double u, v;
	    long pind = p_it->second;
	    ON_3dPoint *p = omesh->fmesh->pnts[pind];
	    fit_plane.ClosestPointTo(*p, &u, &v);
	    std::pair<double, double> proj_2d;
	    proj_2d.first = u;
	    proj_2d.second = v;
	    polygon->pnts_2d.push_back(proj_2d);
	    polygon->p2o[polygon->pnts_2d.size() - 1] = pind;
	}

	// Initialize the polygon edges with one of the triangles.
	cdt_mesh::triangle_t tri1 = omesh->fmesh->tris_vect[*(rtris.begin())];
	rtris.erase(rtris.begin());
	struct cdt_mesh::edge_t e1(t_pts_map[tri1.v[0]], t_pts_map[tri1.v[1]]);
	struct cdt_mesh::edge_t e2(t_pts_map[tri1.v[1]], t_pts_map[tri1.v[2]]);
	struct cdt_mesh::edge_t e3(t_pts_map[tri1.v[2]], t_pts_map[tri1.v[0]]);
	polygon->add_edge(e1);
	polygon->add_edge(e2);
	polygon->add_edge(e3);

	// Grow the polygon with the other triangle.
	std::set<cdt_mesh::uedge_t> new_edges;
	std::set<cdt_mesh::uedge_t> shared_edges;
	cdt_mesh::triangle_t tri2 = omesh->fmesh->tris_vect[*(rtris.begin())];
	rtris.erase(rtris.begin());
	for (int i = 0; i < 3; i++) {
	    int v1 = i;
	    int v2 = (i < 2) ? i + 1 : 0;
	    cdt_mesh::uedge_t ue1(tri2.v[v1], tri2.v[v2]);
	    cdt_mesh::uedge_t nue1(t_pts_map[tri2.v[v1]], t_pts_map[tri2.v[v2]]);
	    if (ue1 != ue) {
		new_edges.insert(nue1);
	    } else {
		shared_edges.insert(nue1);
	    }
	}
	polygon->replace_edges(new_edges, shared_edges);

	{
	FILE *t1plot = fopen("t1.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri1, &c, t1plot, 255, 0, 0);
	fclose(t1plot);
	}
	{
	FILE *t2plot = fopen("t2.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri2, &c, t2plot, 255, 0, 0);
	fclose(t2plot);
	}

	// Grow the polygon with the other triangle.
	std::set<overt_t *> new_overts;
	bool have_inside = false;
	for (size_t i = 0; i < epnts.size(); i++) {
	    bool skip_epnt = false;

	    VPCHECK(epnts[i].ov, NULL);

	    ON_BoundingBox sbb(epnts[i].spnt,epnts[i].spnt);
	    ON_3dVector vmin = epnts[i].ov->bb.Min() - epnts[i].ov->bb.Center();
	    ON_3dVector vmax = epnts[i].ov->bb.Max() - epnts[i].ov->bb.Center();
	    vmin.Unitize();
	    vmax.Unitize();
	    vmin = vmin * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    vmax = vmax * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    sbb.m_min = epnts[i].spnt + vmin;
	    sbb.m_max = epnts[i].spnt + vmax;

	    std::set <overt_t *> nv = omesh->vert_search(sbb);
	    std::set<overt_t *>::iterator v_it;
	    for (v_it = nv.begin(); v_it != nv.end(); v_it++) {

		ON_3dPoint cvpnt = (*v_it)->vpnt();
		double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
		if (cvpnt.DistanceTo(epnts[i].spnt) < cvbbdiag) {
		    // Too close to a vertex in the current mesh, skip
		    std::cout << "skip epnt, too close to vert\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }
	    // If the point is too close to a brep face edge,
	    // we also need to reject it to avoid creating degenerate
	    // triangles
	    std::set<cdt_mesh::uedge_t>::iterator ue_it;
	    std::set<cdt_mesh::uedge_t> b_ue_1 = omesh->fmesh->b_uedges(tri1);
	    for (ue_it = b_ue_1.begin(); ue_it != b_ue_1.end(); ue_it++) {
		cdt_mesh::uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }
	    std::set<cdt_mesh::uedge_t> b_ue_2 = omesh->fmesh->b_uedges(tri2);
	    for (ue_it = b_ue_2.begin(); ue_it != b_ue_2.end(); ue_it++) {
		cdt_mesh::uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }

	    bool inside = false;
	    {
	        // Use the overt point for this - the closest surface point will
	        // always pass, so it's not usable for rejection.
		ON_3dPoint ovpnt = epnts[i].ov->vpnt();
		double u, v;
		fit_plane.ClosestPointTo(ovpnt, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		inside = polygon->point_in_polygon(polygon->pnts_2d.size() - 1, false);
		polygon->pnts_2d.pop_back();
	    }
	    if (inside) {
		ON_3dPoint p = epnts[i].spnt;
		double u, v;
		fit_plane.ClosestPointTo(p, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		size_t p2dind = polygon->pnts_2d.size() - 1;

		// We're going to use it - add new 3D point to CDT
		long f3ind = omesh->fmesh->add_point(new ON_3dPoint(epnts[i].spnt));
		long fnind = omesh->fmesh->add_normal(new ON_3dPoint(epnts[i].sn));
		CDT_Add3DPnt(s_cdt, omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		CDT_Add3DNorm(s_cdt, omesh->fmesh->normals[fnind], omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		omesh->fmesh->nmap[f3ind] = fnind;
		overt_t *nvrt = omesh->vert_add(f3ind, &sbb);
		new_overts.insert(nvrt);
		polygon->p2o[p2dind] = f3ind;
		polygon->interior_points.insert(p2dind);

		have_inside = true;
	    }

	}

	if (!have_inside) {
	    // No point in doing this if none of the points are interior
	    delete polygon;
	    continue;
	}

	polygon->cdt();
	//omesh->fmesh->boundary_edges_plot("bedges.plot3");

	if (polygon->tris.size()) {
	    unsigned char rgb[3] = {0,255,255};
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	    omesh->fmesh->tri_remove(tri1);
	    omesh->fmesh->tri_remove(tri2);
	    std::set<cdt_mesh::triangle_t>::iterator v_it;
	    for (v_it = polygon->tris.begin(); v_it != polygon->tris.end(); v_it++) {
		cdt_mesh::triangle_t vt = *v_it;
		orient_tri(*omesh->fmesh, vt);
		omesh->fmesh->tri_add(vt);
	    }

	    new_tris += polygon->tris.size();

	    //omesh->fmesh->tris_set_plot(polygon->tris, "poly_tris.plot3");
	    //polygon->polygon_plot_in_plane("poly.plot3");
	} else {
	    std::cout << "polygon cdt failed!\n";
	    bu_file_delete("tri_replace_pair_fail.plot3");
	    polygon->polygon_plot_in_plane("tri_replace_pair_fail.plot3");
	}

	if (!omesh->fmesh->valid(1)) {
	    std::cout << "CDT broke mesh!\n";
	    polygon->polygon_plot_in_plane("poly.plot3");
	}
	delete polygon;

	// Have new triangles, update overts
	std::set<overt_t *>::iterator n_it;
	for (n_it = new_overts.begin(); n_it != new_overts.end(); n_it++) {
	    (*n_it)->omesh->vupdate(*n_it);
	}

	// Reassign points to their new closest edge (may be the same edge, but we need
	// to check)
	std::set<cdt_mesh::uedge_t>::iterator pre_it;
	for (pre_it = pnt_reassignment_edges.begin(); pre_it != pnt_reassignment_edges.end(); pre_it++) {
	    cdt_mesh::uedge_t rue = *pre_it;
	    std::vector<revt_pt_t> old_epnts = (*edge_sets)[rue];
	    std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>>::iterator er_it = edge_sets->find(rue);
	    edge_sets->erase(er_it);
	    for (size_t i = 0; i < old_epnts.size(); i++) {
		overt_t *ov = old_epnts[i].ov;
		std::set<cdt_mesh::uedge_t> close_edges = omesh->interior_uedges_search(ov->bb);
		double mindist = DBL_MAX; 
		cdt_mesh::uedge_t closest_uedge;
		std::set<cdt_mesh::uedge_t>::iterator c_it;
		for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
		    cdt_mesh::uedge_t cue = *c_it;
		    double dline = omesh->fmesh->uedge_dist(cue, old_epnts[i].spnt);
		    if (mindist > dline) {
			closest_uedge = cue;
			mindist = dline;
		    }
		}

		(*edge_sets)[closest_uedge].push_back(old_epnts[i]);
	    }
	}
    }

    return new_tris;
}


overt_t *
get_largest_overt(std::set<overt_t *> &verts)
{
    double elen = 0;
    overt_t *l = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *v = *v_it;
	if (v->min_len() > elen) {
	    elen = v->min_len();
	    l = v;
	}
    }
    return l;
}

overt_t *
closest_overt(std::set<overt_t *> &verts, overt_t *v)
{
    overt_t *closest = NULL;
    double dist = DBL_MAX;
    ON_3dPoint p1 = *v->omesh->fmesh->pnts[v->p_id];
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *c = *v_it;
	ON_3dPoint p2 = *c->omesh->fmesh->pnts[c->p_id];
	double d = p1.DistanceTo(p2);
	if (dist > d) {
	    closest = c;
	    dist = d;
	}
    }
    return closest;
}

static int
adjust_overt(overt_t *v, overt_t *v_other, ON_3dPoint &target_point, double pdist)
{
    ON_3dPoint s_p;
    ON_3dVector s_n;
    bool feval = closest_surf_pnt(s_p, s_n, *v->omesh->fmesh, &target_point, pdist);
    if (feval) {

	// See if s_p is an actual move - if not, this is a no-op
	ON_3dPoint p = v->vpnt();
	if (p.DistanceTo(s_p) < ON_ZERO_TOLERANCE) {
	    return 0;
	}

	// If we're adjusting, that's the only refinement we will do with this vert
	v_other->omesh->refinement_overts.erase(v);

	v->omesh->remove_vtree_vert(v);
	(*v->omesh->fmesh->pnts[v->p_id]) = s_p;
	(*v->omesh->fmesh->normals[v->omesh->fmesh->nmap[v->p_id]]) = s_n;

	v->omesh->vupdate(v);

	// We just changed the vertex point values - need to update all the
	// connected vertices which might be impacted...
	v->omesh->verts_one_ring_update(v->p_id);
	return 1;
    } else {
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)v_other->omesh->fmesh->p_cdt;
	std::cout << s_cdt2->name << " face " << v_other->omesh->fmesh->f_id << " closest point eval failure\n";
	return 0;
    }

}

// TODO - right now, we've locked brep face edge points.  This
// isn't ideal, as we could in principle move those points locally
// along the edge curve to align with other points.  This way we
// end up with small triangles to handle fixed edge points.  However,
// adjusting the brep edge points will involved local knowledge
// on the part of each edge vertex about the distances to the next
// edge vertices on either side, as well as updating the corresponding
// vert values throughout the data structures.
//
// Return how many vertices were actually moved a significant distance
// per their bounding box dimensions
int
adjust_overt_pair(overt_t *v1, overt_t *v2)
{
    v1->omesh->refinement_overts.erase(v2);
    v2->omesh->refinement_overts.erase(v1);

    // If we've got two edge vertices, no dice
    if (v1->edge_vert() && v2->edge_vert()) return 0;

    ON_3dPoint p1 = v1->vpnt();
    ON_3dPoint p2 = v2->vpnt();
    double pdist = p1.DistanceTo(p2);
    ON_3dPoint s1_p, s2_p;
    ON_3dVector s1_n, s2_n;

    if (v1->edge_vert()) {
	// It's all up to v2 - if we're too far away, skip it
	if (pdist > v2->min_len()*0.5) {
	    return 0;
	}
	return adjust_overt(v2, v1, p1, pdist);
    }

    if (v2->edge_vert()) {
	// It's all up to v1 - if we're too far away, skip it
	if (pdist > v1->min_len()*0.5) {
	    return 0;
	}
	return adjust_overt(v1, v2, p2, pdist);
    }

    ON_Line l(p1,p2);
    // Weight the t parameter on the line so we are closer to the vertex
    // with the shorter edge length (i.e. less freedom to move without
    // introducing locally severe mesh distortions.)
    double t = 1 - (v2->min_len() / (v1->min_len() + v2->min_len()));
    ON_3dPoint p_wavg = l.PointAt(t);
    if ((p1.DistanceTo(p_wavg) > v1->min_len()*0.5) || (p2.DistanceTo(p_wavg) > v2->min_len()*0.5)) {
	std::cout << "WARNING: large point shift compared to triangle edge length.\n";
    }

    int adj_cnt = 0;
    adj_cnt += adjust_overt(v1, v2, p_wavg, pdist);
    adj_cnt += adjust_overt(v2, v1, p_wavg, pdist);

    return adj_cnt;
}

size_t
adjust_close_verts(std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs)
{
    std::map<overt_t *, std::set<overt_t*>> vert_ovlps;
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 == s_cdt2) continue;
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt1->name, omesh1->fmesh->f_id);
	omesh1->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt2->name, omesh2->fmesh->f_id);
	omesh2->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	std::cout << "omesh1 vtree cnt: " << omesh1->vtree.Count() << "\n";
	std::cout << "omesh2 vtree cnt: " << omesh2->vtree.Count() << "\n";
#endif
	std::set<std::pair<long, long>> vert_pairs;
	vert_pairs.clear();
	omesh1->vtree.Overlaps(omesh2->vtree, &vert_pairs);
	//std::cout << "(" << s_cdt1->name << "," << omesh1->fmesh->f_id << ")+(" << s_cdt2->name << "," << omesh2->fmesh->f_id << "): " << vert_pairs.size() << " vert box overlaps\n";
	std::set<std::pair<long, long>>::iterator v_it;
	for (v_it = vert_pairs.begin(); v_it != vert_pairs.end(); v_it++) {
	    long v_first = (long)v_it->first;
	    long v_second = (long)v_it->second;
	    overt_t *v1 = omesh1->overts[v_first];
	    overt_t *v2 = omesh2->overts[v_second];
	    vert_ovlps[v1].insert(v2);
	    vert_ovlps[v2].insert(v1);
	}
    }
    //std::cout << "Found " << vert_ovlps.size() << " vertices with box overlaps\n";


    std::queue<std::pair<overt_t *, overt_t *>> vq;
    std::set<overt_t *> vq_multi;
    std::map<overt_t *, std::set<overt_t*>>::iterator vo_it;
    for (vo_it = vert_ovlps.begin(); vo_it != vert_ovlps.end(); vo_it++) {
	overt_t *v = vo_it->first;
	if (vo_it->second.size() > 1) {
	    vq_multi.insert(v);
	    continue;
	}
	overt_t *v_other = *vo_it->second.begin();
	if (vert_ovlps[v_other].size() > 1) {
	    // The other point has multiple overlapping points
	    vq_multi.insert(v);
	    continue;
	}
	// Both v and it's companion only have one overlapping point
	vq.push(std::make_pair(v,v_other));
    }

    //std::cout << "Have " << vq.size() << " simple interactions\n";
    //std::cout << "Have " << vq_multi.size() << " complex interactions\n";
    std::set<overt_t *> adjusted;

    int adjusted_overts = 0;
    while (!vq.empty()) {
	std::pair<overt_t *, overt_t *> vpair = vq.front();
	vq.pop();

	if (adjusted.find(vpair.first) == adjusted.end() && adjusted.find(vpair.second) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(vpair.first, vpair.second);
	    adjusted.insert(vpair.first);
	    adjusted.insert(vpair.second);
	}
    }

    // If the box structure is more complicated, we need to be a bit selective
    while (vq_multi.size()) {
	overt_t *l = get_largest_overt(vq_multi);
	if (!l) {
	    vq_multi.erase(l);
	    continue;
	}
	overt_t *c = closest_overt(vert_ovlps[l], l);
	vq_multi.erase(l);
	vq_multi.erase(c);
	vert_ovlps[l].erase(c);
	vert_ovlps[c].erase(l);
	//std::cout << "COMPLEX - adjusting 1 pair only:\n";
	if (adjusted.find(l) == adjusted.end() && adjusted.find(c) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(l, c);
	    adjusted.insert(l);
	    adjusted.insert(c);
	}
    }

    return adjusted_overts;
}


void
replace_edge_split_tri(cdt_mesh::cdt_mesh_t &fmesh, size_t t_id, long np_id, cdt_mesh::uedge_t &split_edge)
{
    cdt_mesh::triangle_t &t = fmesh.tris_vect[t_id];

    fmesh.tri_plot(t, "replacing.plot3");
    // Find the two triangle edges that weren't split - these are the starting points for the
    // new triangles
    cdt_mesh::edge_t e1, e2;
    int ecnt = 0;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	cdt_mesh::edge_t ec(v0, v1);
	cdt_mesh::uedge_t uec(ec);
	if (uec != split_edge) {
	    if (!ecnt) {
		e1 = ec;
	    } else {
		e2 = ec;
	    }
	    ecnt++;
	}
    }


    cdt_mesh::triangle_t ntri1, ntri2;
    ntri1.v[0] = e1.v[0];
    ntri1.v[1] = np_id;
    ntri1.v[2] = e1.v[1];
    orient_tri(fmesh, ntri1);

    ntri2.v[0] = e2.v[0];
    ntri2.v[1] = np_id;
    ntri2.v[2] = e2.v[1];
    orient_tri(fmesh, ntri2);

    fmesh.tri_remove(t);
    fmesh.tri_add(ntri1);
    fmesh.tri_add(ntri2);

    std::set<cdt_mesh::uedge_t> nedges;
    std::set<cdt_mesh::uedge_t> n1 = fmesh.uedges(ntri1);
    std::set<cdt_mesh::uedge_t> n2 = fmesh.uedges(ntri2);
    nedges.insert(n1.begin(), n1.end());
    nedges.insert(n2.begin(), n2.end());
    std::set<cdt_mesh::uedge_t>::iterator n_it;

    fmesh.tri_plot(ntri1, "nt1.plot3");
    fmesh.tri_plot(ntri2, "nt2.plot3");
}

int
ovlp_split_edge(
	std::set<cdt_mesh::bedge_seg_t *> *nsegs,
	cdt_mesh::bedge_seg_t *eseg, double t,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    int replaced_tris = 0;

    // 1.  Get the triangle from each face associated with the edge segment to be split (should only
    // be one per face, since we're working with boundary edges.)
    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
    cdt_mesh::cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
    cdt_mesh::cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];

    // Translate the tseg verts to their parent indices in order to get
    // valid triangle lookups
    cdt_mesh::cpolyedge_t *pe1 = eseg->tseg1;
    cdt_mesh::cpolyedge_t *pe2 = eseg->tseg2;
    cdt_mesh::cpolygon_t *poly1 = pe1->polygon;
    cdt_mesh::cpolygon_t *poly2 = pe2->polygon;
    long ue1_1 = fmesh_f1.p2d3d[poly1->p2o[eseg->tseg1->v[0]]];
    long ue1_2 = fmesh_f1.p2d3d[poly1->p2o[eseg->tseg1->v[1]]];
    cdt_mesh::uedge_t ue1(ue1_1, ue1_2);
    long ue2_1 = fmesh_f2.p2d3d[poly2->p2o[eseg->tseg2->v[0]]];
    long ue2_2 = fmesh_f2.p2d3d[poly2->p2o[eseg->tseg2->v[1]]];
    cdt_mesh::uedge_t ue2(ue2_1, ue2_2);
    fmesh_f1.brep_edges.erase(ue1);
    fmesh_f2.brep_edges.erase(ue2);
    fmesh_f1.ue2b_map.erase(ue1);
    fmesh_f2.ue2b_map.erase(ue2);
    //ON_3dPoint ue1_p1 = *fmesh_f1.pnts[ue1.v[0]];
    //ON_3dPoint ue1_p2 = *fmesh_f1.pnts[ue1.v[1]];
    //std::cout << f_id1 << " ue1: " << ue1.v[0] << "," << ue1.v[1] << ": " << ue1_p1.x << "," << ue1_p1.y << "," << ue1_p1.z << " -> " << ue1_p2.x << "," << ue1_p2.y << "," << ue1_p2.z << "\n";
    //ON_3dPoint ue2_p1 = *fmesh_f2.pnts[ue2.v[0]];
    //ON_3dPoint ue2_p2 = *fmesh_f2.pnts[ue2.v[1]];
    //std::cout << f_id2 << " ue2: " << ue2.v[0] << "," << ue2.v[1] << ": " << ue2_p1.x << "," << ue2_p1.y << "," << ue2_p1.z << " -> " << ue2_p2.x << "," << ue2_p2.y << "," << ue2_p2.z << "\n";
    std::set<size_t> f1_tris = fmesh_f1.uedges2tris[ue1];
    std::set<size_t> f2_tris = fmesh_f2.uedges2tris[ue2];

    int eind = eseg->edge_ind;
    std::set<cdt_mesh::bedge_seg_t *> epsegs = s_cdt_edge->e2polysegs[eind];
    epsegs.erase(eseg);
    std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
    if (!esegs_split.size()) {
	std::cerr << "SPLIT FAILED!\n";
	return -1;
    }

    if (nsegs) {
	(*nsegs).insert(esegs_split.begin(), esegs_split.end());
    }

    epsegs.insert(esegs_split.begin(), esegs_split.end());
    s_cdt_edge->e2polysegs[eind].clear();
    s_cdt_edge->e2polysegs[eind] = epsegs;
    std::set<cdt_mesh::bedge_seg_t *>::iterator es_it;
    std::set<cdt_mesh::uedge_t> f1_new_ue;
    std::set<cdt_mesh::uedge_t> f2_new_ue;
    for (es_it = esegs_split.begin(); es_it != esegs_split.end(); es_it++) {
	cdt_mesh::bedge_seg_t *es = *es_it;
	int fid1 = s_cdt_edge->brep->m_T[es->tseg1->trim_ind].Face()->m_face_index;
	int fid2 = s_cdt_edge->brep->m_T[es->tseg2->trim_ind].Face()->m_face_index;
	cdt_mesh::cdt_mesh_t &f1 = s_cdt_edge->fmeshes[fid1];
	cdt_mesh::cdt_mesh_t &f2 = s_cdt_edge->fmeshes[fid2];
	cdt_mesh::cpolyedge_t *pe_1 = es->tseg1;
	cdt_mesh::cpolyedge_t *pe_2 = es->tseg2;
	cdt_mesh::cpolygon_t *poly_1 = pe_1->polygon;
	cdt_mesh::cpolygon_t *poly_2 = pe_2->polygon;
	long nue1_1 = fmesh_f1.p2d3d[poly_1->p2o[es->tseg1->v[0]]];
	long nue1_2 = fmesh_f1.p2d3d[poly_1->p2o[es->tseg1->v[1]]];
	cdt_mesh::uedge_t ue_1(nue1_1, nue1_2);
	long nue2_1 = fmesh_f2.p2d3d[poly_2->p2o[es->tseg2->v[0]]];
	long nue2_2 = fmesh_f2.p2d3d[poly_2->p2o[es->tseg2->v[1]]];
	cdt_mesh::uedge_t ue_2(nue2_1, nue2_2);
	f1.brep_edges.insert(ue_1);
	f2.brep_edges.insert(ue_2);
	f1_new_ue.insert(ue_1);
	f2_new_ue.insert(ue_2);
	f1.ue2b_map[ue_1] = es;
	f2.ue2b_map[ue_2] = es;
    }

    // Need to return one of the inserted verts from this process - we need to
    // check if that point is going to require any splitting on any other
    // objects' faces.  Any triangle from another object "close" to the new
    // point should have a vertex that can be adjusted, or if not one needs to
    // be introduced.
    long np_id;
    if (f_id1 == f_id2) {
	std::set<size_t> ftris;
	std::set<size_t>::iterator tr_it;
	cdt_mesh::uedge_t ue;
	ue = (f1_tris.size()) ? ue1 : ue2;
	ftris.insert(f1_tris.begin(), f1_tris.end());
	ftris.insert(f2_tris.begin(), f2_tris.end());
	np_id = fmesh_f1.pnts.size() - 1;
	fmesh_f1.ep.insert(np_id);
	for (tr_it = ftris.begin(); tr_it != ftris.end(); tr_it++) {
	    replace_edge_split_tri(fmesh_f1, *tr_it, np_id, ue);
	    replaced_tris++;
	}
	omesh_t *om = f2omap[&fmesh_f1];
	om->vert_add(np_id);

    } else {
	np_id = fmesh_f1.pnts.size() - 1;
	fmesh_f1.ep.insert(np_id);
	replace_edge_split_tri(fmesh_f1, *f1_tris.begin(), np_id, ue1);
	replaced_tris++;
	omesh_t *om1 = f2omap[&fmesh_f1];
	om1->vert_add(np_id);

	np_id = fmesh_f2.pnts.size() - 1;
	fmesh_f2.ep.insert(np_id);
	replace_edge_split_tri(fmesh_f2, *f2_tris.begin(), np_id, ue2);
	replaced_tris++;
    	omesh_t *om2 = f2omap[&fmesh_f2];
	om2->vert_add(np_id);
    }

    return replaced_tris;
}

// Find the point on the edge nearest to the point, and split the edge at that point.
//
// TODO - probably need to get more aggressive about splitting near verts as the
// iteration count increases - if there's no other way to resolve the mesh, that's
// what we need to do, small triangles or not...
int
bedge_split_at_t(
	cdt_mesh::bedge_seg_t *eseg, double t,
	std::set<cdt_mesh::bedge_seg_t *> *nsegs,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    int replaced_tris = 0;
    ON_NurbsCurve *nc = eseg->nc;
    ON_3dPoint cep = nc->PointAt(t);
    double epdist1 = eseg->e_start->DistanceTo(cep);
    double epdist2 = eseg->e_end->DistanceTo(cep);
    double lseg_check = 0.1 * eseg->e_start->DistanceTo(*eseg->e_end);
    if (epdist1 > lseg_check && epdist2 > lseg_check) {
	// If the point is not close to a start/end point on the edge then split the edge.
	int rtris = ovlp_split_edge(nsegs, eseg, t, f2omap);
	if (rtris >= 0) {
	    replaced_tris += rtris;
	} else {
	    std::cout << "split failed\n";
	}
    } else {
	std::cout << "split point too close to vertices - skip\n";
    }
    return replaced_tris;
}

// Find the point on the edge nearest to the point, and split the edge at that point.
int
bedge_split_near_pnt(
	cdt_mesh::bedge_seg_t *eseg, ON_3dPoint &p,
	std::set<cdt_mesh::bedge_seg_t *> *nsegs,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    ON_NurbsCurve *nc = eseg->nc;
    ON_Interval domain(eseg->edge_start, eseg->edge_end);
    double t;
    ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
    return bedge_split_at_t(eseg, t, nsegs, f2omap);
}

// Find the point on the edge nearest to the vert point.
int
bedge_split_near_vert(
	std::map<cdt_mesh::bedge_seg_t *, overt_t *> &edge_vert,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    int replaced_tris = 0;
    std::map<cdt_mesh::bedge_seg_t *, overt_t *>::iterator ev_it;
    for (ev_it = edge_vert.begin(); ev_it != edge_vert.end(); ev_it++) {
	cdt_mesh::bedge_seg_t *eseg = ev_it->first;
	overt_t *v = ev_it->second;
	ON_3dPoint p = v->vpnt();
	replaced_tris += bedge_split_near_pnt(eseg, p, NULL, f2omap);
    }
    return replaced_tris;
}

int
bedge_split_near_verts(
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> &edge_verts,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    int replaced_tris = 0;
    std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
    for (ev_it = edge_verts.begin(); ev_it != edge_verts.end(); ev_it++) {
	std::set<overt_t *> verts = ev_it->second;
	std::set<cdt_mesh::bedge_seg_t *> segs;
	segs.insert(ev_it->first);

	while (verts.size()) {
	    overt_t *v = *verts.begin();
	    ON_3dPoint p = v->vpnt();
	    verts.erase(v);

	    cdt_mesh::bedge_seg_t *eseg_split = NULL;
	    double split_t = -1.0;
	    double closest_dist = DBL_MAX;
	    std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    //std::cout << "segs size: " << segs.size() << "\n";
	    for (e_it = segs.begin(); e_it != segs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *eseg = *e_it;
		ON_NurbsCurve *nc = eseg->nc;
		ON_Interval domain(eseg->edge_start, eseg->edge_end);
		double t;
		ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
		ON_3dPoint cep = nc->PointAt(t);
		double ecdist = cep.DistanceTo(p);
		if (closest_dist > ecdist) {
		    //std::cout << "closest_dist: " << closest_dist << "\n";
		    //std::cout << "ecdist: " << ecdist << "\n";
		    closest_dist = ecdist;
		    eseg_split = eseg;
		    split_t = t;
		}
	    }

	    std::set<cdt_mesh::bedge_seg_t *> nsegs;
	    int ntri_cnt = bedge_split_at_t(eseg_split, split_t, &nsegs, f2omap);
	    if (ntri_cnt) {
		segs.erase(eseg_split);
		replaced_tris += ntri_cnt;
		segs.insert(nsegs.begin(), nsegs.end());
	    }
	}
    }

    return replaced_tris;
}

void
check_faces_validity(std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> &check_pairs)
{
    int verbosity = 0;
    std::set<cdt_mesh::cdt_mesh_t *> fmeshes;
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh::cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh::cdt_mesh_t *fmesh2 = cp_it->second;
	fmeshes.insert(fmesh1);
	fmeshes.insert(fmesh2);
    }
    if (verbosity > 0) {
	std::cout << "Full face validity check results:\n";
    }
    bool valid = true;
    std::set<cdt_mesh::cdt_mesh_t *>::iterator f_it;
    for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = *f_it;
	if (!fmesh->valid(verbosity)) {
	    valid = false;
	}
#if 0
	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	std::string fpname = std::to_string(id) + std::string("_") + std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	fmesh->tris_plot(fpname.c_str());
#endif
    }
    if (!valid) {
	bu_exit(1, "fatal mesh damage");
    }
}

int
omesh_interior_edge_verts(std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs)
{
    std::set<omesh_t *> omeshes;
    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	if (omesh1->refinement_overts.size()) {
	    omeshes.insert(omesh1);
	}
	if (omesh2->refinement_overts.size()) {
	    omeshes.insert(omesh2);
	}
    }
    std::cout << "Need to split triangles in " << omeshes.size() << " meshes\n";

    int rcnt = 0;

    std::set<omesh_t *>::iterator o_it;
    for (o_it = omeshes.begin(); o_it != omeshes.end(); o_it++) {
	omesh_t *omesh = *o_it;

	std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>> edge_sets;

	// From the refinement_overts set, build up the set of vertices
	// that look like they are intruding
	std::set<overt_t *> omesh_rverts;
	std::map<overt_t *, std::set<long>>::iterator r_it;
	for (r_it = omesh->refinement_overts.begin(); r_it != omesh->refinement_overts.end(); r_it++) {
	    if (r_it->second.size() > 1) {
		omesh_rverts.insert(r_it->first);
	    }
	}


	while (omesh_rverts.size()) {

	    overt_t *ov = *omesh_rverts.begin();

	    VPCHECK(ov, NULL);

	    omesh_rverts.erase(ov);
	    ON_3dPoint ovpnt = ov->vpnt();

	    ON_3dPoint spnt;
	    ON_3dVector sn;
	    double dist = ov->bb.Diagonal().Length() * 10;

	    if (!closest_surf_pnt(spnt, sn, *omesh->fmesh, &ovpnt, 2*dist)) {
		std::cout << "closest point failed\n";
		continue;
	    }

	    int problem_flag = 0;

	    // Check this point against the mesh vert tree - if we're
	    // extremely close to an existing vertex, we don't want to split
	    // and create extremely tiny triangles - vertex adjustment should
	    // handle super-close points...
	    ON_BoundingBox pbb(spnt, spnt);
	    std::set<overt_t *> cverts = omesh->vert_search(pbb);
	    if (problem_flag && !cverts.size()) {
		cverts = omesh->vert_search(pbb);
	    }

	    // If we're close to a vertex, find the vertices from the other
	    // mesh that overlap with that vertex.  If there are, and if the
	    // surface point is close to them per the vert bbox dimension,
	    // skip that point as a refinement point in this step.
	    if (cverts.size()) {
		double spdist = ovpnt.DistanceTo(spnt);

		// Find the closest vertex, and use its bounding box size as a
		// gauge for how close is too close for the surface point
		double vdist = DBL_MAX;
		double bbdiag = DBL_MAX;
		bool skip = false;
		std::set<overt_t *>::iterator v_it;
		for (v_it = cverts.begin(); v_it != cverts.end(); v_it++) {
		    ON_3dPoint cvpnt = (*v_it)->vpnt();
		    double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
		    if (cvpnt.DistanceTo(spnt) < cvbbdiag) {
			// Too close to a vertex in the current mesh, skip
			skip = true;
			break;
		    }
		    double cvdist = ovpnt.DistanceTo(cvpnt);
		    if (cvdist < vdist) {
			vdist = cvdist;
			bbdiag = cvbbdiag;
		    }
		}

		if (skip || spdist < bbdiag) {
		    continue;
		}
	    }

	    // If we passed the above filter, find the closest edges
	    std::set<cdt_mesh::uedge_t> close_edges = omesh->interior_uedges_search(ov->bb);

	    double mindist = DBL_MAX; 
	    cdt_mesh::uedge_t closest_uedge;
	    std::set<cdt_mesh::uedge_t>::iterator c_it;
	    for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
		cdt_mesh::uedge_t ue = *c_it;
		double dline = omesh->fmesh->uedge_dist(ue, spnt);
		if (mindist > dline) {
		    closest_uedge = ue;
		    mindist = dline;
		}
	    }

	    if (closest_uedge.v[0] == -1) {
		std::cout << "uedge problem\n";
		continue;
	    }

	    revt_pt_t rpt;
	    rpt.spnt = spnt;
	    rpt.sn = sn;
	    rpt.ov = ov;
	    edge_sets[closest_uedge].push_back(rpt);
	}

	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;
	std::cout << s_cdt->name << " face " << omesh->fmesh->f_id << " has " << edge_sets.size() << " edge/point sets:\n";
	std::map<cdt_mesh::uedge_t, std::vector<revt_pt_t>>::iterator es_it;
	for (es_it = edge_sets.begin(); es_it != edge_sets.end(); es_it++) {
	    cdt_mesh::uedge_t ue = es_it->first;
	    std::cout << "Edge: " << ue.v[0] << "<->" << ue.v[1] << ": " << es_it->second.size() << " points\n";
	}

	rcnt += refine_edge_vert_sets(omesh, &edge_sets);
    }

    return rcnt;
}

#if 0
static int
ptri_process(cdt_mesh::cpolygon_t *polygon, omesh_t *om, std::set<long> &overts, size_t tind, std::set<cdt_mesh::uedge_t> *ne, std::set<cdt_mesh::uedge_t> *se)
{
    cdt_mesh::triangle_t t = om->fmesh->tris_vect[tind];

    bool e_shared[3];
    struct cdt_mesh::edge_t e[3];
    struct cdt_mesh::uedge_t ue[3];
    for (int i = 0; i < 3; i++) {
	e_shared[i] = false;
    }
    e[0].set(t.v[0], t.v[1]);
    e[1].set(t.v[1], t.v[2]);
    e[2].set(t.v[2], t.v[0]);
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);

    // Check the polygon edges against the triangle edges
    for (int i = 0; i < 3; i++) {
	std::set<cdt_mesh::cpolyedge_t *>::iterator pe_it;
	for (pe_it = polygon->poly.begin(); pe_it != polygon->poly.end(); pe_it++) {
	    cdt_mesh::cpolyedge_t *pe = *pe_it;
	    cdt_mesh::uedge_t pue(pe->v[0], pe->v[1]);
	    if (ue[i] == pue) {
		e_shared[i] = true;
		break;
	    }
	}
    }

    // Count categories and file edges in the appropriate output sets
    long shared_cnt = 0;
    for (int i = 0; i < 3; i++) {
	if (e_shared[i]) {
	    shared_cnt++;
	    se->insert(ue[i]);
	} else {
	    ne->insert(ue[i]);
	}
    }

    if (shared_cnt == 0) {
	std::cerr << "ovlp aligned resolve triangle growth error\n";
	return 0;
    }

    if (shared_cnt == 1) {
	// If we've got only one shared edge, there should be a vertex not currently
	// involved with the loop - verify that, and if it's true report it.
	long unshared_vert = polygon->unshared_vertex(t);

	if (overts.find(unshared_vert) != overts.end()) {
	    // Adds a vert we want
	    return 1;
	}
	return 0;
    }


    if (shared_cnt == 2) {
	// Two shared edges - all three verts are already in the polygon.  We want
	// this one
	return 1;
    }

    return 0;
}
#endif



class ovlp_grp {
    public:
	ovlp_grp(omesh_t *m1, omesh_t *m2) {
	    om1 = m1;
	    om2 = m2;
	}
	omesh_t *om1;
	omesh_t *om2;
	std::set<size_t> tris1;
	std::set<size_t> tris2;
	std::set<long> verts1;
	std::set<long> verts2;
	std::set<overt_t *> overts1;
	std::set<overt_t *> overts2;

	void add_tri(omesh_t *m, size_t tind) {
	    if (om1 == m) {
		tris1.insert(tind);
		cdt_mesh::triangle_t tri = om1->fmesh->tris_vect[tind];
		for (int i = 0; i < 3; i++) {
		    verts1.insert(tri.v[i]);
		    overt_t *ov = om1->overts[tri.v[i]];
		    if (!ov) {
			std::cout << "WARNING: - no overt for tri vertex??\n";
			continue;
		    }
		    overts1.insert(ov);
		}
		return;
	    }
	    if (om2 == m) {
		tris2.insert(tind);
		cdt_mesh::triangle_t tri = om2->fmesh->tris_vect[tind];
		for (int i = 0; i < 3; i++) {
		    verts2.insert(tri.v[i]);
		    overt_t *ov = om2->overts[tri.v[i]];
		    if (!ov) {
			std::cout << "WARNING: - no overt for tri vertex??\n";
			continue;
		    }
		    overts2.insert(ov);
		}
		return;
	    }
	}

	// Each point involved in this operation must have it's closest point
	// in the other mesh involved.  If the closest point in the other mesh
	// ISN'T the closest surface point, we need to introduce that
	// point in the other mesh.
	bool refinement_pnts(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts);

	// Confirm that all triangles in the group are still in the fmeshes - if
	// we processed a prior group that involved a triangle incorporated into
	// this group that is now gone, this grouping is invalid and can't be
	// processed
	bool validate();

    private:
	bool ovlp_vert_validate(int ind, std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts);
};

bool
ovlp_grp::ovlp_vert_validate(int ind, std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    omesh_t *other_m = (!ind) ? om2 : om1;
    std::set<overt_t *> &ov1 = (!ind) ? overts1 : overts2;
    std::set<overt_t *> &ov2 = (!ind) ? overts2 : overts1;
    std::set<long> &v2 = (!ind) ? verts2 : verts1;

    bool have_refine_pnts = false;
    std::set<overt_t *>::iterator ov_it;
    for (ov_it = ov1.begin(); ov_it != ov1.end(); ov_it++) {
	// Find any points whose matching closest surface point isn't a vertex
	// in the other mesh per the vtree.  That point is a refinement point.
	overt_t *ov = *ov_it;
	overt_t *nv = other_m->vert_closest(NULL, ov);
	ON_3dPoint target_point = ov->vpnt();
	double pdist = ov->bb.Diagonal().Length() * 10;
	ON_3dPoint s_p;
	ON_3dVector s_n;
	bool feval = closest_surf_pnt(s_p, s_n, *other_m->fmesh, &target_point, 2*pdist);
	if (!feval) {
	    std::cout << "Error - couldn't find closest point for unpaired vert\n";
	}
	if (s_p.DistanceTo(nv->vpnt()) > ON_ZERO_TOLERANCE) {
	    std::cout << "Need new vert paring(" << s_p.DistanceTo(nv->vpnt()) << ": " << target_point.x << "," << target_point.y << "," << target_point.z << "\n";
	    isect_process_edge_vert2(ov, other_m, s_p, edge_verts);
	    // Ensure the count for this vert is above the processing threshold
	    other_m->refinement_overts[ov].insert(-1);
	    other_m->refinement_overts[ov].insert(-2);
	    have_refine_pnts = true;
	}
	// Make sure both vert sets store all the required vertices
	ov2.insert(nv);
	v2.insert(nv->p_id);
    }

    return have_refine_pnts;
}

bool
ovlp_grp::refinement_pnts(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    size_t v1_prev_size = verts1.size();
    size_t v2_prev_size = verts2.size();
    bool r1 = ovlp_vert_validate(0, edge_verts);
    bool r2 = ovlp_vert_validate(1, edge_verts);
    while ((v1_prev_size != verts1.size()) || (v2_prev_size != verts2.size())) {
	v1_prev_size = verts1.size();
	v2_prev_size = verts2.size();
	r1 = ovlp_vert_validate(0, edge_verts);
	r2 = ovlp_vert_validate(1, edge_verts);
    }
    return (r1 || r2);
}

bool
ovlp_grp::validate()
{
    return false;
}

void
shared_cdts(
	std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap
	)
{
    bool have_refine_pnts = true;
    std::vector<ovlp_grp> bins;
    std::map<std::pair<omesh_t *, size_t>, size_t> bin_map;

    while (have_refine_pnts) {

	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> edge_verts;

	bins.clear();
	bin_map.clear();
	have_refine_pnts = false;

	std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
	for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	    omesh_t *omesh1 = cp_it->first;
	    omesh_t *omesh2 = cp_it->second;
	    if (!omesh1->intruding_tris.size() || !omesh2->intruding_tris.size()) {
		continue;
	    }
	    //std::cout << "Need to check " << omesh1->fmesh->name << " " << omesh1->fmesh->f_id << " vs " << omesh2->fmesh->name << " " << omesh2->fmesh->f_id << "\n";
	    std::set<size_t> om1_tris = omesh1->intruding_tris;
	    while (om1_tris.size()) {
		size_t t1 = *om1_tris.begin();
		std::pair<omesh_t *, size_t> ckey(omesh1, t1);
		om1_tris.erase(t1);
		//std::cout << "Processing triangle " << t1 << "\n";
		ON_BoundingBox tri_bb = omesh1->fmesh->tri_bbox(t1);
		std::set<size_t> ntris = omesh2->tris_search(tri_bb);
		std::set<size_t>::iterator nt_it;
		for (nt_it = ntris.begin(); nt_it != ntris.end(); nt_it++) {
		    int real_ovlp = tri_isect(false, omesh1, omesh1->fmesh->tris_vect[t1], omesh2, omesh2->fmesh->tris_vect[*nt_it], NULL);
		    if (!real_ovlp) continue;
		    //std::cout << "real overlap with " << *nt_it << "\n";
		    std::pair<omesh_t *, size_t> nkey(omesh2, *nt_it);
		    size_t key_id;
		    if (bin_map.find(ckey) == bin_map.end() && bin_map.find(nkey) == bin_map.end()) {
			// New group
			ovlp_grp ngrp(ckey.first, nkey.first);
			ngrp.add_tri(ckey.first, ckey.second);
			ngrp.add_tri(nkey.first, nkey.second);
			bins.push_back(ngrp);
			key_id = bins.size() - 1;
			bin_map[ckey] = key_id;
			bin_map[nkey] = key_id;
		    } else {
			if (bin_map.find(ckey) != bin_map.end()) {
			    key_id = bin_map[ckey];
			    bins[key_id].add_tri(nkey.first, nkey.second);
			    bin_map[nkey] = key_id;
			} else {
			    key_id = bin_map[nkey];
			    bins[key_id].add_tri(ckey.first, ckey.second);
			    bin_map[ckey] = key_id;
			}
		    }
		}
	    }
	}

	if (!bins.size()) {
	    // If we didn't find anything, we're done
	    return;
	}

	// Have groupings - reset refinement info
	for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	    omesh_t *omesh1 = cp_it->first;
	    omesh_t *omesh2 = cp_it->second;
	    omesh1->refinement_clear();
	    omesh2->refinement_clear();
	}

	for (size_t i = 0; i < bins.size(); i++) {
	    if (bins[i].refinement_pnts(&edge_verts)) {
		have_refine_pnts = true;
	    }

	    if (!have_refine_pnts) {
		std::set<size_t>::iterator t_it;
		std::cout << "Group " << i << " vert cnts:\n";
		std::cout << "      " << bins[i].om1->fmesh->name << " " << bins[i].om1->fmesh->f_id << ": " << bins[i].overts1.size() << " verts\n";
		for (t_it = bins[i].tris1.begin(); t_it != bins[i].tris1.end(); t_it++) {
		    std::cout << "      " << bins[i].om1->fmesh->name << " " << bins[i].om1->fmesh->f_id << " triangle " << *t_it << "\n";
		}
		std::cout << "      " << bins[i].om2->fmesh->name << " " << bins[i].om2->fmesh->f_id << ": " << bins[i].overts2.size() << " verts\n";
		for (t_it = bins[i].tris2.begin(); t_it != bins[i].tris2.end(); t_it++) {
		    std::cout << "      " << bins[i].om2->fmesh->name << " " << bins[i].om2->fmesh->f_id << " triangle " << *t_it << "\n";
		}
	    }
	}

	// If refinement points were added above, refine and repeat
	if (have_refine_pnts) {
	    int bedge_replaced_tris = INT_MAX;
	    while (bedge_replaced_tris) {
		// Process edge_verts
		bedge_replaced_tris = bedge_split_near_verts(edge_verts, f2omap);
	    }
	    int interior_replaced_tris = INT_MAX;
	    while (interior_replaced_tris) {
		interior_replaced_tris = omesh_interior_edge_verts(check_pairs);
	    }
	    // Restore "normal" overlap information for next pass
	    face_omesh_ovlps(check_pairs, edge_verts, f2omap);
	}
    }

    for (size_t bin_id = 0; bin_id < bins.size(); bin_id++) {
	// Find the best fit plane of all 3D points from all the vertices in play from both
	// meshes.  This will be our polygon plane.
	std::set<ON_3dPoint> plane_pnts;
	std::set<long>::iterator vp_it;
	for (vp_it = bins[bin_id].verts1.begin(); vp_it != bins[bin_id].verts1.end(); vp_it++) {
	    ON_3dPoint p = *bins[bin_id].om1->fmesh->pnts[*vp_it];
	    plane_pnts.insert(p);
	}
	for (vp_it = bins[bin_id].verts2.begin(); vp_it != bins[bin_id].verts2.end(); vp_it++) {
	    ON_3dPoint p = *bins[bin_id].om2->fmesh->pnts[*vp_it];
	    plane_pnts.insert(p);
	}

	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane_pnts.begin(); p_it != plane_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");

	ON_Plane fit_plane(pcenter, pnorm);


#if 0
	// With a seed triangle from each mesh, grow two polygons such that all active
	// vertices from each mesh are either boundary or interior points on the polygons.
	// Normally, shouldn't grow beyond the active triangle set in the mesh - there may
	// however be cases were we need to for a valid projection (next step)
	//
	// TODO - the below logic is the right idea, but om1_verts is not the
	// vertex list to use - that's just from the overlapping triangles.  We
	// need the vert_bin set that includes all of the vertices active due
	// to the other mesh's triangles as well.
	std::set<long>::iterator tv_it;

	cdt_mesh::cpolygon_t *polygon = new cdt_mesh::cpolygon_t;
	polygon->pdir = fit_plane.Normal();
	polygon->tplane = fit_plane;
	for (tv_it = om1_verts.begin(); tv_it != om1_verts.end(); tv_it++) {
	    double u, v;
	    long pind = *tv_it;
	    ON_3dPoint *p = om[0]->fmesh->pnts[pind];
	    fit_plane.ClosestPointTo(*p, &u, &v);
	    std::pair<double, double> proj_2d;
	    proj_2d.first = u;
	    proj_2d.second = v;
	    polygon->pnts_2d.push_back(proj_2d);
	    polygon->p2o[polygon->pnts_2d.size() - 1] = pind;
	}
	// Seed the polygon with one of the triangles.
	cdt_mesh::triangle_t tri1 = om[0]->fmesh->tris_vect[*(om1_tris.begin())];
	cdt_mesh::edge_t e1(tri1.v[0], tri1.v[1]);
	cdt_mesh::edge_t e2(tri1.v[1], tri1.v[2]);
	cdt_mesh::edge_t e3(tri1.v[2], tri1.v[0]);
	polygon->add_edge(e1);
	polygon->add_edge(e2);
	polygon->add_edge(e3);

	// Now, the hard part.  We need to grow the polygon to encompass the om1_vert vertices.  This
	// may involve more triangles than om1_tris, but shouldn't involve vertices that aren't in the
	// om1_vert set.
	std::set<long> unused_verts = om1_verts;
	std::set<size_t> visited_tris, added_tris;
	visited_tris.insert(tri1.ind);
	added_tris.insert(tri1.ind);
	unused_verts.erase(tri1.v[0]);
	unused_verts.erase(tri1.v[1]);
	unused_verts.erase(tri1.v[2]);

	while (unused_verts.size()) {
	    std::set<cdt_mesh::cpolyedge_t *>::iterator pe_it;
	    for (pe_it = polygon->poly.begin(); pe_it != polygon->poly.end(); pe_it++) {
		cdt_mesh::cpolyedge_t *pe = *pe_it;
		cdt_mesh::uedge_t ue(pe->v[0], pe->v[1]);
		std::set<size_t> petris = om[0]->fmesh->uedges2tris[ue];
		std::set<size_t>::iterator t_it;
		for (t_it = petris.begin(); t_it != petris.end(); t_it++) {
		    if (visited_tris.find(*t_it) != visited_tris.end()) continue;
		    visited_tris.insert(*t_it);
		    // If either all three points are in the active set or the
		    // point not on the shared edge is in the set, the triangle
		    // should be considered.
		    std::set<cdt_mesh::uedge_t> new_edges;
		    std::set<cdt_mesh::uedge_t> shared_edges;
		    if (ptri_process(polygon, om[0], om1_verts, *t_it, &new_edges, &shared_edges)) {
			added_tris.insert(*t_it);
			cdt_mesh::triangle_t ntri = om[0]->fmesh->tris_vect[*t_it];
			unused_verts.erase(ntri.v[0]);
			unused_verts.erase(ntri.v[1]);
			unused_verts.erase(ntri.v[2]);
			polygon->replace_edges(new_edges, shared_edges);
		    }
		}
	    }
	}
#endif

	// TODO - If any points are not contained or too close to an edge but not on
	// it, we need to grow *both* meshes' polygons out from the edge nearest to the
	// problematic point.

	// CDT the polygon in the shared plane.  Those triangles will now map back to matching
	// closest points in both meshes.  Remove the original triangles from each mesh, and
	// replace with the new in both meshes.
    }
}


static void
make_omeshes(
	std::set<std::pair<omesh_t *, omesh_t *>> &ocheck_pairs,
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> &f2omap,
	std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs)
{
    // Make omesh containers for all the cdt_meshes in play
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>>::iterator p_it;
    std::set<cdt_mesh::cdt_mesh_t *> afmeshes;
    std::vector<omesh_t *> omeshes;
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	afmeshes.insert(p_it->first);
	afmeshes.insert(p_it->second);
    }
    std::set<cdt_mesh::cdt_mesh_t *>::iterator af_it;
    for (af_it = afmeshes.begin(); af_it != afmeshes.end(); af_it++) {
	cdt_mesh::cdt_mesh_t *fmesh = *af_it;
	omeshes.push_back(new omesh_t(fmesh));
	f2omap[fmesh] = omeshes[omeshes.size() - 1];
    }
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	omesh_t *o1 = f2omap[p_it->first];
	omesh_t *o2 = f2omap[p_it->second];
	ocheck_pairs.insert(std::make_pair(o1, o2));
    }
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
    std::set<std::pair<cdt_mesh::cdt_mesh_t *, cdt_mesh::cdt_mesh_t *>> check_pairs;
    check_pairs = possibly_interfering_face_pairs(s_a, s_cnt);

    //std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
    if (!check_pairs.size()) return 0;

    // Sanity check - are we valid?
    check_faces_validity(check_pairs);
    int face_ov_cnt = 1;
    int iterations = 0;
    while (face_ov_cnt) {

	iterations++;
	if (iterations > 4) break;

	std::set<std::pair<omesh_t *, omesh_t *>> ocheck_pairs;
	std::map<cdt_mesh::cdt_mesh_t *, omesh_t *> f2omap;
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> edge_verts;

	// TODO - we shouldn't really have to remake these every time - just
	// doing it now as a quick and dirty way of skipping the bookeeping
	// cleanup/resetting work that will need...
	make_omeshes(ocheck_pairs, f2omap, check_pairs);

	// Report our starting overlap count
	face_ov_cnt = face_omesh_ovlps(ocheck_pairs, edge_verts, f2omap);
	std::cout << "Initial overlap cnt: " << face_ov_cnt << "\n";
	if (!face_ov_cnt) return 0;

	int bedge_replaced_tris = INT_MAX;

	while (bedge_replaced_tris) {
	    int avcnt = 0;
	    // The simplest operation is to find vertices close to each other
	    // with enough freedom of movement (per triangle edge length) that
	    // we can shift the two close neighbors to surface points that are
	    // both the respective closest points to a center point between the
	    // two originals.
	    avcnt = adjust_close_verts(ocheck_pairs);
	    if (avcnt) {
		std::cout << "Adjusted " << avcnt << " vertices\n";
	    }

	    // Process edge_verts
	    bedge_replaced_tris = bedge_split_near_verts(edge_verts, f2omap);

	    face_ov_cnt = face_omesh_ovlps(ocheck_pairs, edge_verts, f2omap);
	}

	// Once edge splits are handled, use remaining closest points and find
	// nearest interior edge curve, building sets of points near each
	// interior edge.  Then, for all interior edges, yank the two triangles
	// associated with that edge, build a polygon with interior points and
	// tessellate.  I think we probably need this to introduce no changes
	// to the meshes as a condition of termination...
	int interior_replaced_tris = INT_MAX;
	while (interior_replaced_tris) {
	    interior_replaced_tris = omesh_interior_edge_verts(ocheck_pairs);
	    face_ov_cnt = face_omesh_ovlps(ocheck_pairs, edge_verts, f2omap);
	}


	// TODO - Instead of splitting to refine these last cases, we need to
	// build a set of vertex points common to both overlapping triangle
	// pairs, construct in each mesh the bounding polygon that has all of
	// the corresponding closest verts in both meshes in the polygon,
	// project that polygon to a COMMON plane best fitting all the points,
	// retriangulate in that plane, then construct the corresponding
	// triangles in both meshes from the shared triangulation.  Splitting
	// won't necessarily clear these cases - it may create more like them
	// on a smaller scale - so instead the idea is to construct a shared
	// triangulation where both meshes agree which triangles get which
	// vertices

	shared_cdts(ocheck_pairs, f2omap);


	check_faces_validity(check_pairs);
	face_ov_cnt = face_omesh_ovlps(ocheck_pairs, edge_verts, f2omap);
	std::cout << "Iteration " << iterations << " post tri split overlap cnt: " << face_ov_cnt << "\n";
    }

    return 0;
}


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

