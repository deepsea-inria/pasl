/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graphgenerators.hpp
 *
 */

#include <math.h>
#include <fstream>

#include "graphio.hpp"
#include "rmat.hpp"

 #include "quickcheck.hh"

#ifndef _PASL_GRAPHGENERATORS_H_
#define _PASL_GRAPHGENERATORS_H_

/***********************************************************************/

namespace pasl {
namespace graph {

/* TODO: add this section

class point { // TODO: move this class elswhere, into a graphviz namespace
public:
  double x;
  double y;
  double point() { }
  double point(double x, double y) : x(x),y(y) { }
}; 

template <class Edge_bag>
void init_layout(bool make_layout, point*& layout, typename Edge_bag::value_type::vtxid_type nb_vertices) {
  if (!make_layout)
    return;
  layout = new point*[nb_vertices];
}

// Purpose is to lay out all the nodes into a square area of dimensions 1.0 by 1.0,
// using mathematical coordinates : x goes to the west, and y goes to the north;
// that is, the origin is at the bottom left of the square.

template <class Edge_bag>
void add_to_layout(bool make_layout, point*& layout, typename Edge_bag::value_type::vtxid_type id, double x, double y) {
  if
  (!make_layout)
    return;
  layout[id] = point(x,y); // TODO: do i need an operator= to be defined on points?
}

// TODO: replace operator<< by proper functions which may take as argument the layout positions,
// and whether to draw (or not) the node ids, and a way to control the diameter of the nodes.
*/

template <class Edge_bag>
void generate_grid2d(typename Edge_bag::value_type::vtxid_type width,
                     typename Edge_bag::value_type::vtxid_type height,
                     edgelist<Edge_bag>& dst
                     /* TODO: add this here: bool make_layout, point*& layout */) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  vtxid_type nb_vertices = width * height;
  // TODO: add this: init_layout(make_layout, layout, nb_vertices);
  edgeid_type nb_edges = 2 * nb_vertices;
  dst.edges.alloc(nb_edges);
  auto loc2d = [&](vtxid_type i, vtxid_type j) {
    return ((i + height) % height) * width + (j + width) % width;
  };
  sched::native::parallel_for(vtxid_type(0), height, [&] (vtxid_type i) {
    // LATER: (if possible) please make the inner loop parallel as well, useful to generate horizontal rectangles
    for (vtxid_type j = 0; j < width; j++) {
      vtxid_type vertex = loc2d(i, j);
      vtxid_type vertex_below = loc2d(i+1, j);
      vtxid_type vertex_right = loc2d(i, j+1);
      dst.edges[2*vertex] = edge_type(vertex, vertex_below);
      dst.edges[2*vertex+1] = edge_type(vertex, vertex_right);
      // TODO: add this: add_to_layout(make_layout, layout, nb_vertices, vertex, j / width, (height-i) / height);
    }
  });
  dst.nb_vertices = nb_vertices;
  dst.check();
}

template <class Edge_bag>
void generate_square_grid(typename Edge_bag::value_type::vtxid_type nb_on_side,
                          edgelist<Edge_bag>& dst) {
  generate_grid2d(nb_on_side, nb_on_side, dst);
}

template <class Edge_bag>
void generate_square_grid_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  vtxid_type nb_on_side = vtxid_type(sqrt(double(tgt_nb_edges) / 2.0));
  generate_square_grid(nb_on_side, dst);
}

template <class Edge_bag>
void generate_cube_grid(typename Edge_bag::value_type::vtxid_type nb_on_side,
                        edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  vtxid_type dn = nb_on_side;
  edgeid_type nn = dn*dn*dn;
  edgeid_type nb_edges = 3*nn;
  dst.edges.alloc(nb_edges);
  auto loc3d = [&](vtxid_type i1, vtxid_type i2, vtxid_type i3) { // todo: x,y,z
    return ((i1 + dn) % dn)*dn*dn + ((i2 + dn) % dn)*dn + (i3 + dn) % dn;
  };
  sched::native::parallel_for ((vtxid_type)0, dn, [&] (vtxid_type i) {
    for (vtxid_type j = 0; j < dn; j++)
      for (vtxid_type k = 0; k < dn; k++) {
        vtxid_type l = loc3d(i,j,k);
        dst.edges[3*l] =   edge<vtxid_type>(l,loc3d(i+1,j,k));
        dst.edges[3*l+1] = edge<vtxid_type>(l,loc3d(i,j+1,k));
        dst.edges[3*l+2] = edge<vtxid_type>(l,loc3d(i,j,k+1));
        // double dec = k / (nb_on_side + 1) / (2 * nb_on_side);
        // add_to_layout(..., l, i / (nb_on_side+1) + dec, (nb_on_side-1-j) / (nb_on_side+1) - dec);
      }
  });
  dst.nb_vertices = vtxid_type(nn);
  dst.check();
}

template <class Edge_bag>
void generate_cube_grid_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  vtxid_type nb_on_side = vtxid_type(pow(double(tgt_nb_edges / 3.0), 1.0/3.0));
  generate_cube_grid(nb_on_side, dst);
}

template <class Number>
Number triangular_root(Number x) {
  return (Number)(sqrt((double)(4 * x + 1)) + 1) / 2;
}

// stupid implementation for the sole purpose of testing
template <class Edge_bag>
void generate_complete_graph(typename Edge_bag::value_type::vtxid_type nb_vertices, edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using resizable_edge_bag = data::pcontainer::bag<edge_type>;
  resizable_edge_bag edges;
  data::pcontainer::combine(vtxid_type(0), nb_vertices, edges, [&] (vtxid_type u, resizable_edge_bag& edges) {
    data::pcontainer::combine(vtxid_type(0), nb_vertices, edges, [&] (vtxid_type v, resizable_edge_bag& edges) {
      if (u != v)
        edges.push_back(edge_type(u, v));
    });
  });
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.nb_vertices = nb_vertices;
}

template <class Edge_bag>
void generate_complete_graph_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  vtxid_type nb_vertices = vtxid_type(triangular_root(tgt_nb_edges));
  generate_complete_graph(nb_vertices, dst);
}

template <class Edge_bag>
void generate_phased(typename Edge_bag::value_type::vtxid_type nb_phases,
                     typename Edge_bag::value_type::vtxid_type nb_vertices_per_phase,
                     typename Edge_bag::value_type::vtxid_type nb_per_phase_at_max_arity,
                     typename Edge_bag::value_type::vtxid_type arity_of_vertices_not_at_max_arity,
                     edgelist<Edge_bag>& dst) {
  // Remark: code seems to crash with nb phases 1
  /* ./graphfile.opt2 -loop_cutoff 1000 -outfile _data/phased_1025_arity_1025_small.adj_bin -generator phased -generator_proc 40 -nb_phases 1 -nb_vertices_per_phase 1025 -nb_per_phase_at_max_arity 1025 -arity_of_vertices_not_at_max_arity 0 -bits 32 -source 0 -seed 1 -proc 40 */

  // nb_edges = (nb_phases-1) * (  nb_per_phase_at_max_arity * nb_vertices_per_phase
  //                             + (nb_vertices_per_phase - nb_per_phase_at_max_arity) * arity_of_vertices_not_at_max_arity)
  //             + nb_vertices_per_phase
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using resizable_edge_bag = data::pcontainer::bag<edge_type>;
  vtxid_type K = nb_vertices_per_phase;
      //DEPRECATED: 
      //vtxid_type nb_outedges_per_vertex = (vtxid_type)(frac_edges_per_vertex * (float)K);
      //std::cout << "phased_nb_outedges_per_vertex" << "\t" << nb_outedges_per_vertex << std::endl;
  resizable_edge_bag edges;
  vtxid_type n_source = 0;
  // add_to_layout(..., n_source, 0, 0.5);
  data::pcontainer::combine(vtxid_type(0), nb_phases-1, edges, [&] (vtxid_type r, resizable_edge_bag& edges) {
    data::pcontainer::combine(vtxid_type(0), K, edges, [&] (vtxid_type k, resizable_edge_bag& edges) {
      vtxid_type n1 = 1 + r * K + k;
      if (r == 0)
         edges.push_back(edge_type(n_source, n1));
      vtxid_type arity = (k < nb_per_phase_at_max_arity) ? K : arity_of_vertices_not_at_max_arity;
      data::pcontainer::combine(vtxid_type(0), arity, edges, [&] (vtxid_type e, resizable_edge_bag& edges) {
        vtxid_type n2 = 1 + (r + 1) * K + ((k + e) % K);
        edges.push_back(edge_type(n1, n2));
      });
        // add_to_layout(..., n1, (r+1) / (nb_phases + 1), (K-1-k) / K);
    });
  });
  dst.nb_vertices = 1 + nb_phases * nb_vertices_per_phase;
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.check();
}

static int phased_nb_per_group = 40;

template <class Edge_bag>
void generate_phased_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  edgeid_type n = tgt_nb_edges / phased_nb_per_group / phased_nb_per_group;
  vtxid_type nb_phases = std::max(vtxid_type(1), vtxid_type(n));
  // link have the nodes at a phase to all the nodes at the next phase, the others to a single node
  generate_phased(nb_phases, vtxid_type(phased_nb_per_group), vtxid_type(phased_nb_per_group) / 2, 1, dst);
}

template <class Edge_bag>
void generate_parallel_paths(typename Edge_bag::value_type::vtxid_type nb_phases,
                             typename Edge_bag::value_type::vtxid_type nb_paths_per_phase,
                             typename Edge_bag::value_type::vtxid_type nb_edges_per_path,
                             edgelist<Edge_bag>& dst) {
  // nb_edges = nb_phases * nb_paths_per_phase * nb_edges_per_path
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using resizable_edge_bag = data::pcontainer::bag<edge_type>;
  resizable_edge_bag edges;
  vtxid_type P = nb_paths_per_phase;
  vtxid_type L = nb_edges_per_path;
  data::pcontainer::combine(vtxid_type(0), nb_phases, edges, [&] (vtxid_type r, resizable_edge_bag& edges) {
    vtxid_type s = r * (P * L + 1);
    vtxid_type e = s + P * L + 1;
    // double dec = r / nb_phases 
    // add_to_layout(..., s, dec, 0.5);
    // if r == nb_phases-1
    //   add_to_layout(..., e, 1.0, 0.5);
    data::pcontainer::combine(vtxid_type(0), P, edges, [&] (vtxid_type k, resizable_edge_bag& edges) { // TODO: swap 'k' and 'p' everywhere in this function
      vtxid_type ps = s + 1 + k * L;
      vtxid_type pe = ps + L - 1;
      edges.push_back(edge_type(s, ps));
      data::pcontainer::combine(vtxid_type(0), L - 1, edges, [&] (vtxid_type p, resizable_edge_bag& edges) {  
        // add_to_layout(..., ps+p, dec + (p+1) / nb_phases / (L+1), (L-1-k) / P);
        edges.push_back(edge_type(ps + p, ps + p + 1));
      });
      edges.push_back(edge_type(pe, e));
      // add_to_layout(..., pe, dec + L / nb_phases / (L+1), (L-1-k) / P);
    });
  });
  dst.nb_vertices = nb_phases * nb_paths_per_phase * nb_edges_per_path + nb_phases + 1;
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.check();
}

static int parallel_paths_nb_phases = 3;
static int parallel_paths_nb_paths_per_phase = 10;
static int parallel_paths_nb_edges_per_path = 10;

template <class Edge_bag>
void generate_parallel_paths_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  edgeid_type n = tgt_nb_edges / parallel_paths_nb_phases / parallel_paths_nb_paths_per_phase;
  vtxid_type nb_edges_per_path = std::max(vtxid_type(1), vtxid_type(n));
  generate_parallel_paths(parallel_paths_nb_phases, parallel_paths_nb_paths_per_phase, nb_edges_per_path, dst);
}

template <class Edge_bag>
void generate_chain(edgeid_type nb_edges, edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using resizable_edge_bag = data::pcontainer::bag<edge_type>;
  dst.nb_vertices = vtxid_type(nb_edges) + 1;
  dst.edges.alloc(nb_edges);
  pbbs::native::parallel_for(edgeid_type(0), nb_edges, [&] (edgeid_type i) {
    dst.edges[i] = edge_type(i, i + 1);
  });
  dst.check();
}

template <class Edge_bag>
void generate_chain_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  edgeid_type nb_edges = std::max(edgeid_type(2), tgt_nb_edges);
  generate_chain(nb_edges, dst);
}
  
template <class Edge_bag>
void generate_circular_knext(edgeid_type nb_vertices, edgeid_type knext, edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using resizable_edge_bag = data::pcontainer::bag<edge_type>;
  resizable_edge_bag edges;
  data::pcontainer::combine(edgeid_type(0), nb_vertices, edges, [&] (vtxid_type i, resizable_edge_bag& edges) {
    // double alpha = 2. * PI / nb_vertices
    // add_to_layout(..., i, 0.5 + 0.5 * cos(alpha), 0.5 + 0.5 * sin(alpha));
    data::pcontainer::combine(edgeid_type(0), knext, edges, [&] (vtxid_type k, resizable_edge_bag& edges) {
      vtxid_type kn = (i + k + 1) % nb_vertices;
      edges.push_back(edge_type(i, kn));
    });
  });
  dst.nb_vertices = (vtxid_type)nb_vertices;
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.check();
}

template <class Edge_bag>
void generate_circular_knext_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  edgeid_type nb_edges = std::max(edgeid_type(2), tgt_nb_edges);
  generate_circular_knext(nb_edges, 1, dst);
}

template <class Edge_bag>
void generate_rmat(edgeid_type tgt_nb_vertices,
                   edgeid_type nb_edges,
                   typename Edge_bag::value_type::vtxid_type seed,
                   float a, float b, float c,
                   edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  vtxid_type nb_vertices = (1 << pbbs::utils::log2Up(tgt_nb_vertices));
  pbbs::rMat<edgelist_type> g(nb_vertices,seed,a,b,c);
  dst.edges.alloc(nb_edges);
  sched::native::parallel_for(edgeid_type(0), nb_edges, [&] (vtxid_type i) {
    dst.edges[i] = g(i);
  });
  dst.nb_vertices = nb_vertices;
  edgelist_type newdst;
  remove_duplicates(dst, newdst);
  dst.swap(newdst);
  dst.check();
}

template <class Edge_bag>
void generate_rmat_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  vtxid_type tgt_nb_vertices = vtxid_type(0.2 * double(tgt_nb_edges));
  generate_rmat(tgt_nb_vertices, tgt_nb_edges, 12334, 0.5, 0.1, 0.1, dst);
}

template <class Edge_bag>
void generate_randlocal(typename Edge_bag::value_type::vtxid_type dim,
                        typename Edge_bag::value_type::vtxid_type degree,
                        typename Edge_bag::value_type::vtxid_type num_rows,
                        edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using intT = vtxid_type;
  intT nonZeros = num_rows*degree;
  dst.edges.alloc(nonZeros);
  sched::native::parallel_for(intT(0), nonZeros, [&] (intT k) {
    intT i = k / degree;
    intT j;
    if (dim==0) {
      intT h = k;
      do {
        j = ((h = pbbs::dataGen::hash<intT>(h)) % num_rows);
      } while (j == i);
    } else {
      intT pow = dim+2;
      intT h = k;
      do {
        while ((((h = pbbs::dataGen::hash<intT>(h)) % 1000003) < 500001)) pow += dim;
        j = (i + ((h = pbbs::dataGen::hash<intT>(h)) % (((long) 1) << pow))) % num_rows;
      } while (j == i);
    }
    dst.edges[k].src = i;  dst.edges[k].dst = j;
  });
  dst.nb_vertices = num_rows;
  dst.check();
}

template <class Edge_bag>
void generate_randlocal_by_nb_edges(edgeid_type tgt_nb_edges, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  edgeid_type degree = 8;
  tgt_nb_edges = std::min(degree, tgt_nb_edges);
  generate_randlocal(10, degree, tgt_nb_edges, dst);
}
  
template <class Edge_bag>
void generate_star(edgeid_type nb_edges, edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  dst.edges.alloc(nb_edges);
  sched::native::parallel_for(edgeid_type(0), nb_edges, [&] (edgeid_type i) {
    dst.edges[i] = edge_type(0, vtxid_type(i + 1));
  });
  dst.nb_vertices = vtxid_type(nb_edges + 1);
  dst.check();
}
  
template <class Edge_bag>
void generate_balanced_tree(typename edgelist<Edge_bag>::vtxid_type branching_factor,
                            typename edgelist<Edge_bag>::vtxid_type height,
                            edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using vertex_bag_type = data::pcontainer::bag<vtxid_type>;
  using edge_container_type = data::pcontainer::bag<edge_type>;

  using size_type = typename edge_bag_type::size_type;
  
  vertex_bag_type prev;
  vertex_bag_type next;
  edge_container_type edges;
  
  prev.push_back(vtxid_type(0));
  vtxid_type fresh = 1;
  for (vtxid_type level = 0; level < height; level++) {
    prev.for_each([&] (vtxid_type v) {
      for (vtxid_type n = 0; n < branching_factor; n++) {
        vtxid_type child = fresh;
        fresh++;
        next.push_back(child);
        edges.push_back(edge_type(v, child));
      }
    });
    prev.clear();
    prev.swap(next);
  }

  vtxid_type nb_edges = vtxid_type(edges.size());
  vtxid_type nb_nodes = nb_edges + 2;
  dst.edges.alloc(nb_edges);
  data::pcontainer::transfer_contents_to_array(edges, dst.edges.data());
  dst.nb_vertices = nb_nodes;
  dst.check();
}
  
template <class Edge_bag>
void generate_tree_depth_2(typename edgelist<Edge_bag>::vtxid_type branching_factor,
                            edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using edge_container_type = data::pcontainer::stack<edge_type>;
  using size_type = typename edge_bag_type::size_type;

  edge_container_type edges;
  
  vtxid_type root = 0;
  // add_to_layout(..., root, 0.5, 1.0);
  data::pcontainer::combine(vtxid_type(0), branching_factor, edges, [&] (vtxid_type i, edge_container_type& edges) {
    edges.push_back(edge_type(root, i+1));
    // add_to_layout(..., i+1, 0.25 + 0.5 * (i/branching_factor), 0.5);
  });

  data::pcontainer::combine(vtxid_type(0), branching_factor, edges, [&] (vtxid_type i, edge_container_type& edges) {
    vtxid_type src = i + 1;
    data::pcontainer::combine(vtxid_type(0), branching_factor, edges, [&] (vtxid_type j, edge_container_type& edges) {
      vtxid_type dst = src * branching_factor + j + 1;
      // add_to_layout(..., dst, (src * branching_factor + j) / branching_factor / branching_factor, 0);
      edges.push_back(edge_type(src, dst));
    });
  });

  vtxid_type nb_edges = vtxid_type(edges.size());
  vtxid_type nb_nodes = nb_edges + 1;
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.nb_vertices = nb_nodes;
  dst.check();
}
  
template <class Edge_bag>
void generate_tree_2(typename edgelist<Edge_bag>::vtxid_type branching_factor_1,
                     typename edgelist<Edge_bag>::vtxid_type branching_factor_2,
                     typename edgelist<Edge_bag>::vtxid_type nb_phases,
                            edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using edge_container_type = data::pcontainer::stack<edge_type>;
  using size_type = typename edge_bag_type::size_type;

  edge_container_type edges;
  
  vtxid_type nb_per_phases = branching_factor_1 + branching_factor_1 * branching_factor_2;
  vtxid_type root;
  for (vtxid_type phase = 0; phase < nb_phases; phase++) {
    root = phase * nb_per_phases;
    // add_to_layout(
    data::pcontainer::combine(vtxid_type(0), branching_factor_1, edges, [&] (vtxid_type i, edge_container_type& edges) {
      edges.push_back(edge_type(root, root+i+1));
      // add_to_layout(...
    });
    data::pcontainer::combine(vtxid_type(0), branching_factor_1, edges, [&] (vtxid_type i, edge_container_type& edges) {
      vtxid_type src = root + i + 1;
      data::pcontainer::combine(vtxid_type(0), branching_factor_2, edges, [&] (vtxid_type j, edge_container_type& edges) {
        vtxid_type dst = root + branching_factor_1 + i * branching_factor_2 + j + 1;
        // add_to_layout(...,
        edges.push_back(edge_type(src, dst));
      });
    });
  }

  vtxid_type nb_edges = vtxid_type(edges.size());
  vtxid_type nb_nodes = 1 + nb_phases * nb_per_phases;
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.nb_vertices = nb_nodes;
  dst.check();
}
  
template <class Edge_bag>
void generate_balanced_tree(typename edgelist<Edge_bag>::vtxid_type nb_edges_tgt,
                            edgelist<Edge_bag>& dst) {
  typename edgelist<Edge_bag>::vtxid_type branching_factor = 2;
  typename edgelist<Edge_bag>::vtxid_type height = pbbs::utils::log2Up(nb_edges_tgt) - 1;
  generate_balanced_tree(branching_factor, height, dst);
}
  
template <class Edge_bag>
void generate_unbalanced_tree(typename edgelist<Edge_bag>::vtxid_type depth_of_trunk,
                              typename edgelist<Edge_bag>::vtxid_type depth_of_branches,
                              bool trunk_first,
                              edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  using edge_container_type = data::pcontainer::stack<edge_type>;
  using size_type = typename edge_bag_type::size_type;
  
  edge_container_type edges;
  
  vtxid_type nb_vertices = (depth_of_trunk-1) *(1+depth_of_branches) + 1;
  data::pcontainer::combine(vtxid_type(0), depth_of_trunk-1, edges, [&] (vtxid_type i, edge_container_type& edges) {
    vtxid_type vertex = i * (1+depth_of_branches);
    vtxid_type next_vertex = (i+1) * (1+depth_of_branches);
    if (trunk_first)
      edges.push_back(edge_type(vertex, next_vertex));
    data::pcontainer::combine(vtxid_type(0), depth_of_branches-1, edges, [&] (vtxid_type j, edge_container_type& edges) {
      edges.push_back(edge_type(vertex+j, vertex+j+1));
    });
    if (! trunk_first)
      edges.push_back(edge_type(vertex, next_vertex));
  });
  data::pcontainer::transfer_contents_to_array_seq(edges, dst.edges);
  dst.nb_vertices = nb_vertices;
  dst.check();
}

enum { BALANCED_TREE,
       COMPLETE, PHASED, PARALLEL_PATHS, RMAT, 
	 SQUARE_GRID, CUBE_GRID, CHAIN, STAR,
       //RANDOM,
  NB_GENERATORS };

class generator_type {
public:
  generator_type()
  : ty(BALANCED_TREE) { }
  
  unsigned int ty;
};

static inline void generate(generator_type& ty) {
  quickcheck::generate(NB_GENERATORS-1, ty.ty);
}

template <class Edge_bag>
void generate(edgeid_type& tgt_nb_edges,
              generator_type& which_generator,
              edgelist<Edge_bag>& graph) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = Edge_bag;
  using edgelist_type = edgelist<edgelist_bag_type>;
  switch (which_generator.ty) {
    case COMPLETE: {
      generate_complete_graph_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case PHASED: {
      generate_phased_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case PARALLEL_PATHS: {
      generate_parallel_paths_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case RMAT: {
      generate_rmat_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case SQUARE_GRID: {
      generate_square_grid_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case CUBE_GRID: {
      generate_cube_grid_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case CHAIN: {
      generate_chain_by_nb_edges(tgt_nb_edges, graph);
      break;
    }
    case STAR: {
      generate_star(tgt_nb_edges, graph);
      break;
    }
    case BALANCED_TREE: {
      generate_balanced_tree(tgt_nb_edges, graph);
      break;
    }
 /*
    case RANDOM: {
      generate_randlocal_by_nb_edges(tgt_nb_edges, graph);
      break;
    } */
    default: {
      util::atomic::die("unknown graph type %d",which_generator.ty);
    }
  }
#ifdef REMOVE_DUPLICATE_EDGES
  edgelist_type tmp;
  remove_duplicates(graph, tmp);
  tmp.swap(graph);
#endif
}

template <class Edge_bag>
void generate(size_t& _tgt_nb_edges, edgelist<Edge_bag>& graph) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = Edge_bag;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgeid_type tgt_nb_edges((vtxid_type)_tgt_nb_edges);
  static const edgeid_type max_nb_edges = 10000;
  tgt_nb_edges = std::min(tgt_nb_edges, max_nb_edges);
  size_t scale;
  quickcheck::generate(50, scale);
  tgt_nb_edges *= edgeid_type(scale);
  generator_type which_generator;
  generate(which_generator);
  generate(tgt_nb_edges, which_generator, graph);
}

template <class Adjlist_seq>
void generate(size_t& _tgt_nb_edges, adjlist<Adjlist_seq>& graph) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edg;
  generate(_tgt_nb_edges, edg);
  adjlist_from_edgelist(edg, graph);
}
    
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPHGENERATORS_H_ */
