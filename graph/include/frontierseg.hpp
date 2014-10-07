/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Data structure for representing a piece of the frontier of a
 * graph traversal
 * \file frontierseg.hpp
 *
 */

#ifndef _PASL_GRAPH_FRONTIERSEG_H_
#define _PASL_GRAPH_FRONTIERSEG_H_

#include "chunkedseq.hpp"

namespace pasl {
namespace graph {

/***********************************************************************/

namespace frontiersegbase {
  
template <
  class Graph,
  template <
    class Vertex,
    class Cache_policy
  >
  class Vertex_container
>
class frontiersegbase {
public:

  /*---------------------------------------------------------------------*/

  using self_type = frontiersegbase<Graph, Vertex_container>;
  using size_type = size_t;
  using graph_type = Graph;
  using vtxid_type = typename graph_type::vtxid_type;
  using const_vtxid_pointer = const vtxid_type*;
  
  class edgelist_type {
  public:
    
    const_vtxid_pointer lo;
    const_vtxid_pointer hi;
    
    edgelist_type()
    : lo(nullptr), hi(nullptr) { }
    
    edgelist_type(size_type nb, const_vtxid_pointer edges)
    : lo(edges), hi(edges + nb) { }
    
    size_type size() const {
      return size_type(hi - lo);
    }

    void clear() {
      hi = lo;
    }

    static edgelist_type take(edgelist_type edges, size_type nb) {
      assert(nb <= edges.size());
      assert(nb >= 0);
      edgelist_type edges2 = edges;
      edges2.hi = edges2.lo + nb;
      assert(edges2.size() == nb);
      return edges2;
    }
    
    static edgelist_type drop(edgelist_type edges, size_type nb) {
      assert(nb <= edges.size());
      assert(nb >= 0);
      edgelist_type edges2 = edges;
      edges2.lo = edges2.lo + nb;
      assert(edges2.size() + nb == edges.size());
      return edges2;
    }
    
    void swap(edgelist_type& other) {
      std::swap(lo, other.lo);
      std::swap(hi, other.hi);
    }
    
    template <class Body>
    void for_each(const Body& func) const {
      for (auto e = lo; e < hi; e++)
        func(*e);
    }
  };
  
private:
  
  /*---------------------------------------------------------------------*/
  
  static size_type out_degree_of_vertex(graph_type g, vtxid_type v) {
    return size_type(g.adjlists[v].get_out_degree());
  }
  
  static vtxid_type* neighbors_of_vertex(graph_type g, vtxid_type v) {
    return g.adjlists[v].get_out_neighbors();
  }
  
  edgelist_type create_edgelist(vtxid_type v) const {
    graph_type g = get_graph();
    size_type degree = out_degree_of_vertex(g, v);
    vtxid_type* neighbors = neighbors_of_vertex(g, v);
    return edgelist_type(vtxid_type(degree), neighbors);
  }
  
  /*---------------------------------------------------------------------*/

  class graph_env {
  public:
    
    graph_type g;
    
    graph_env() { }
    graph_env(graph_type g) : g(g) { }

    size_type operator()(vtxid_type v) const {
      return out_degree_of_vertex(g, v);
    }
    
  };
  
  using cache_type = data::cachedmeasure::weight<vtxid_type, vtxid_type, size_type, graph_env>;
  using seq_type = Vertex_container<vtxid_type, cache_type>;
//  using seq_type = data::chunkedseq::bootstrapped::stack<vtxid_type, chunk_capacity, cache_type>;
  
  /*---------------------------------------------------------------------*/
  
  // FORMIKE: suggesting rename to fr, mid, bk 
  edgelist_type f;
  seq_type m;
  edgelist_type b;
  
  /*---------------------------------------------------------------------*/

  void check() const {
#ifdef FULLDEBUG
    size_type nf = f.size();
    size_type nb = b.size();
    size_type nm = 0;
    graph_type g = get_graph();
    m.for_each([&] (vtxid_type v) {
      nm += out_degree_of_vertex(g, v);
    });
    size_type n = nf + nb + nm;
    size_type e = nb_outedges();
    size_type em = nb_outedges_of_middle();
    size_type szm = m.size();
    assert(n == e);
    assert((szm > 0) ? em > 0 : true);
    assert((em > 0) ? (szm > 0) : true);
#endif
  }
  
  size_type nb_outedges_of_middle() const {
    return m.get_cached();
  }
  
public:
  
  /*---------------------------------------------------------------------*/
  
  frontiersegbase() { }
  
  frontiersegbase(graph_type g) {
    set_graph(g);
  }
  
  /* We use the following invariant so that client code can use empty check
   * of the container interchangeably with a check on the number of outedges:
   *
   *     empty() iff nb_outedges() == 0
   */
  
  bool empty() const {
    return f.size() == 0
        && m.empty()
        && b.size() == 0;
  }

  size_type nb_outedges() const {
    size_type nb = 0;
    nb += f.size();
    nb += nb_outedges_of_middle();
    nb += b.size();
    return nb;
  }

  void push_vertex_back(vtxid_type v) {
    check();
    size_type d = out_degree_of_vertex(get_graph(), v);
    // FORMIKE: this optimization might actually be slowing down things
    // MIKE: this is not an optimization; it's necessary to maintain invariant 1 above
    if (d > 0)
      m.push_back(v);
    check();
  }
  
  /*
  void push_edgelist_back(edgelist_type edges) {
    assert(b.size() == 0);
    check();
    b = edges;
    check();
  } */
  
  edgelist_type pop_edgelist_back() {
    size_type nb_outedges1 = nb_outedges();
    assert(nb_outedges1 > 0);
    edgelist_type edges;
    check();
    if (b.size() > 0) {
      edges.swap(b);
    } else if (! m.empty()) {
      // FORMIKE: if we allow storing nodes with zero out edges,
      // then we may consider having a while loop here, to pop
      // from middle until we get a node with more than zero edges
      edges = create_edgelist(m.pop_back());
    } else {
      assert(f.size() > 0);
      edges.swap(f);
    }
    check();
    size_type nb_popped_edges = edges.size();
    assert(nb_popped_edges > 0);
    size_type nb_outedges2 = nb_outedges();
    assert(nb_outedges2 + nb_popped_edges == nb_outedges1);
    assert(b.size() == 0);
    return edges;
  }

  // pops the back edgelist containing at most `nb` edges
  /*
  edgelist_type pop_edgelist_back_at_most(size_type nb) {
    size_type nb_outedges1 = nb_outedges();
    edgelist_type edges = pop_edgelist_back();
    size_type nb_edges1 = edges.size();
    if (nb_edges1 > nb) {
      size_type nb_edges_to_keep = nb_edges1 - nb;
      // FORMIKE: could have a split(edges, nb_to_keep, edges_dst) method to combine take/drop
      edgelist_type edges_to_keep = edgelist_type::take(edges, nb_edges_to_keep);
      edges = edgelist_type::drop(edges, nb_edges_to_keep);
      push_edgelist_back(edges_to_keep);
    }
    size_type nb_edges2 = edges.size();
    assert(nb_edges2 <= nb);
    assert(nb_edges2 >= 0);
    size_type nb_outedges2 = nb_outedges();
    assert(nb_outedges2 + nb_edges2 == nb_outedges1);
    check();
    return edges;
  } */
  
  /* The container is erased after the first `nb` edges.
   *
   * The erased edges are moved to `other`.
   */
  void split(size_type nb, self_type& other) {
    check();// other.check();
    assert(other.nb_outedges() == 0);
    size_type nb_outedges1 = nb_outedges();
    assert(nb_outedges1 >= nb);
    if (nb_outedges1 == nb)
      return;
    size_type nb_f = f.size();
    size_type nb_m = nb_outedges_of_middle();
    if (nb <= nb_f) { // target in front edgelist
      m.swap(other.m);
      b.swap(other.b);
      edgelist_type edges = f;
      f = edgelist_type::take(edges, nb);
      other.f = edgelist_type::drop(edges, nb);
      nb -= f.size();
    } else if (nb <= nb_f + nb_m) { // target in middle sequence
      b.swap(other.b);
      nb -= nb_f;
      vtxid_type middle_vertex = -1000;
      bool found = m.split([nb] (vtxid_type n) { return nb <= n; }, middle_vertex, other.m);
      assert(found && middle_vertex != -1000);
      edgelist_type edges = create_edgelist(middle_vertex);
      nb -= nb_outedges_of_middle();
      b = edgelist_type::take(edges, nb);
      other.f = edgelist_type::drop(edges, nb);
      nb -= b.size();
    } else { // target in back edgelist
      nb -= nb_f + nb_m;
      edgelist_type edges = b;
      b = edgelist_type::take(edges, nb);
      other.b = edgelist_type::drop(edges, nb);
      nb -= b.size();
    }
    size_type nb_outedges2 = nb_outedges();
    size_type nb_other_outedges = other.nb_outedges();
    assert(nb_outedges1 == nb_outedges2 + nb_other_outedges);
    assert(nb == 0);
    check(); other.check();
  }
  
  // concatenate with data of `other`; leaving `other` empty
  // pre: back edgelist empty
  // pre: front edgelist of `other` empty
  void concat(self_type& other) {
    size_type nb_outedges1 = nb_outedges();
    size_type nb_outedges2 = other.nb_outedges();
    assert(b.size() == 0);
    assert(other.f.size() == 0);
    m.concat(other.m);
    b.swap(other.f);
    assert(nb_outedges() == nb_outedges1 + nb_outedges2);
    assert(other.nb_outedges() == 0);
  }
   // FORMIKE: let's call the function above "concat_core",
  // and have a function "concat" that pushes s1.b and s2.f into the middle sequences
 
  void swap(self_type& other) {
    check();
    other.check();
    // size_type nb1 = nb_outedges();
    // size_type nb2 = other.nb_outedges();
    f.swap(other.f);
    m.swap(other.m);
    b.swap(other.b);
    // assert(nb2 == nb_outedges());
    // assert(nb1 == other.nb_outedges());
    check(); other.check();
  }
  
  void clear_when_front_and_back_empty() {
    check();
    m.clear();
    assert(nb_outedges() == 0);
    check();
  }

  void clear() {
    check();
    f = edgelist_type();
    m.clear();
    b = edgelist_type();
    assert(nb_outedges() == 0);
    check();
  }

  template <class Body>
  void for_each_edgelist(const Body& func) const {
    if (f.size() > 0)
      func(f);
    m.for_each([&] (vtxid_type v) { func(create_edgelist(v)); });
    if (b.size() > 0)
      func(b);
  }

  template <class Body>
  void for_each_edgelist_when_front_and_back_empty(const Body& func) const {
    m.for_each([&] (vtxid_type v) { func(create_edgelist(v)); });
  }
  
  template <class Body>
  void for_each_outedge_when_front_and_back_empty(const Body& func) const {
    for_each_edgelist_when_front_and_back_empty([&] (edgelist_type edges) {
      for (auto e = edges.lo; e < edges.hi; e++)
        func(*e);
    });
  }

  template <class Body>
  void for_each_outedge(const Body& func) const {
    for_each_edgelist([&] (edgelist_type edges) {
      for (auto e = edges.lo; e < edges.hi; e++)
        func(*e);
    });
  }

  // Warning: "func" may only call "push_vertex_back"
  // Returns the number of nodes that have been processed
  template <class Body>
  size_type for_at_most_nb_outedges(size_type nb, const Body& func) {
    size_type nb_left = nb;
    size_type f_size = f.size();
    // process front if not empty
    if (f_size > 0) {
      if (f_size >= nb_left) {
        // process only part of the front
        auto e = edgelist_type::take(f, nb_left);
        f = edgelist_type::drop(f, nb_left);
        e.for_each(func);
        return nb;
      } else {
        // process all of the front, to begin with
        nb_left -= f_size;
        f.for_each(func);
        f.clear();
     }
    }
    // assume now front to be empty, and work on middle
    while (nb_left > 0 && ! m.empty()) {
      vtxid_type v = m.pop_back();
      edgelist_type edges = create_edgelist(v);
      size_type d = edges.size();
      if (d <= nb_left) {
        // process all of the edges associated with v
        edges.for_each(func);
        nb_left -= d;
      } else { // d > nb_left
        // save the remaining edges into the front
        f = edgelist_type::drop(edges, nb_left);
        auto edges2 = edgelist_type::take(edges, nb_left);
        edges2.for_each(func);
        return nb;
      }
    }
    // process the back if not empty
    size_type b_size = b.size();
    if (nb_left > 0 && b_size > 0) {
      if (b_size >= nb_left) {
        // process only part of the back, leave the rest to the front
        auto e = edgelist_type::take(b, nb_left);
        f = edgelist_type::drop(b, nb_left);
        b.clear();
        e.for_each(func);
        return nb;
      } else {
        // process all of the back
        b.for_each(func);
        b.clear();
      }
    }
    return nb - nb_left;
  }
  
  graph_type get_graph() const {
    return m.get_measure().get_env().g;
  }
  
  void set_graph(graph_type g) {
    using measure_type = typename cache_type::measure_type;
    graph_env env(g);
    measure_type meas(env);
    m.set_measure(meas);
  }
  
};
  
/*---------------------------------------------------------------------*/
  
static constexpr int chunk_capacity = 1024;

template <
  class Vertex,
  class Cache_policy
>
using chunkedbag = data::chunkedseq::bootstrapped::bagopt<Vertex, chunk_capacity, Cache_policy>;

template <
class Vertex,
class Cache_policy
>
using chunkedstack = data::chunkedseq::bootstrapped::stack<Vertex, chunk_capacity, Cache_policy>;
  
} // end namespace
  
/*---------------------------------------------------------------------*/

template <class Graph>
using frontiersegbag = frontiersegbase::frontiersegbase<Graph, frontiersegbase::chunkedbag>;
  
template <class Graph>
using frontiersegstack = frontiersegbase::frontiersegbase<Graph, frontiersegbase::chunkedstack>;

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PASL_GRAPH_FRONTIERSEG_H_ */
