/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file edgelist.hpp
 * \brief Edge-list graph format
 *
 */

#ifndef _PASL_GRAPH_EDGELIST_H_
#define _PASL_GRAPH_EDGELIST_H_

#include <vector>

#define FORCE_SEQUENTIAL_REMOVE_DUPLICATES 1 // until we debug parallel code

#if defined(SEQUENTIAL_ELISION) || defined(FORCE_SEQUENTIAL_REMOVE_DUPLICATES)
#include <unordered_set>
#endif

#include "graph.hpp"
#include "pcontainer.hpp"
#include "container.hpp"
#include "hashtable.hpp"
#include "utils.hpp"

/***********************************************************************/

namespace pasl {
namespace graph {
  
template <class Vertex_id>
class edge {
public:
  
  typedef Vertex_id vtxid_type;
  
  vtxid_type src;
  vtxid_type dst;
  
  edge(vtxid_type src, vtxid_type dst)
  : src(src), dst(dst) { }
  
  edge()
  : src(vtxid_type(0)), dst(vtxid_type(0)) { }
  
  void check(vtxid_type nb_vertices) const {
#ifndef NDEBUG
    check_vertex(src, nb_vertices);
    check_vertex(dst, nb_vertices);
#endif
  }
  
};
  
/*---------------------------------------------------------------------*/


//FORMIKE: suggest renaming to edgerange
template <class Edge_bag>
class edgelist {
public:
  
  typedef Edge_bag edge_bag_type;
  typedef typename edge_bag_type::value_type edge_type;
  typedef typename edge_type::vtxid_type vtxid_type;
  typedef edgelist<Edge_bag> self_type;
  
  vtxid_type nb_vertices;
  edge_bag_type edges;
  
  edgelist()
  : nb_vertices(0) { }
  
  edgelist(vtxid_type nb_vertices, edge_bag_type& other)
  : nb_vertices(nb_vertices) {
    edges.swap(other);
  }
  
  void clear() {
    nb_vertices = 0;
    edges.clear();
  }
  
  edgeid_type get_nb_edges() const {
    return edgeid_type(edges.size());
  }
  
  void check() const {
#ifndef NDEBUG
    for (edgeid_type i = 0; i < get_nb_edges(); i++)
      edges[i].check(nb_vertices);
#endif
  }
  
  edge_type* data() const {
    return edges.data();
  }
  
  void swap(self_type& other) {
    std::swap(nb_vertices, other.nb_vertices);
    edges.swap(other.edges);
  }
  
};
  
/*---------------------------------------------------------------------*/

template <class Vertex_id>
bool operator==(const edge<Vertex_id>& e1,
                const edge<Vertex_id>& e2) {
  return e1.src == e2.src
  && e1.dst == e2.dst;
}

template <class Vertex_id>
bool operator!=(const edge<Vertex_id>& e1,
                const edge<Vertex_id>& e2) {
  return ! (e1 == e2);
}

template <class Edge_bag>
bool operator==(const edgelist<Edge_bag>& e1, const edgelist<Edge_bag>& e2) {
  using edge_type = typename Edge_bag::value_type;
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  if (e1.nb_vertices != e2.nb_vertices)
    return false;
  if (e1.get_nb_edges() != e2.get_nb_edges())
    return false;
  std::vector<edge_type> edges1;
  std::vector<edge_type> edges2;
  for (edgeid_type i = 0; i < e1.get_nb_edges(); i++) {
    edges1.push_back(e1.edges[i]);
    edges2.push_back(e2.edges[i]);
  }
  auto comp = [] (edge_type e, edge_type f) {
    return e.src < f.src || ((e.src == f.src) && (e.dst < f.dst));
  };
  std::sort(edges1.begin(), edges1.end(), comp);
  std::sort(edges2.begin(), edges2.end(), comp);
  for (vtxid_type i = 0; i < edges1.size(); i++)
    if (edges1[i] != edges2[i])
      return false;
  return true;
}

template <class Edge_bag>
bool operator!=(const edgelist<Edge_bag>& e1, const edgelist<Edge_bag>& e2) {
  return ! (e1 == e2);
}
  
template <class Vertex_id>
class edge_hash {
public:
  std::size_t operator()(edge<Vertex_id> const& e) const {
    std::size_t h1 = std::hash<std::size_t>()(e.src);
    std::size_t h2 = std::hash<std::size_t>()(e.dst);
    return h1 ^ (h2 << 1);
  }
};
  
#if defined(SEQUENTIAL_ELISION) || defined(FORCE_SEQUENTIAL_REMOVE_DUPLICATES)
template <class Edge_bag>
void remove_duplicates_sequential(edgelist<Edge_bag>& src, edgelist<Edge_bag>& dst) {
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  vtxid_type nb_vertices = src.nb_vertices;
  std::unordered_set<edge<vtxid_type>, edge_hash<vtxid_type>> edges;
  edgeid_type nb_edges = src.get_nb_edges();
  for (edgeid_type i = 0; i < nb_edges; i++)
    edges.insert(src.edges[i]);
  edgeid_type nb_edges2 = edges.size();
  dst.edges.alloc(nb_edges2);
  edgeid_type k = 0;
  for (auto it = edges.begin(); it != edges.end(); it++)
    dst.edges[k++] = *it;
  dst.nb_vertices = nb_vertices;
}
#endif

template <class Edge_bag>
void remove_duplicates(edgelist<Edge_bag>& src, edgelist<Edge_bag>& dst) {
#if defined(SEQUENTIAL_ELISION) || defined(FORCE_SEQUENTIAL_REMOVE_DUPLICATES)
  remove_duplicates_sequential(src, dst);
#else
  using edgelist_type = edgelist<Edge_bag>;
  using edge_bag_type = Edge_bag;
  using vtxid_type = typename edge_bag_type::value_type::vtxid_type;
  using edge_type = typename edgelist_type::edge_type;
  struct hashEdge {
    typedef edge_type* eType;
    typedef edge_type* kType;
    eType empty() {return nullptr;}
    kType getKey(eType v) {return v;}
    unsigned int hash(kType v) {
      unsigned int x = (unsigned int)v->src >> 16;
      unsigned int y = (unsigned int)v->dst >> 16;
      return pbbs::utils::hash((x<<16) | y);
    }
    int cmp(kType v, kType b) {
      if (v->src > b->src)
        return 1;
      if (v->src == b->src) {
        if (v->dst == b->dst)
          return 0;
        else if (v->dst > b->dst)
          return 1;
      }
      return -1;
    }
    bool replaceQ(eType v, eType b) {return 0;}
  };
  edgeid_type m = src.get_nb_edges();
  edge_type** EP = data::mynew_array<edge_type*>(m);
  sched::native::parallel_for(edgeid_type(0), m, [&] (edgeid_type i) { EP[i] = &src.edges[i]; });
  pbbs::_seq<edge_type*> F = pbbs::removeDuplicates(pbbs::_seq<edge_type*>(EP,m), hashEdge());
  data::myfree(EP);
  edgeid_type l = edgeid_type(F.n);
  dst.edges.alloc(l);
  sched::native::parallel_for(edgeid_type(0), l, [&] (edgeid_type i) {
    dst.edges[i] = *F.A[i];
  });
  dst.nb_vertices = src.nb_vertices;
  dst.check();
#endif
}
  
template <class Edge_bag>
typename edgelist<Edge_bag>::vtxid_type max_vtxid_of_edgelist(const edgelist<Edge_bag>& edges) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  auto load_fct = [&] (edgeid_type i) {
    edge_type e = edges.edges[i];
    return std::max(e.src, e.dst);
  };
  edgeid_type nb_edges = edges.edges.size();
  return pbbs::sequence::maxReduce((vtxid_type*)nullptr, nb_edges, load_fct);
}

template <class Edge_bag>
void make_edgelist_graph_undirected(edgelist<Edge_bag>& edges) {
  using vtxid_type = typename edgelist<Edge_bag>::vtxid_type;
  using edge_type = typename edgelist<Edge_bag>::edge_type;
  using edge_container_type = data::pcontainer::stack<edge_type>;
  vtxid_type nb_vertices = edges.nb_vertices;
  edgeid_type nb_edges = edges.get_nb_edges();
  edge_container_type tmp;
  data::pcontainer::combine(edgeid_type(0), nb_edges, tmp, [&] (size_t i, edge_container_type& tmp) {
    edge_type e = edges.edges[i];
    tmp.push_back(e);
    tmp.push_back(edge_type(e.dst, e.src));
  });
  edges.clear();
  edgelist<Edge_bag> edges1;
  //  std::cout << "transfer contents" << std::endl;
  data::pcontainer::transfer_contents_to_array_seq(tmp, edges1.edges);
  edges1.nb_vertices = nb_vertices;
  edgeid_type nb_edges1 = edges1.get_nb_edges();
  edgelist<Edge_bag> edges2;
  //  std::cout << "remove duplicates" << std::endl;
  remove_duplicates(edges1, edges2);
  edgeid_type nb_edges2 = edges2.get_nb_edges();
  std::cout << "nb_duplicates\t" << (nb_edges1 - nb_edges2) << std::endl;
  edges2.swap(edges);
  edges.nb_vertices = nb_vertices;
}
  
} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_EDGELIST_H_ */
