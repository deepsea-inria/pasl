/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file graph.hpp
 * \brief Graph algorithms
 *
 */

#include <atomic>
#include <vector>
#include <algorithm>

#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "sparray.hpp"

#ifndef _MINICOURSE_GRAPH_H_
#define _MINICOURSE_GRAPH_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Edge-list representation of graphs */

using vtxid_type = value_type;

// convention: first component source vertex id, second target
using edge_type = std::pair<vtxid_type, vtxid_type>;
using edgelist = std::deque<edge_type>;

edge_type mk_edge(vtxid_type source, vtxid_type dest) {
  return std::make_pair(source, dest);
}

long get_nb_vertices_of_edgelist(const edgelist& edges) {
  long nb = 0;
  for (long i = 0; i < edges.size(); i++)
    nb = std::max((value_type)nb, std::max(edges[i].first, edges[i].second));
  return nb+1;
}

std::ostream& operator<<(std::ostream& out, const edge_type& edge) {
  out << edge.first << " -> " << edge.second << ";\n";
  return out;
}

std::ostream& output_directed_dot(std::ostream& out, const edgelist& edges) {
  out << "digraph {\n";
  for (long i = 0; i < edges.size(); ++i)
    out << edges[i];
  return out << "}";
  
}

std::ostream& operator<<(std::ostream& out, const edgelist& edges) {
  return output_directed_dot(out, edges);
}

/*---------------------------------------------------------------------*/
/* Adjacency-list representation of graphs */

using neighbor_list = const value_type*;

class adjlist {
private:
  
  class Deleter {
  public:
    void operator()(char* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<char[], Deleter> ptr;
  long nb_offsets = 0l;  // == nb_vertices+1
  long nb_edges = 0l;
  vtxid_type* start_offsets;
  vtxid_type* start_edgelists;
  
  void check(vtxid_type v) const {
    assert(v >= (vtxid_type)0);
    assert(v < nb_offsets-1);
  }
  
  long alloc() {
    long nb_cells_allocated = nb_offsets + nb_edges;;
    char* p = (char*)my_malloc<vtxid_type>(nb_cells_allocated);
    assert(nb_cells_allocated==0l || p != nullptr);
    ptr.reset(p);
    start_offsets = (vtxid_type*)p;
    start_edgelists = &start_offsets[nb_offsets];
    return nb_cells_allocated;
  }
  
  void from_edgelist(const edgelist& edges) {
    long nb_vertices = get_nb_vertices_of_edgelist(edges);
    nb_offsets = nb_vertices + 1;
    nb_edges = edges.size();
    alloc();
    sparray degrees = sparray(nb_vertices);
    for (vtxid_type i = 0; i < nb_vertices; i++)
      degrees[i] = 0;
    for (long i = 0; i < edges.size(); i++) {
      edge_type e = edges[i];
      degrees[e.first]++;
    }
    start_offsets[0] = 0;
    for (vtxid_type i = 1; i < nb_offsets; i++)
      start_offsets[i] = start_offsets[i - 1] + degrees[i - 1];
    for (vtxid_type i = 0; i < nb_vertices; i++)
      degrees[i] = 0;
    for (long i = 0; i < edges.size(); i++) {
      edge_type e = edges[i];
      vtxid_type cnt = degrees[e.first];
      start_edgelists[start_offsets[e.first] + cnt] = e.second;
      degrees[e.first]++;
    }
  }
  
  const uint64_t GRAPH_TYPE_ADJLIST = 0xdeadbeef;
  const uint64_t GRAPH_TYPE_EDGELIST = 0xba5eba11;
  
  const int bits_per_byte = 8;
  const int graph_file_header_sz = 5;
  
public:
  
  adjlist(long nb_vertices = 0, long nb_edges = 0)
  : nb_offsets(nb_vertices+1), nb_edges(nb_edges) {
    if (nb_vertices > 0)
      alloc();
  }
  
  adjlist(const edgelist& edges) {
    from_edgelist(edges);
  }
  
  adjlist(std::initializer_list<edge_type> edges_init) {
    edgelist edges;
    for (auto it = edges_init.begin(); it != edges_init.end(); it++)
      edges.push_back(*it);
    from_edgelist(edges);
  }
  
  // To disable copy semantics, we disable:
  adjlist(const adjlist& other);            //   1. copy constructor
  adjlist& operator=(const adjlist& other); //   2. assign-by-copy operator
  
  adjlist& operator=(adjlist&& other) {
    // todo: make sure that the following does not create a memory leak
    ptr = std::move(other.ptr);
    nb_offsets = std::move(other.nb_offsets);
    nb_edges = std::move(other.nb_edges);
    start_offsets = std::move(other.start_offsets);
    start_edgelists = std::move(other.start_edgelists);
    return *this;
  }
  
  long get_nb_vertices() const {
    return nb_offsets-1l;
  }
  
  long get_nb_edges() const {
    return nb_edges;
  }
  
  long get_out_degree_of(vtxid_type v) const {
    check(v);
    return (long)(start_offsets[v+1] - start_offsets[v]);
  }
  
  neighbor_list get_out_edges_of(vtxid_type v) const {
    check(v);
    return &start_edgelists[start_offsets[v]];
  }
  
  void load_from_file(std::string fname) {
    std::ifstream in(fname, std::ifstream::binary);
    uint64_t graph_type;
    int nbbits;
    long nb_vertices;
    bool is_symmetric;
    uint64_t header[graph_file_header_sz];
    in.read((char*)header, sizeof(header));
    graph_type = header[0];
    nbbits = int(header[1]);
    assert(nbbits == VALUE_NB_BITS);
    if (nbbits != VALUE_NB_BITS)
      pasl::util::atomic::fatal([&] { std::cerr << "Bogus graph file: given " <<
        nbbits << " but expected " << VALUE_NB_BITS << " bits" << std::endl; });
    nb_vertices = long(header[2]);
    nb_offsets = nb_vertices + 1;
    nb_edges = long(header[3]);
    long nb_cells_alloced = alloc();
    long nb_bytes_allocated = nb_cells_alloced * sizeof(vtxid_type);
    is_symmetric = bool(header[4]);
    if (graph_type != GRAPH_TYPE_ADJLIST)
      pasl::util::atomic::fatal([] { std::cerr << "Bogus graph type." << std::endl; });
    if (sizeof(vtxid_type) * 8 < nbbits)
      pasl::util::atomic::fatal([] { std::cerr << "Incompatible graph file." << std::endl; });
    in.seekg (0, in.end);
    long contents_szb = long(in.tellg()) - sizeof(header);
    assert(contents_szb == nb_bytes_allocated);
    in.seekg (sizeof(header), in.beg);
    in.read (ptr.get(), contents_szb);
    in.close();
  }
  
};

std::ostream& output_directed_dot(std::ostream& out, const adjlist& graph) {
  out << "digraph {\n";
  for (long i = 0; i < graph.get_nb_vertices(); ++i) {
    for (long j = 0; j < graph.get_out_degree_of(i); j++) {
      neighbor_list out_edges = graph.get_out_edges_of(i);
      out << i << " -> " << out_edges[j] << ";\n";
    }
  }
  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const adjlist& graph) {
  return output_directed_dot(out, graph);
}

/*---------------------------------------------------------------------*/
/* Sequential BFS */

sparray bfs_seq(const adjlist& graph, vtxid_type source) {
  long nb_vertices = graph.get_nb_vertices();
  bool* visited = my_malloc<bool>(nb_vertices);
  for (long i = 0; i < nb_vertices; i++)
    visited[i] = false;
  sparray frontiers[2];
  frontiers[0] = sparray(nb_vertices);
  frontiers[1] = sparray(nb_vertices);
  long frontier_sizes[2];
  frontier_sizes[0] = 0;
  frontier_sizes[1] = 0;
  vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
  vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
  visited[source] = true;
  frontiers[cur][frontier_sizes[cur]++] = source;
  while (frontier_sizes[cur] > 0) {
    long nb = frontier_sizes[cur];
    for (vtxid_type ix = 0; ix < nb; ix++) {
      vtxid_type vertex = frontiers[cur][ix];
      vtxid_type degree = graph.get_out_degree_of(vertex);
      neighbor_list out_edges = graph.get_out_edges_of(vertex);
      for (vtxid_type edge = 0; edge < degree; edge++) {
        vtxid_type other = out_edges[edge];
        if (visited[other])
          continue;
        visited[other] = true;
        if (graph.get_out_degree_of(other) > 0)
          frontiers[nxt][frontier_sizes[nxt]++] = other;
      }
    }
    frontier_sizes[cur] = 0;
    cur = 1 - cur;
    nxt = 1 - nxt;
  }
  sparray result = sparray(nb_vertices);
  for (long i = 0; i < nb_vertices; i++)
    result[i] = visited[i];
  free(visited);
  return result;
}

/*---------------------------------------------------------------------*/
/* Parallel BFS */

sparray edge_map(const adjlist& graph, std::atomic<bool>* visited, const sparray& in_frontier) {
  return copy(in_frontier);
}

loop_controller_type bfs_par_init_contr("bfs_init");

sparray bfs_par(const adjlist& graph, vtxid_type source) {
  long n = graph.get_nb_vertices();
  std::atomic<bool>* visited = my_malloc<std::atomic<bool>>(n);
  par::parallel_for(bfs_par_init_contr, 0l, n, [&] (long i) {
    visited[i].store(false);
  });
  visited[source].store(true);
  sparray cur_frontier = { source };
  while (cur_frontier.size() > 0)
    cur_frontier = edge_map(graph, visited, cur_frontier);
  sparray result = tabulate([&] (value_type i) { return visited[i].load(); }, n);
  free(visited);
  return result;
}

sparray bfs(const adjlist& graph, vtxid_type source) {
#ifdef SEQUENTIAL_BASELINE
  return bfs_seq(graph, source);
#else
  return bfs_par(graph, source);
#endif
}

/***********************************************************************/

#endif /*! _MINICOURSE_GRAPH_H_ */
