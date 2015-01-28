/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graphfileshared.hpp
 *
 */



#include "graphgenerators.hpp"
#include "graphio.hpp"
#include "graphconversions.hpp"
#include "cmdline.hpp"

#ifndef _PASL_GRAPHFILESHARED_H_
#define _PASL_GRAPHFILESHARED_H_

/***********************************************************************/

namespace pasl {
namespace graph {
  
using thunk_type = std::function<void ()>;

template <class Adjlist>
void print_adjlist_summary(const Adjlist& graph) {
  std::cout << "nb_vertices\t" << graph.get_nb_vertices() << std::endl;
  std::cout << "nb_edges\t" << graph.nb_edges << std::endl;
}
  
template <class Number>
Number read_big_number_from_command_line(std::string name) {
  return Number(util::cmdline::parse_uint64(name));
}
  
extern bool should_disable_random_permutation_of_vertices;
  
template <class Adjlist>
void generate_graph(Adjlist& graph) {
  using vtxid_type = typename Adjlist::vtxid_type;
  using vtxid_type = typename Adjlist::vtxid_type;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edges;
  util::cmdline::argmap_dispatch c;
  c.add("complete", [&] {
    vtxid_type nb_vertices = read_big_number_from_command_line<vtxid_type>("nb_vertices");
    generate_complete_graph(nb_vertices, edges);
  });
  c.add("phased", [&] {
    vtxid_type nb_phases = read_big_number_from_command_line<vtxid_type>("nb_phases");
    vtxid_type nb_vertices_per_phase = read_big_number_from_command_line<vtxid_type>("nb_vertices_per_phase");
    vtxid_type nb_per_phase_at_max_arity = read_big_number_from_command_line<vtxid_type>("nb_per_phase_at_max_arity");
    vtxid_type arity_of_vertices_not_at_max_arity = read_big_number_from_command_line<vtxid_type>("arity_of_vertices_not_at_max_arity");
    generate_phased(nb_phases, nb_vertices_per_phase, nb_per_phase_at_max_arity, arity_of_vertices_not_at_max_arity, edges);
  });
  c.add("parallel_paths", [&] {
    vtxid_type nb_phases = read_big_number_from_command_line<vtxid_type>("nb_phases");
    vtxid_type nb_paths_per_phase = read_big_number_from_command_line<vtxid_type>("nb_paths_per_phase");
    vtxid_type nb_edges_per_path = read_big_number_from_command_line<vtxid_type>("nb_edges_per_path");
    generate_parallel_paths(nb_phases, nb_paths_per_phase, nb_edges_per_path, edges);
  });
  c.add("rmat",           [&] {
    vtxid_type tgt_nb_vertices = read_big_number_from_command_line<vtxid_type>("tgt_nb_vertices");
    vtxid_type nb_edges = read_big_number_from_command_line<vtxid_type>("nb_edges");
    vtxid_type seed = read_big_number_from_command_line<vtxid_type>("rmat_seed");
    double a = util::cmdline::parse_double("a");
    double b = util::cmdline::parse_double("b");
    double c = util::cmdline::parse_double("c");
    generate_rmat(tgt_nb_vertices, nb_edges, seed, a, b, c, edges);
  });
  c.add("random",         [&] {
    vtxid_type dim = read_big_number_from_command_line<vtxid_type>("dim");
      vtxid_type degree = read_big_number_from_command_line<vtxid_type>("degree");
    vtxid_type num_rows = read_big_number_from_command_line<vtxid_type>("num_rows");
    generate_randlocal(dim, degree, num_rows, edges);
  });
  c.add("grid_2d",    [&] {
    vtxid_type width = read_big_number_from_command_line<vtxid_type>("width");
    vtxid_type height = read_big_number_from_command_line<vtxid_type>("height");
    generate_grid2d(width, height, edges);
  });
  c.add("square_grid",    [&] {
    vtxid_type nb_on_side = read_big_number_from_command_line<vtxid_type>("nb_on_side");
    generate_square_grid(nb_on_side, edges);
  });
  c.add("cube_grid",      [&] {
    vtxid_type nb_on_side = read_big_number_from_command_line<vtxid_type>("nb_on_side");
    generate_cube_grid(nb_on_side, edges);
  });
  c.add("chain",          [&] {
    vtxid_type nb_edges = read_big_number_from_command_line<vtxid_type>("nb_edges");
    generate_chain(nb_edges, edges);
  });
  c.add("star",           [&] {
    vtxid_type nb_edges = read_big_number_from_command_line<vtxid_type>("nb_edges");
    generate_star(nb_edges, edges);
  });
  c.add("tree_binary",           [&] {
    vtxid_type branching_factor = read_big_number_from_command_line<vtxid_type>("branching_factor");
    vtxid_type height = read_big_number_from_command_line<vtxid_type>("height");
    generate_balanced_tree(branching_factor, height, edges);
  });
  c.add("tree_depth_2",           [&] {
    vtxid_type branching_factor = read_big_number_from_command_line<vtxid_type>("branching_factor");
    generate_tree_depth_2(branching_factor, edges);
  });
  c.add("tree_2",           [&] {
    vtxid_type branching_factor_1 = read_big_number_from_command_line<vtxid_type>("branching_factor_1");
    vtxid_type branching_factor_2 = read_big_number_from_command_line<vtxid_type>("branching_factor_2");
    vtxid_type nb_phases = read_big_number_from_command_line<vtxid_type>("nb_phases");
    generate_tree_2(branching_factor_1, branching_factor_2, nb_phases, edges);
  });
  c.add("circular_knext",           [&] {
    vtxid_type nb_vertices = read_big_number_from_command_line<vtxid_type>("nb_vertices");
    vtxid_type k = read_big_number_from_command_line<vtxid_type>("k");
    generate_circular_knext(nb_vertices, k, edges);
  });
  c.add("unbalanced_tree",         [&] {
    vtxid_type depth_of_trunk = read_big_number_from_command_line<vtxid_type>("depth_of_trunk");
    vtxid_type depth_of_branches = read_big_number_from_command_line<vtxid_type>("depth_of_branches");
    bool trunk_first = util::cmdline::parse_or_default_bool("trunk_first", true);
    generate_unbalanced_tree(depth_of_trunk, depth_of_branches, trunk_first, edges);
  });
  util::cmdline::dispatch_by_argmap(c, "generator");
  if (! should_disable_random_permutation_of_vertices)
    randomly_permute_vertex_ids(edges);
  adjlist_from_edgelist(edges, graph);
}

template <class Adjlist>
void generate_graph_by_nb_edges(Adjlist& graph) {
  using vtxid_type = typename Adjlist::vtxid_type;
  vtxid_type nb_edges_target = vtxid_type(util::cmdline::parse_or_default_uint64("nb_edges_target", 0, false));
  using vtxid_type = typename Adjlist::vtxid_type;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edges;
  util::cmdline::argmap_dispatch c;
  c.add("complete",       [&] { generate_complete_graph_by_nb_edges(nb_edges_target, edges); });
  c.add("phased",         [&] { generate_phased_by_nb_edges(nb_edges_target, edges); });
  c.add("parallel_paths", [&] { generate_parallel_paths_by_nb_edges(nb_edges_target, edges); });
  c.add("rmat",           [&] { generate_rmat_by_nb_edges(nb_edges_target, edges); });
  c.add("random",         [&] { generate_randlocal_by_nb_edges(nb_edges_target, edges); });
  c.add("square_grid",    [&] { generate_square_grid_by_nb_edges(nb_edges_target, edges); });
  c.add("cube_grid",      [&] { generate_cube_grid_by_nb_edges(nb_edges_target, edges); });
  c.add("chain",          [&] { generate_chain_by_nb_edges(nb_edges_target, edges); });
  c.add("star",           [&] { generate_star(nb_edges_target, edges); });
  c.add("tree",           [&] { generate_balanced_tree(nb_edges_target, edges); });
  c.add("circular_knext", [&] { generate_circular_knext_by_nb_edges(nb_edges_target, edges); });
  util::cmdline::dispatch_by_argmap(c, "generator");

  adjlist_from_edgelist(edges, graph);
}

static inline void parse_fname(std::string fname, std::string& base, std::string& extension) {
  if (fname == "")
    util::atomic::die("bogus filename");
  std::stringstream ss(fname);
  std::getline(ss, base, '.');
  std::getline(ss, extension);
}

template <class Adjlist>
void load_graph_from_file(Adjlist& graph) {
  std::string infile = util::cmdline::parse_or_default_string("infile", "");
  std::string base;
  std::string extension;
  parse_fname(infile, base, extension);
  if (extension == "adj_bin")
    read_adjlist_from_file(infile, graph);
  else if (extension == "snap")
    read_snap_graph(infile, graph);
  else if (extension == "twitter")
    read_twitter_graph(infile, graph);
  else if (extension == "mmarket")
    read_matrix_market(infile, graph);
  else
    util::atomic::die("unknown file format %s", extension.c_str());
}

template <class Adjlist>
void write_graph_to_file(Adjlist& graph) {
  std::string outfile = util::cmdline::parse_or_default_string("outfile", "");
  std::stringstream ss(outfile);
  std::string base;
  std::string extension;
  parse_fname(outfile, base, extension);
  if (extension == "adj_bin") {
    std::cout << "Writing file " << outfile << std::endl;
    write_adjlist_to_file(outfile, graph);
  } else if (extension == "dot") {
    write_adjlist_to_dotfile(outfile, graph);
  } else {
    util::atomic::die("unknown extension for outfile %s", extension.c_str());
  }
}

} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPHFILESHARED_H_ */
