
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>

#ifndef _PASL_GRAPH_BELLMAN_FORD_H_
#define _PASL_GRAPH_BELLMAN_FORD_H_
	
/***********************************************************************/

namespace pasl {
  namespace graph {
    
    namespace util {         
      template <class Size, class Dist>
      void print_dists(Size size, Dist * dist) {
        for (int i = 0; i < size; i++) {
          std::cout << dist[i] << " ";
        }
        std::cout << std::endl;
      }
      
      /*---------------------------------------------------------------------*/
      /* Set dist[V] = -inf for all V reachable	*/ 
      /* from the negative cycle vertices				*/
      /*---------------------------------------------------------------------*/
      template <class Adjlist_seq>
      int* normalize(const adjlist<Adjlist_seq>& graph, int* dists) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        std::queue<vtxid_type> queue;
        
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            
            if (dists[other] > dists[i] + w) {
              queue.push(other);
              dists[other] = shortest_path_constants<int>::minus_inf_dist;
            }
          }
        }
        while (!queue.empty()) {
          vtxid_type from = queue.front();
          queue.pop();
          vtxid_type degree = graph.adjlists[from].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[from].get_out_neighbor(edge);			          
            if (dists[other] != shortest_path_constants<int>::minus_inf_dist) {
              queue.push(other);
              dists[other] = shortest_path_constants<int>::minus_inf_dist;
            }
          }
        }      
        return dists;
      }   
    } // end namespace util
    
    enum { 	
      SERIAL_CLASSIC,
      SERIAL_YEN,      
      SERIAL_BFS,
      PAR_NUM_VERTICES,
      PAR_NUM_EDGES,
      PAR_BFS,
      PAR_COMBINED,
      NB_BF_ALGO 
    };      			
    
    std::string const algo_names[] = {
      "SerialClassic", 
      "SerialYen",       
      "SerialBFS", 
      "ParNumVertices", 
      "ParNumEdges", 
      "ParBFS", 
      "ParCombined"};
        
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; serial classic */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq_classic(const adjlist<Adjlist_seq>& graph,
                                  typename adjlist<Adjlist_seq>::vtxid_type source) {
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      int inf_dist = shortest_path_constants<int>::inf_dist;
      vtxid_type nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);
      
      fill_array_seq(dists, nb_vertices, inf_dist);
      
      dists[source] = 0;
      
      LOG_BASIC(ALGO_PHASE);
      int steps = 0;
      for (size_t step = 0; step < nb_vertices; step++) {
        steps++;
        bool changed = false;
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            
            if (dists[other] > dists[i] + w) {
              changed = true;
              dists[other] = dists[i] + w;
            }
          }
        }
        if (!changed) break;
      }
      std::cout << "Rounds : " << steps << std::endl;      
      return util::normalize(graph, dists);
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; serial classic with Yen's optimization */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq_classic_opt(const adjlist<Adjlist_seq>& graph,
                                  typename adjlist<Adjlist_seq>::vtxid_type source) {
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      int inf_dist = shortest_path_constants<int>::inf_dist;
      vtxid_type nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);
      
      fill_array_seq(dists, nb_vertices, inf_dist);
      
      dists[source] = 0;
      
      LOG_BASIC(ALGO_PHASE);
      int steps = 0;
      for (size_t step = 0; step < nb_vertices; step++) {
        steps++;
        bool changed = false;
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            
            if (dists[other] > dists[i] + w) {
              changed = true;
              dists[other] = dists[i] + w;
            }
          }
        }
        if (!changed) break;
      }
      std::cout << "Rounds : " << steps << std::endl;      
      return util::normalize(graph, dists);
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; serial bfs */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq_bfs(const adjlist<Adjlist_seq>& graph,
                              typename adjlist<Adjlist_seq>::vtxid_type source) {
      
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      int inf_dist = shortest_path_constants<int>::inf_dist;
      vtxid_type nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);      
      fill_array_seq(dists, nb_vertices, inf_dist);      
      dists[source] = 0; 
      
      LOG_BASIC(ALGO_PHASE);
      std::queue<vtxid_type> cur, next;
      cur.push(source);
      int steps = 0;
      double total_size = 0.;
      
      while (steps < nb_vertices) {
        steps++;
        if (steps > nb_vertices) break; 
        std::queue<vtxid_type> empty;
        std::swap(next, empty);
        total_size += cur.size();
        while (!cur.empty()) {
          vtxid_type from = cur.front();
          cur.pop();
          vtxid_type degree = graph.adjlists[from].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[from].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[from].get_out_neighbor_weight(edge);
            
            if (dists[other] > dists[from] + w) {
              next.push(other);
              dists[other] = dists[from] + w;
            }
          }          
        }
        std::swap(cur, next);
      }
      std::cout << "Rounds : " << steps << "; Avg queue size : " << total_size / steps << std::endl;
      return util::normalize(graph, dists);
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel by number of vertices */
    /*---------------------------------------------------------------------*/
    
    extern const int bellman_ford_par_by_vertices_cutoff;
    
    template <class Adjlist_seq>
    int* bellman_ford_par_vertices(const adjlist<Adjlist_seq>& graph,
                           typename adjlist<Adjlist_seq>::vtxid_type source) {
      int inf_dist = shortest_path_constants<int>::inf_dist;
      long nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);
      
      fill_array_seq(dists, nb_vertices, inf_dist);
      
      dists[source] = 0;
      
      LOG_BASIC(ALGO_PHASE);
      bool changed = false;
      
      int steps = 0;
      for (int i = 0; i < nb_vertices; i++) {
        steps++;
        changed = false;
        process_par_by_vertices(graph, dists, 0, nb_vertices, changed);
        if (!changed) break;
      }
      std::cout << "Rounds : " << steps << std::endl;      
      return util::normalize(graph, dists);
    }
    
    template <class Adjlist_seq>
    void process_vertex_seq(const adjlist<Adjlist_seq>& graph,
                            int* dists, int vertex, bool& changed) {
      long degree = graph.adjlists[vertex].get_in_degree();
      for (int edge = 0; edge < degree; edge++) {
        long other = graph.adjlists[vertex].get_in_neighbor(edge);
        long w = graph.adjlists[vertex].get_in_neighbor_weight(edge);
        if (dists[vertex] > dists[other] + w) {
          dists[vertex] = dists[other] + w;
          changed = true;
        }
      }
    }
    
    template <class Adjlist_seq>
    void process_vertices_seq(const adjlist<Adjlist_seq>& graph,
                              int* dists, int start, int stop, bool& changed) {
      for (int i = start; i < stop; i++) {
        process_vertex_seq(graph, dists, i, changed);
      }
    }
    
    template <class Adjlist_seq>
    void process_par_by_vertices(const adjlist<Adjlist_seq>& graph,
                               int* dists, int start, int stop, bool& changed) {
      int nb = stop - start;
      if (nb < bellman_ford_par_by_vertices_cutoff) {
        process_vertices_seq(graph, dists, start, stop, changed);
      } else {
        int mid = (start + stop) / 2;
        sched::native::fork2([&] { process_par_by_vertices(graph,  dists, start, mid, changed); },
                             [&] { process_par_by_vertices(graph,  dists, mid, stop, changed); });
        
      }
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel by number of edges */
    /*---------------------------------------------------------------------*/
    
    extern const int bellman_ford_par_by_edges_cutoff;

    template <class Adjlist_seq>
    int* bellman_ford_par_edges(const adjlist<Adjlist_seq>& graph,
                           typename adjlist<Adjlist_seq>::vtxid_type source) {
      int inf_dist = shortest_path_constants<int>::inf_dist;
      long nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);
      
      fill_array_seq(dists, nb_vertices, inf_dist);
      
      dists[source] = 0;
      
      LOG_BASIC(ALGO_PHASE);
      int* pref_sum = data::mynew_array<int>(nb_vertices + 1);
      pref_sum[0] = 0;
      
      for (int i = 1; i < nb_vertices + 1; i++) {
        pref_sum[i] = pref_sum[i - 1] + graph.adjlists[i - 1].get_in_degree();
      }
      
      bool changed = false;
      int steps = 0;
      
      for (int i = 0; i < nb_vertices; i++) {
        steps++;
        changed = false;
        process_par_by_edges(graph, dists, 0, nb_vertices, pref_sum, changed);
        if (!changed) break;
      }
      std::cout << "Rounds : " << steps << std::endl;
      
      return util::normalize(graph, dists);
    }
    
    template <class Adjlist_seq>
    void process_par_by_edges(const adjlist<Adjlist_seq>& graph,
                               int * dists, int start, int stop, int * pref_sum, bool & changed) {
      int nb_edges = pref_sum[stop] - pref_sum[start];
      if (nb_edges < bellman_ford_par_by_edges_cutoff || stop - start == 1) {
        std::cout << "Do it in " << std::this_thread::get_id() << std::endl;

        process_vertices_seq(graph, dists, start, stop, changed);
      } else {
        std::cout << "Fork" << std::endl;
        int mid_val = (pref_sum[start] + pref_sum[stop]) / 2;
        int left = start, right = stop;
        while (right - left > 1) {
          int m = (left + right) / 2;
          if (pref_sum[m] <= mid_val) {
            left = m;
          } else {
            right = m;
          }
        }
        
        sched::native::fork2([&] { process_par_by_edges(graph,  dists, start, left, pref_sum, changed); },
                             [&] { process_par_by_edges(graph,  dists, left, stop, pref_sum, changed); });
        
      }
    }  
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel BFS-like */
    /*---------------------------------------------------------------------*/
        
    extern const int bellman_ford_bfs_process_layer_cutoff;
    extern const int bellman_ford_bfs_process_next_vertices_cutoff;    
    
    template <class Adjlist_seq>
    class bfs_bellman_ford {
      public:
      
      template <class Graph, class SizeType, class VertexIdType>
      class graph_env {
        public:
        
        Graph g;
        
        graph_env() { }
        graph_env(Graph g) : g(g) { }
        
        SizeType operator()(VertexIdType v) const {
          return g.adjlists[v].get_in_degree();
        }
        
      };
      
      using self_type = bfs_bellman_ford;
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      // TODO : replace with another type
      using edgeweight_type = int;
      
      using adjlist_type = adjlist<Adjlist_seq>;
      using adjlist_alias_type = typename adjlist_type::alias_type;
      using size_type = size_t;
      
      using Frontier = pasl::graph::frontiersegbag<adjlist_alias_type>;

      using edgelist_type = typename Frontier::edgelist_type;
      using graph_type = adjlist_type;
      using graph_env_help = graph_env<adjlist_alias_type, size_type, vtxid_type>;
      using cache_type = data::cachedmeasure::weight<vtxid_type, vtxid_type, size_type, graph_env_help>;
      using seq_type = data::chunkedseq::bootstrapped::bagopt<vtxid_type, 1024, cache_type>;
      

      
      static bool try_to_set_visited(vtxid_type target, std::atomic<bool>* visited) {
        bool unvisited = false;
        if (! visited[target].compare_exchange_strong(unvisited, true)) return false;
        return true;
      }
      
      template <class Adjlist_alias, class Frontier, class WeightedSeq, class DistFromParent>
      static void process_layer_par(Adjlist_alias graph_alias,
                                 std::atomic<bool>* visited,
                                 Frontier& prev,
                                 Frontier& next,
                                 WeightedSeq& next_vertices,
                                 int * dists,
                                 DistFromParent& dists_from_parent ) {
        auto cutoff = [] (Frontier& f) {
          return f.nb_outedges() <= vtxid_type(bellman_ford_bfs_process_layer_cutoff);
        };
        auto split = [] (Frontier& src, Frontier& dst) {
          std::cout << "Fork" << std::endl;
          assert(src.nb_outedges() > 1);
          src.split(src.nb_outedges() / 2, dst);
        };
        auto append = [] (Frontier& src, Frontier& dst) {
          src.concat(dst);
        };
        auto set_env = [graph_alias] (Frontier& f) {
          f.set_graph(graph_alias);
        };
        sched::native::forkjoin(prev, next, cutoff, split, append, set_env, set_env,
                                [&] (Frontier& prev, Frontier& next) {
                                  std::cout << "Do it in " << std::this_thread::get_id() << std::endl;
                                  prev.for_each_outedge([&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
                                    if (dists[to] > dists[from] + weight) {
                                      if ((*dists_from_parent[to])[from] > dists[from] + weight) {
                                        (*dists_from_parent[to])[from] = dists[from] + weight;                                        
                                      }
                                      if (try_to_set_visited(to, visited)) {
                                        next.push_vertex_back(to);
                                        next_vertices.push_back(to);
                                      }
                                    }
                                  });
                                  
                                  prev.clear();
                                });
      }
      
      template <class Adjlist, class WeightedSeq, class DistFromParent>
      static void process_next_vert(Adjlist&  graph,
                             std::atomic<bool>* visited,
                             WeightedSeq& next_vertices,
                             int * dists,
                             DistFromParent& dists_from_parent ) {
        using vtxid_type = typename Adjlist::vtxid_type;
        if (next_vertices.get_cached() < bellman_ford_bfs_process_next_vertices_cutoff) {
          next_vertices.for_each([&] (vtxid_type vertex) {
            visited[vertex] = false;
            long degree = graph.adjlists[vertex].get_in_degree();
            for (int edge = 0; edge < degree; edge++) {
              auto from = graph.adjlists[vertex].get_in_neighbor(edge);
              if (dists[vertex] > (*dists_from_parent[vertex])[from]) {
                dists[vertex] = (*dists_from_parent[vertex])[from];
              }
            }
          });
        } else {
          WeightedSeq other;
          auto nb = *(next_vertices.begin());
          nb = next_vertices.get_cached() / 2;
          next_vertices.split([nb] (vtxid_type n) { return nb <= n; }, other);
          sched::native::fork2([&] { process_next_vert(graph, visited, next_vertices, dists, dists_from_parent); },
                               [&] { process_next_vert(graph, visited, other, dists, dists_from_parent); });
          
        }
      }

      static edgeweight_type* bellman_ford_par_bfs(const adjlist<Adjlist_seq>& graph,
                             typename adjlist<Adjlist_seq>::vtxid_type source) {
        auto inf_dist = shortest_path_constants<edgeweight_type>::inf_dist;

        vtxid_type nb_vertices = graph.get_nb_vertices();
        edgeweight_type* dists = data::mynew_array<edgeweight_type>(nb_vertices);
        std::map<int, int>** dists_from_parent = data::mynew_array<std::map<int, int>*>(nb_vertices);
        for (int i = 0; i < nb_vertices; ++i) {
          auto cur_size = graph.adjlists[i].get_in_degree();
          dists_from_parent[i] = new std::map<int, int>();
          for (int j = 0; j < cur_size; j++) {
            (*dists_from_parent[i])[graph.adjlists[i].get_in_neighbor(j)] = inf_dist;
          }
        }
        fill_array_seq(dists, nb_vertices, inf_dist);
        dists[source] = 0;
        
        LOG_BASIC(ALGO_PHASE);
        auto graph_alias = get_alias_of_adjlist(graph);
        Frontier frontiers[2];
        seq_type next_vertices;
        
        using measure_type = typename cache_type::measure_type;
        graph_env_help env(graph_alias);
        measure_type meas(env);
        next_vertices.set_measure(meas);
        
        frontiers[0].set_graph(graph_alias);
        frontiers[1].set_graph(graph_alias);
        vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
        vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
        frontiers[0].push_vertex_back(source);
        
        std::atomic<bool>* visited = data::mynew_array<std::atomic<bool>>(nb_vertices);
        fill_array_par(visited, nb_vertices, false);
        int steps = 0;
        while (! frontiers[cur].empty()) {
          next_vertices.clear();
          steps++;
          if (steps > nb_vertices) break;
          if (frontiers[cur].nb_outedges() <= bellman_ford_bfs_process_layer_cutoff) {
            frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
              if (dists[to] > dists[from] + weight) {
                if ((*dists_from_parent[to])[from] > dists[from] + weight)
                (*dists_from_parent[to])[from] = dists[from] + weight;
                if (try_to_set_visited(to, visited)) {
                  frontiers[nxt].push_vertex_back(to);
                  next_vertices.push_back(to);
                }
              }
            });
            frontiers[cur].clear_when_front_and_back_empty();
          } else {
            self_type::process_layer_par(graph_alias, visited, frontiers[cur], frontiers[nxt], next_vertices, dists, dists_from_parent);
          }
          self_type::process_next_vert(graph, visited, next_vertices, dists, dists_from_parent);
          cur = 1 - cur;
          nxt = 1 - nxt;
        }
        
        
        std::cout << "Rounds : " << steps << std::endl;      
        return util::normalize(graph, dists);
      }            
    };
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; combined version */
    /*---------------------------------------------------------------------*/
    extern const std::function<bool(double, double)> algo_chooser_pred;
    
    template <class Adjlist_seq>
    int* bellman_ford_par_combined(const adjlist<Adjlist_seq>& graph,
                                typename adjlist<Adjlist_seq>::vtxid_type source) {
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      vtxid_type nb_vertices = graph.get_nb_vertices();
      auto num_edges = graph.nb_edges;
      auto num_less = 0;
      for (size_t from = 0; from < nb_vertices; from++) {
        vtxid_type degree = graph.adjlists[from].get_out_degree();
        for (vtxid_type edge = 0; edge < degree; edge++) {
          vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
          if (from < to) num_less++;
        }
      }
      auto f = (.0 + num_less) / num_edges;
      auto d = (.0 + num_edges) / nb_vertices;
      if (algo_chooser_pred(f, d)) {
        std::cout << "choosed classic" << std::endl;
        return bellman_ford_par_edges(graph, source);
      } else {
        std::cout << "choosed bfs" << std::endl;
        return bfs_bellman_ford<Adjlist_seq>::bellman_ford_par_bfs(graph, source);        
      }
    }
        
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
