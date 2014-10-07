/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file adjlist.hpp
 * \brief Adjacency-list graph format
 *
 */

#ifndef _PASL_GRAPH_ADJLIST_H_
#define _PASL_GRAPH_ADJLIST_H_

#include "graph.hpp"

/***********************************************************************/

namespace pasl {
namespace graph {
  
/*---------------------------------------------------------------------*/
/* Symmetric vertex */
  
template <class Vertex_id_bag>
class symmetric_vertex {
public:
  
  typedef Vertex_id_bag vtxid_bag_type;
  typedef typename vtxid_bag_type::value_type vtxid_type;
  
  symmetric_vertex() { }
  
  symmetric_vertex(vtxid_bag_type neighbors)
  : neighbors(neighbors) { }
  
  vtxid_bag_type neighbors;
  
  vtxid_type get_in_neighbor(vtxid_type j) const {
    return neighbors[j];
  }
  
  vtxid_type get_out_neighbor(vtxid_type j) const {
    return neighbors[j];
  }
  
  vtxid_type* get_in_neighbors() const {
    return neighbors.data();
  }
  
  vtxid_type* get_out_neighbors() const {
    return neighbors.data();
  }
  
  void set_in_neighbor(vtxid_type j, vtxid_type nbr) {
    neighbors[j] = nbr;
  }
  
  void set_out_neighbor(vtxid_type j, vtxid_type nbr) {
    neighbors[j] = nbr;
  }
  
  vtxid_type get_in_degree() const {
    return vtxid_type(neighbors.size());
  }
  
  vtxid_type get_out_degree() const {
    return vtxid_type(neighbors.size());
  }
  
  void set_in_degree(vtxid_type j) {
    neighbors.alloc(j);
  }
  
  // todo: use neighbors.resize()
  void set_out_degree(vtxid_type j) {
    neighbors.alloc(j);
  }
  
  void swap_in_neighbors(vtxid_bag_type& other) {
    neighbors.swap(other);
  }
  
  void swap_out_neighbors(vtxid_bag_type& other) {
    neighbors.swap(other);
  }
  
  void check(vtxid_type nb_vertices) const {
#ifndef NDEBUG
    for (vtxid_type i = 0; i < neighbors.size(); i++)
      check_vertex(neighbors[i], nb_vertices);
#endif
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Asymmetric vertex */

template <class Vertex_id_bag>
class asymmetric_vertex {
public:
  
  typedef Vertex_id_bag vtxid_bag_type;
  typedef typename vtxid_bag_type::value_type vtxid_type;
  
  vtxid_bag_type in_neighbors;
  vtxid_bag_type out_neighbors;
  
  vtxid_type get_in_neighbor(vtxid_type j) const {
    return in_neighbors[j];
  }
  
  vtxid_type get_out_neighbor(vtxid_type j) const {
    return out_neighbors[j];
  }
  
  vtxid_type* get_in_neighbors() const {
    return in_neighbors.data();
  }
  
  vtxid_type* get_out_neighbors() const {
    return out_neighbors.data();
  }
  
  void set_in_neighbor(vtxid_type j, vtxid_type nbr) {
    in_neighbors[j] = nbr;
  }
  
  void set_out_neighbor(vtxid_type j, vtxid_type nbr) {
    out_neighbors[j] = nbr;
  }
  
  vtxid_type get_in_degree() const {
    return vtxid_type(in_neighbors.size());
  }
  
  vtxid_type get_out_degree() const {
    return vtxid_type(out_neighbors.size());
  }
  
  void set_in_degree(vtxid_type j) {
    in_neighbors.alloc(j);
  }
  
  void set_out_degree(vtxid_type j) {
    out_neighbors.alloc(j);
  }
  
  void swap_in_neighbors(vtxid_bag_type& other) {
    in_neighbors.swap(other);
  }
  
  void swap_out_neighbors(vtxid_bag_type& other) {
    out_neighbors.swap(other);
  }
  
  void check(vtxid_type nb_vertices) const {
    for (vtxid_type i = 0; i < in_neighbors.size(); i++)
      check_vertex(in_neighbors[i], nb_vertices);
    for (vtxid_type i = 0; i < out_neighbors.size(); i++)
      check_vertex(out_neighbors[i], nb_vertices);
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Adjacency-list format */

template <class Adjlist_seq>
class adjlist {
public:
  
  typedef Adjlist_seq adjlist_seq_type;
  typedef typename adjlist_seq_type::value_type vertex_type;
  typedef typename vertex_type::vtxid_bag_type::value_type vtxid_type;
  typedef typename adjlist_seq_type::alias_type adjlist_seq_alias_type;
  typedef adjlist<adjlist_seq_alias_type> alias_type;
  
  edgeid_type nb_edges;
  adjlist_seq_type adjlists;
  
  adjlist()
  : nb_edges(0) { }
  
  adjlist(edgeid_type nb_edges)
  : nb_edges(nb_edges) { }
  
  vtxid_type get_nb_vertices() const {
    return vtxid_type(adjlists.size());
  }
  
  void check() const {
#ifndef NDEBUG
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      adjlists[i].check(get_nb_vertices());
    size_t m = 0;
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      m += adjlists[i].get_in_degree();
    assert(m == nb_edges);
    m = 0;
    for (vtxid_type i = 0; i < adjlists.size(); i++)
      m += adjlists[i].get_out_degree();
    assert(m == nb_edges);
#endif
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Equality operators */

template <class Vertex_id_bag>
bool operator==(const symmetric_vertex<Vertex_id_bag>& v1,
                const symmetric_vertex<Vertex_id_bag>& v2) {
  using vtxid_type = typename symmetric_vertex<Vertex_id_bag>::vtxid_type;
  if (v1.get_out_degree() != v2.get_out_degree())
    return false;
  for (vtxid_type i = 0; i < v1.get_out_degree(); i++)
    if (v1.get_out_neighbor(i) != v2.get_out_neighbor(i))
      return false;
  return true;
}

template <class Vertex_id_bag>
bool operator!=(const symmetric_vertex<Vertex_id_bag>& v1,
                const symmetric_vertex<Vertex_id_bag>& v2) {
  return ! (v1 == v2);
}

template <class Adjlist_seq>
bool operator==(const adjlist<Adjlist_seq>& g1,
                const adjlist<Adjlist_seq>& g2) {
  using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
  if (g1.get_nb_vertices() != g2.get_nb_vertices())
    return false;
  if (g1.nb_edges != g2.nb_edges)
    return false;
  for (vtxid_type i = 0; i < g1.get_nb_vertices(); i++)
    if (g1.adjlists[i] != g2.adjlists[i])
      return false;
  return true;
}

template <class Adjlist_seq>
bool operator!=(const adjlist<Adjlist_seq>& g1,
                const adjlist<Adjlist_seq>& g2) {
  return ! (g1 == g2);
}

/*---------------------------------------------------------------------*/
/* Flat adjacency-list format */

template <class Vertex_id, bool Is_alias = false>
class flat_adjlist_seq {
public:
  
  typedef flat_adjlist_seq<Vertex_id> self_type;
  typedef Vertex_id vtxid_type;
  typedef size_t size_type;
  typedef data::pointer_seq<vtxid_type> vertex_seq_type;
  typedef symmetric_vertex<vertex_seq_type> value_type;
  typedef flat_adjlist_seq<vtxid_type, true> alias_type;
  
  char* underlying_array;
  vtxid_type* offsets;
  vtxid_type nb_offsets;
  vtxid_type* edges;
  
  flat_adjlist_seq()
  : underlying_array(NULL), offsets(NULL),
  nb_offsets(0), edges(NULL) { }
  
  flat_adjlist_seq(const flat_adjlist_seq& other) {
    if (Is_alias) {
      underlying_array = other.underlying_array;
      offsets = other.offsets;
      nb_offsets = other.nb_offsets;
      edges = other.edges;
    } else {
      util::atomic::die("todo");
    }
  }
  
  //! \todo instead of using Is_alias, pass either ptr_seq or array_seq as underlying_array
  ~flat_adjlist_seq() {
    if (! Is_alias)
      clear();
  }
  
  void get_alias(alias_type& alias) const {
    alias.underlying_array = NULL;
    alias.offsets = offsets;
    alias.nb_offsets = nb_offsets;
    alias.edges = edges;
  }
  
  alias_type get_alias() const {
    alias_type alias;
    alias.underlying_array = NULL;
    alias.offsets = offsets;
    alias.nb_offsets = nb_offsets;
    alias.edges = edges;
    return alias;
  }
  
  void clear() {
    if (underlying_array != NULL)
      data::myfree(underlying_array);
    offsets = NULL;
    edges = NULL;
  }
  
  vtxid_type degree(vtxid_type v) const {
    assert(v >= 0);
    assert(v < size());
    return offsets[v + 1] - offsets[v];
  }
  
  value_type operator[](vtxid_type ix) const {
    assert(ix >= 0);
    assert(ix < size());
    return value_type(vertex_seq_type(&edges[offsets[ix]], degree(ix)));
  }
  
  vtxid_type size() const {
    return nb_offsets - 1;
  }
  
  void swap(self_type& other) {
    std::swap(underlying_array, other.underlying_array);
    std::swap(offsets, other.offsets);
    std::swap(nb_offsets, other.nb_offsets);
    std::swap(edges, other.edges);
  }
  
  void alloc(size_type) {
    util::atomic::die("unsupported");
  }
  
  void init(char* bytes, vtxid_type nb_vertices, edgeid_type nb_edges) {
    nb_offsets = nb_vertices + 1;
    underlying_array = bytes;
    offsets = (vtxid_type*)bytes;
    edges = &offsets[nb_offsets];
  }
  
  value_type* data() {
    util::atomic::die("unsupported");
    return NULL;
  }
  
};

template <class Vertex_id, bool Is_alias = false>
using flat_adjlist = adjlist<flat_adjlist_seq<Vertex_id, Is_alias>>;

template <class Vertex_id>
using flat_adjlist_alias = flat_adjlist<Vertex_id, true>;
  
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_ADJLIST_H_ */
