/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graphio.hpp
 *
 */

#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "edgelist.hpp"
#include "adjlist.hpp"
#include "mmio.hpp"
#include "sequence.hpp"
#include "cmdline.hpp"

#ifndef _PASL_GRAPH_IO_H_
#define _PASL_GRAPH_IO_H_

/***********************************************************************/

namespace pasl {
namespace graph {
  
/*---------------------------------------------------------------------*/
/* Graphviz output */
  
template <class Edge_bag>
std::ostream& output_undirected_dot(std::ostream& out, const edgelist<Edge_bag>& edges) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  out << "graph {\n";
  for (edgeid_type i = 0; i < edges.get_nb_edges(); ++i) {
    out << edges.edges[i].src << " -- " << edges.edges[i].dst << ";\n";
  }
  return out << "}";
}

template<class Adjlist_seq>
std::ostream& output_undirected_dot(std::ostream& out, const adjlist<Adjlist_seq>& graph) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  out << "graph {\n";
  for (vtxid_type i = 0; i < graph.get_nb_vertices(); ++i) {
    for (vtxid_type j = 0; j < graph.adjlists[i].get_out_degree(); j++) {
      out << i << " -- " << graph.adjlists[i].get_out_neighbor(j) << ";\n";
    }
  }
  return out << "}";
}

template <class Edge_bag>
std::ostream& output_directed_dot(std::ostream& out, const edgelist<Edge_bag>& edges) {
  using vtxid_type = typename Edge_bag::value_type::vtxid_type;
  out << "digraph {\n";
  for (edgeid_type i = 0; i < edges.get_nb_edges(); ++i) {
    out << edges.edges[i].src << " -> " << edges.edges[i].dst << ";\n";
  }
  return out << "}";
}

template<class Adjlist_seq>
std::ostream& output_directed_dot(std::ostream& out, const adjlist<Adjlist_seq>& graph) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  out << "digraph {\n";
  for (vtxid_type i = 0; i < graph.get_nb_vertices(); ++i) {
    for (vtxid_type j = 0; j < graph.adjlists[i].get_out_degree(); j++) {
      out << i << " -> " << graph.adjlists[i].get_out_neighbor(j) << ";\n";
    }
  }
  return out << "}";
}
  
/*---------------------------------------------------------------------*/
/* Default graph printing routines */

template <class Edge_bag>
std::ostream& operator<<(std::ostream& out, const edgelist<Edge_bag>& edges) {
  return output_directed_dot(out, edges);
}

template<class Adjlist_seq>
std::ostream& operator<<(std::ostream& out, const adjlist<Adjlist_seq>& graph) {
  return output_directed_dot(out, graph);
}

template <class Graph>
void write_graph_to_dot(std::string fname, const Graph& graph) {
  std::ofstream ofs(fname.c_str(), std::ofstream::out);
  ofs << graph;
  ofs.close();
}

/*---------------------------------------------------------------------*/
/* Native file IO */

static constexpr uint64_t GRAPH_TYPE_ADJLIST = 0xdeadbeef;
static constexpr uint64_t GRAPH_TYPE_EDGELIST = 0xba5eba11;

static const int bits_per_byte = 8;
static const int graph_file_header_sz = 5;

template <class Vertex_id>
void read_adjlist_from_file(std::string fname, adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  std::ifstream in(fname, std::ifstream::binary);
  uint64_t graph_type;
  int nbbits;
  vtxid_type nb_vertices;
  edgeid_type nb_edges;
  bool is_symmetric;
  uint64_t header[5];
  in.read((char*)header, sizeof(header));
  graph_type = header[0];
  nbbits = int(header[1]);
  nb_vertices = vtxid_type(header[2]);
  nb_edges = edgeid_type(header[3]);
  is_symmetric = bool(header[4]);
  if (graph_type != GRAPH_TYPE_ADJLIST)
    util::atomic::die("read_adjlist_from_file"); //! \todo print the values expected and the value read
  if (sizeof(vtxid_type) * 8 < nbbits)
    util::atomic::die("read_adjlist_from_file: incompatible graph file");
  edgeid_type contents_szb;
  char* bytes;
  in.seekg (0, in.end);
  contents_szb = edgeid_type(in.tellg()) - sizeof(header);
  in.seekg (sizeof(header), in.beg);
  bytes = data::mynew_array<char>(contents_szb);
  if (bytes == NULL)
    util::atomic::die("failed to allocate space for graph");
  in.read (bytes, contents_szb);
  in.close();
  vtxid_type nb_offsets = nb_vertices + 1;
  if (contents_szb != sizeof(vtxid_type) * (nb_offsets + nb_edges))
    util::atomic::die("bogus file");
  graph.adjlists.init(bytes, nb_vertices, nb_edges);
  graph.nb_edges = nb_edges;
}

template <class Adjlist_seq>
void read_adjlist_from_file(std::string fname, adjlist<Adjlist_seq>& graph) {
  util::atomic::die("todo");
}

template <class Vertex_id>
void write_adjlist_to_file(std::string fname, const adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  std::ofstream out(fname, std::ofstream::binary);
  const int bytes_per_vtxid = sizeof(vtxid_type);
  int nbbits = bytes_per_vtxid * bits_per_byte;
  vtxid_type nb_vertices = graph.get_nb_vertices();
  edgeid_type nb_edges = graph.nb_edges;
  bool is_symmetric = false;
  uint64_t header[5];
  header[0] = uint64_t(GRAPH_TYPE_ADJLIST);
  header[1] = uint64_t(nbbits);
  header[2] = uint64_t(nb_vertices);
  header[3] = uint64_t(nb_edges);
  header[4] = uint64_t(is_symmetric);
  out.write((char*)header, sizeof(header));
  char* bytes = (char*)graph.adjlists.offsets;
  vtxid_type nb_offsets = nb_vertices + 1;
  edgeid_type contents_szb = sizeof(vtxid_type) * (nb_offsets + nb_edges);
  out.write(bytes, contents_szb);
  out.close();
}
  
template <class Adjlist>
void write_adjlist_to_dotfile(std::string fname, const Adjlist& graph) {
  std::ofstream out(fname);
  out << graph;
  out.close();
}

/*---------------------------------------------------------------------*/
/* Foreign file IO */

inline bool is_space(char c) {
  switch (c)  {
    case '\r':
    case '\t':
    case '\n':
    case 0:
    case ' ' : return true;
    default : return false;
  }
}

template <class Is_delimiter, class Container>
void tokenize_string(const Is_delimiter& is_delim,
                     char* src, size_t sz,
                     Container& dst) {
  data::pcontainer::combine(size_t(0), sz, dst, [&] (size_t i, Container& dst) {
    assert(i < sz);
    if (is_delim(src[i]))
      src[i] = '\0';
    else if ( (i == 0) || is_delim(src[i - 1]))
      dst.push_back(&src[i]);
  });
}
  
/* todo: atol is bogus in general because there is no guarantee that
 * long offers 64 bits. we need to either use scanf if it is efficient
 * or detect and handle problematic cases.
 *
 
 correct but slow method:
 
 vtxid_type v_src
 std::istringstream(words_array[i*2    ]) >> v_src;
 
 */
template <class Vertex_id>
void str_to_vtxidtype(const char* str, Vertex_id& id) {
  long n = atol(str);
  id = Vertex_id(n);
  assert(n == long(id));
}

template <class Vertex_id>
void str_to_vtxidtype(const std::string& str, Vertex_id& id) {
  str_to_vtxidtype(str.c_str(), id);
}

template <class Edge_bag>
void read_matrix_market(std::string fname, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  int nb_vertices;
  int nb_edges;
  int* I;
  int* J;
  double* val;
  mm_read_unsymmetric_sparse(fname.c_str(),
                             &nb_vertices, &nb_vertices, &nb_edges,
                             &val, &I, &J);
  dst.edges.alloc(edgeid_type(nb_edges));
  for (edgeid_type i = 0; i < nb_edges; i++)
    dst.edges[i] = edge_type(I[i], J[i]);
  free(I);
  free(J);
  dst.nb_vertices = nb_vertices;
}
  
template <class Vertex_id>
void read_matrix_market(std::string fname, adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edges;
  read_matrix_market(fname, edges);
  adjlist_from_edgelist(edges, graph);
}
  
//static const int twitter_graph_nb_vertices = 61578414;

const bool should_be_verbose = true;

static inline
void msg(std::string s) {
  if (should_be_verbose)
    std::cout << s << std::endl;
}
  
template <class Edge_bag>
void read_twitter_graph(std::string fname, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  using size_type = typename data::array_seq<int>::size_type;
  msg("read twitter file");
  std::ifstream in(fname);
  in.seekg (0, in.end);
  long n = in.tellg();
  in.seekg (0, in.beg);
  char* bytes = data::mynew_array<char>(n+1);
  in.read (bytes,n);
  in.close();
  msg("parse file contents");
  data::pcontainer::stack<char*> words;
  tokenize_string([] (char c) { return is_space(c); }, bytes, vtxid_type(n), words);
  edgeid_type nb_edges = edgeid_type(words.size()) / 2;
  data::array_seq<char*> words_array;
  msg("copy bits to array");
  data::pcontainer::transfer_contents_to_array_seq(words, words_array);
  dst.edges.alloc(nb_edges);
  msg("write edges to edge array");
  sched::native::parallel_for(edgeid_type(0), nb_edges, [&] (size_type i) {
    vtxid_type v_src;
    vtxid_type v_dst;
    str_to_vtxidtype(words_array[i*2    ], v_src);
    str_to_vtxidtype(words_array[i*2 + 1], v_dst);
    dst.edges[i] = edge_type(v_src, v_dst);
  });
  data::myfree(bytes);
//  dst.nb_vertices = twitter_graph_nb_vertices;
  dst.nb_vertices = max_vtxid_of_edgelist(dst) + 1;
  //std::cout << dst.nb_vertices << std::endl;
  msg("make undirected");
  bool should_make_undirected = util::cmdline::parse_or_default_bool("should_make_undirected", true);
  if (should_make_undirected)
    make_edgelist_graph_undirected(dst);
}

template <class Vertex_id>
void read_twitter_graph(std::string fname, adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edges;
  read_twitter_graph(fname, edges);
  adjlist_from_edgelist(edges, graph);
}
  
template <class Edge_bag>
void compute_nb_vertices(edgelist<Edge_bag>& graph) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  auto max_in_edge = [&] (vtxid_type i) {
    edge_type e = graph.edges[i];
    return std::max(e.src, e.dst);
  };
  vtxid_type max_vtxid =
    pbbs::sequence::maxReduce((vtxid_type*)NULL, graph.get_nb_edges(), max_in_edge);
  graph.nb_vertices = max_vtxid + 1;
}

/* snap graph format
 * http://snap.stanford.edu/data/
 */
template <class Edge_bag>
void read_snap_graph(std::string fname, edgelist<Edge_bag>& dst) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  using size_type = typename data::array_seq<int>::size_type;
  std::ifstream in(fname);
  std::string metadata;
  const int nb_header_lines = 4;
  const int metadata_line_id = 2;
  for (int i = 0; i < nb_header_lines; i++) {
    std::string data;
    std::getline(in, data);
    if (i == metadata_line_id)
      metadata = data;
  }
  std::stringstream ss(metadata);
  std::string tmp;
  vtxid_type nb_vertices;
  edgeid_type nb_edges = 0;
  int i = 0;
  const int nb_metadata_items = 5;
  const int nb_vertices_idx = 2;
  const int nb_edges_idx = 4;
  while (getline(ss, tmp, ' ')) {
    if (i == nb_vertices_idx)
      str_to_vtxidtype(tmp, nb_vertices);
    else if (i == nb_edges_idx)
      str_to_vtxidtype(tmp, nb_edges);
    i++;
  }
  if (i != nb_metadata_items)
    util::atomic::die("bogus header");
  long beg = in.tellg();
  in.seekg (0, in.end);
  long n = in.tellg();
  in.seekg (beg, in.beg);
  char* bytes = data::mynew_array<char>(n+1);
  in.read (bytes,n);
  in.close();
  data::pcontainer::stack<char*> words;
  tokenize_string([] (char c) { return is_space(c); }, bytes, vtxid_type(n), words);
  edgeid_type nb_actual_edges = edgeid_type(words.size()) / 2;
  if (nb_actual_edges != nb_edges)
    util::atomic::die("inconsistent edge counts");
  data::array_seq<char*> words_array;
  data::pcontainer::transfer_contents_to_array_seq(words, words_array);
  dst.edges.alloc(nb_edges);
  sched::native::parallel_for(edgeid_type(0), nb_edges, [&] (size_type i) {
    vtxid_type v_src;
    vtxid_type v_dst;
    str_to_vtxidtype(words_array[i*2    ], v_src);
    str_to_vtxidtype(words_array[i*2 + 1], v_dst);
    dst.edges[i] = edge_type(v_src, v_dst);
  });
  data::myfree(bytes);
  in.close();
  compute_nb_vertices(dst);
}
  
template <class Vertex_id>
void read_snap_graph(std::string fname, adjlist<flat_adjlist_seq<Vertex_id>>& graph) {
  using vtxid_type = Vertex_id;
  using edge_type = edge<vtxid_type>;
  using edgelist_bag_type = data::array_seq<edge_type>;
  using edgelist_type = edgelist<edgelist_bag_type>;
  edgelist_type edges;
  read_snap_graph(fname, edges);
  adjlist_from_edgelist(edges, graph);
}
  
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_IO_H_ */
