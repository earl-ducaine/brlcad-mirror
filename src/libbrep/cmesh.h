// This evolved from the original trimesh halfedge data structure code:

// Author: Yotam Gingold <yotam (strudel) yotamgingold.com>
// License: Public Domain.  (I, Yotam Gingold, the author, release this code into the public domain.)
// GitHub: https://github.com/yig/halfedge
//
// It ended up rather different, as we are concerned with somewhat different
// mesh properties for CDT and the halfedge data structure didn't end up being
// a good fit, but as it's derived from that source the public domain license
// is maintained for the changes as well.

#ifndef __cmesh_h__
#define __cmesh_h__

#include "common.h"

#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "poly2tri/poly2tri.h"
#include "opennurbs.h"
#include "bu/color.h"
namespace cmesh
{


struct edge_t {
    long v[2];

    long& start() { return v[0]; }
    const long& start() const {	return v[0]; }
    long& end() { return v[1]; }
    const long& end() const { return v[1]; }

    edge_t(long i, long j)
    {
	v[0] = i;
	v[1] = j;
    }

    edge_t()
    {
	v[0] = v[1] = -1;
    }

    void set(long i, long j)
    {
	v[0] = i;
	v[1] = j;
    }

    bool operator<(edge_t other) const
    {
	bool c1 = (v[0] < other.v[0]);
	bool c1e = (v[0] == other.v[0]);
	bool c2 = (v[1] < other.v[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(edge_t other) const
    {
	bool c1 = (v[0] == other.v[0]);
	bool c2 = (v[1] == other.v[1]);
	return (c1 && c2);
    }
    bool operator!=(edge_t other) const
    {
	bool c1 = (v[0] != other.v[0]);
	bool c2 = (v[1] != other.v[1]);
	return (c1 || c2);
    }
};

struct uedge_t {
    long v[2];

    uedge_t()
    {
	v[0] = v[1] = -1;
    }

    uedge_t(struct edge_t e)
    {
	v[0] = (e.v[0] <= e.v[1]) ? e.v[0] : e.v[1];
	v[1] = (e.v[0] > e.v[1]) ? e.v[0] : e.v[1];
    }

    uedge_t(long i, long j)
    {
	v[0] = (i <= j) ? i : j;
	v[1] = (i > j) ? i : j;
    }

    void set(long i, long j)
    {
	v[0] = (i <= j) ? i : j;
	v[1] = (i > j) ? i : j;
    }

    bool operator<(uedge_t other) const
    {
	bool c1 = (v[0] < other.v[0]);
	bool c1e = (v[0] == other.v[0]);
	bool c2 = (v[1] < other.v[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(uedge_t other) const
    {
	bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]));
	bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]));
	return (c1 && c2);
    }
    bool operator!=(uedge_t other) const
    {
	bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]));
	bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]));
	return (c1 || c2);
    }
};

struct triangle_t {
    long v[3];

    long& i() { return v[0]; }
    const long& i() const { return v[0]; }
    long& j() {	return v[1]; }
    const long& j() const { return v[1]; }
    long& k() { return v[2]; }
    const long& k() const { return v[2]; }

    triangle_t(long i, long j, long k)
    {
	v[0] = i;
	v[1] = j;
	v[2] = k;
    }

    triangle_t()
    {
	v[0] = v[1] = v[2] = -1;
    }

    bool operator<(triangle_t other) const
    {
	std::vector<long> vca, voa;
	for (int i = 0; i < 3; i++) {
	    vca.push_back(v[i]);
	    voa.push_back(other.v[i]);
	}
	std::sort(vca.begin(), vca.end());
	std::sort(voa.begin(), voa.end());
	bool c1 = (vca[0] < voa[0]);
	bool c1e = (vca[0] == voa[0]);
	bool c2 = (vca[1] < voa[1]);
	bool c2e = (vca[1] == voa[1]);
	bool c3 = (vca[2] < voa[2]);
	return (c1 || (c1e && c2) || (c1e && c2e && c3));
    }
    bool operator==(triangle_t other) const
    {
	bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]) || (v[0] == other.v[2]));
	bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]) || (v[1] == other.v[2]));
	bool c3 = ((v[2] == other.v[0]) || (v[2] == other.v[1]) || (v[2] == other.v[2]));
	return (c1 && c2 && c3);
    }
    bool operator!=(triangle_t other) const
    {
	bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]) && (v[0] != other.v[2]));
	bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]) && (v[1] != other.v[2]));
	bool c3 = ((v[2] != other.v[0]) && (v[2] != other.v[1]) && (v[2] != other.v[2]));
	return (c1 || c2 || c3);
    }

};

class cmesh_t
{
public:

    int type;  /* 0 = 3D, 1 = 2D */
    std::vector<ON_2dPoint *> pnts_2d;
    std::map<ON_2dPoint *, long> p2d2ind;
    std::vector<ON_3dPoint *> pnts;
    std::map<ON_3dPoint *, long> p2ind;

    std::set<triangle_t> tris;

    std::map<long, std::set<edge_t>> v2edges;
    std::map<long, std::set<triangle_t>> v2tris;
    std::map<edge_t, triangle_t> edges2tris;
    std::map<uedge_t, std::set<triangle_t>> uedges2tris;

    void reset();

    void build_3d(std::set<p2t::Triangle *> *cdttri, std::map<p2t::Point *, ON_3dPoint *> *pointmap);
    void set_brep_data(
	    bool brev,
	    std::set<ON_3dPoint *> *e,
	    std::set<ON_3dPoint *> *s,
	    std::map<ON_3dPoint *, ON_3dPoint *> *n
	    );

    bool tri_add(triangle_t &atris, int check);
    void tri_remove(triangle_t &etris);

    std::set<long> interior_points();
    std::set<uedge_t> boundary_edges();
    std::vector<std::vector<long>> boundary_loops();
    std::vector<triangle_t> face_neighbors(const triangle_t &f);
    std::vector<triangle_t> vertex_face_neighbors(long vind);

    std::vector<triangle_t> interior_incorrect_normals();


    ON_3dVector tnorm(const triangle_t &t);

    // Plot3 generation routines for debugging
    void boundary_edges_plot(const char *filename);
    void boundary_loops_plot(const char *filename);
    void face_neighbors_plot(const triangle_t &f, const char *filename);
    void vertex_face_neighbors_plot(long vind, const char *filename);
    void interior_incorrect_normals_plot(const char *filename);
    void tris_set_plot(std::set<triangle_t> &tset, const char *filename);
    void tris_plot(const char *filename);
    void tri_plot(triangle_t &tri, const char *filename);

#if 0
    void build_2d_submesh(cmesh_t *mesh_3d, triangle_t &seed, double deg_tol);
#endif
private:
    // For situations where we need to process using Brep data
    std::set<ON_3dPoint *> *edge_pnts;
    std::set<ON_3dPoint *> *singularities;
    std::map<ON_3dPoint *, ON_3dPoint *> *normalmap;
    bool m_bRev;
    ON_3dVector bnorm(const triangle_t &t);

    std::set<uedge_t> current_bedges;
    std::set<uedge_t> problem_edges;
    std::map<long, std::set<edge_t>> edge_pnt_edges;

    // misc
    edge_t find_boundary_oriented_edge(uedge_t &ue);
    void plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b);
    void plot_uedge(struct uedge_t &ue, FILE* plot_file);
};

}

#endif /* __trimesh_h__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
