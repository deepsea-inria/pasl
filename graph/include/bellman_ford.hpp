#include "base_algo.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>
#include <unordered_map>

#ifndef _PASL_GRAPH_BELLMAN_FORD_H_
#define _PASL_GRAPH_BELLMAN_FORD_H_

/***********************************************************************/

namespace pasl {
  namespace graph {
    extern int bellman_ford_par_bfs_cutoff;
    extern int bellman_ford_par_serial_cutoff;
    const int communicate_cutoff = 1024;
    
    template <class Adjlist_seq>
    class bellman_ford_algo: public base_algo<Adjlist_seq> {
    public:
      
      std::string get_impl_name(int index) {
        return bf_algo_names[index];
      }
      
      int get_impl_count() {
        return BF_NB_ALGO;
      }
      
      int* get_dist(int algo_id, const adjlist<Adjlist_seq>& graph, int source_vertex) {
        switch (algo_id) {
          case BF_SERIAL_CLASSIC:
            return bellman_ford_seq_classic(graph, source_vertex);
            break;
          case BF_SERIAL_BFS:
            return bellman_ford_seq_bfs(graph, source_vertex);
            break;
          case BF_PAR_CLASSIC:
            return bellman_ford_par_edges(graph, source_vertex);
            break;
          case BF_PAR_BFS:
            return bfs_bellman_ford::bellman_ford_par_bfs(graph, source_vertex);
            break; 
          default:
            return bellman_ford_seq_classic(graph, source_vertex);
        }
      }
      
      enum { 	
        BF_SERIAL_CLASSIC,
        BF_SERIAL_BFS,
        BF_PAR_CLASSIC,
        BF_PAR_BFS,
        BF_NB_ALGO 
      };      			
      
      std::string const bf_algo_names[4] = {
        "SerialClassic", 
        "SerialBFS", 
        "ParClassic", 
        "ParBFS"};  
      
      
      
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
      static int* normalize(const adjlist<Adjlist_seq>& graph, int* dists) {
        //        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        //        vtxid_type nb_vertices = graph.get_nb_vertices();
        //        std::queue<vtxid_type> queue;
        //        
        //        for (size_t i = 0; i < nb_vertices; i++) {
        //          vtxid_type degree = graph.adjlists[i].get_out_degree();
        //          for (vtxid_type edge = 0; edge < degree; edge++) {
        //            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
        //            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
        //            
        //            if (dists[other] > dists[i] + w) {
        //              queue.push(other);
        //              dists[other] = shortest_path_constants<int>::minus_inf_dist;
        //            }
        //          }
        //        }
        //        while (!queue.empty()) {
        //          vtxid_type from = queue.front();
        //          queue.pop();
        //          vtxid_type degree = graph.adjlists[from].get_out_degree();
        //          for (vtxid_type edge = 0; edge < degree; edge++) {
        //            vtxid_type other = graph.adjlists[from].get_out_neighbor(edge);			          
        //            if (dists[other] != shortest_path_constants<int>::minus_inf_dist) {
        //              queue.push(other);
        //              dists[other] = shortest_path_constants<int>::minus_inf_dist;
        //            }
        //          }
        //        }      
        return dists;
      }   
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Bellman-Ford; serial classic */
      /*---------------------------------------------------------------------*/
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
          for (int i = 0; i < nb_vertices; i++) {
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
        return normalize(graph, dists);
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Bellman-Ford; serial bfs */
      /*---------------------------------------------------------------------*/
      int* bellman_ford_seq_bfs(const adjlist<Adjlist_seq>& graph,
                                typename adjlist<Adjlist_seq>::vtxid_type source) {
        
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);   
        int* visited = data::mynew_array<int>(nb_vertices);   
        
        fill_array_seq(dists, nb_vertices, inf_dist);      
        fill_array_seq(visited, nb_vertices, -1);      
        dists[source] = 0; 
        
        std::queue<vtxid_type> cur, next;
        cur.push(source);
        int steps = 0;
        double total_size = 0.;
        
        while (steps < nb_vertices && !cur.empty()) {
          steps++;
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
                if (visited[other] != steps) {
                  next.push(other);
                  visited[other] = steps;                
                }
                dists[other] = dists[from] + w;
              }
            }          
          }
          std::swap(cur, next);
        }
        std::cout << "Rounds : " << steps << "; Avg queue size : " << total_size / steps << std::endl;
        free(visited);
        return normalize(graph, dists);
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Bellman-Ford; parallel classic */
      /*---------------------------------------------------------------------*/  
      
      int* bellman_ford_par_edges(const adjlist<Adjlist_seq>& graph,
                                  typename adjlist<Adjlist_seq>::vtxid_type source) {
        if (graph.fraction > 0.97) {
          std::cout << "Choose high fraction method" << std::endl;
          return bellman_ford_par_group_edges(graph, source);
        } else {
          std::cout << "Choose low fraction method" << std::endl;
          return bellman_ford_par_all_edges(graph, source);
        }
      }
      
      // For high fraction
      int* bellman_ford_par_group_edges(const adjlist<Adjlist_seq>& graph,
                                        typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        sched::native::parallel_for(0, nb_vertices, [&] (int i) {
          dists[i] = inf_dist;
        });
        
        dists[source] = 0;
        bool changed = false;
        int steps = 0;
        for (int i = 0; i < nb_vertices; i++) {
          steps++;
          changed = false;
          for (int j = 0; j < nb_vertices; j++) {
            vtxid_type degree = graph.adjlists[j].get_in_degree();            
            if (degree < 1000) {
              process_vertex_seq(graph, dists, j, changed);
            } else {
              process_vertex_par(graph, dists, j, changed);
            }            
          }
          if (!changed) break;
        }
        std::cout << "Rounds : " << steps << std::endl;
        return normalize(graph, dists);
      }
      
      // For low fraction
      int* bellman_ford_par_all_edges(const adjlist<Adjlist_seq>& graph,
                                      typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        sched::native::parallel_for(0, nb_vertices, [&] (int i) {
          dists[i] = inf_dist;
        });
        
        dists[source] = 0;
        
        int* pref_sum = data::mynew_array<int>(nb_vertices + 1);
        pref_sum[0] = 0;
        
        for (int i = 1; i < nb_vertices + 1; i++) {
          pref_sum[i] = pref_sum[i - 1] + graph.adjlists[i - 1].get_in_degree();
        }
        std::unordered_map<long long, int> mid_map;
        int forked_cnt = 0;
        build_plan(mid_map, 0, nb_vertices, pref_sum, nb_vertices, forked_cnt);
        
        bool changed = false;
        int steps = 0;
        
        for (int i = 0; i < nb_vertices; i++) {
          steps++;
          changed = false;
          process_par_by_edges(graph, dists, 0, nb_vertices, pref_sum, mid_map, changed, nb_vertices);
          if (!changed) break;
        }
        std::cout << "Rounds : " << steps << "; Forked per round : " << forked_cnt << std::endl;
        
        return normalize(graph, dists);
      }

			// Utils
      
      void build_plan(std::unordered_map<long long, int> & mid_map, int start, int stop, int * pref_sum, int & vertex_num, int & forked_cnt) {
        int nb_edges = pref_sum[stop] - pref_sum[start];
        if (nb_edges >= bellman_ford_par_serial_cutoff && stop - start > 2) {
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
          long long id = ((long long) start) * vertex_num + stop;
          mid_map[id] = left;
          forked_cnt++;
          build_plan(mid_map, start, left, pref_sum, vertex_num, forked_cnt);
          build_plan(mid_map, left, stop, pref_sum, vertex_num, forked_cnt);        
        }
      }  
      
      void process_vertex_seq(const adjlist<Adjlist_seq>& graph,
                              int* dists, int vertex, bool& changed) {
        int degree = graph.adjlists[vertex].get_in_degree();
        int* neighbours = graph.adjlists[vertex].get_in_neighbors();

        for (int edge = 0; edge < degree; edge++) {
          int other = neighbours[edge];
          int new_dist = dists[other] + neighbours[edge + degree];
          if (dists[vertex] > new_dist) {
            dists[vertex] = new_dist;
            changed = true;
          }
        }
      }
      
      void process_vertex_par(const adjlist<Adjlist_seq>& graph,
                              int* dists, int vertex, bool& changed) {
        std::atomic<int> min_val;
        int degree = graph.adjlists[vertex].get_in_degree();
        int* neighbours = graph.adjlists[vertex].get_in_neighbors();

        min_val.store(dists[vertex]);
        sched::native::parallel_for(0, degree, [&] (int edge) {
          int other = neighbours[edge];
          int w = dists[other] + neighbours[edge + degree];
          changed |= update_minimum(min_val, w);
        });
      }
      
      void process_vertices_seq(const adjlist<Adjlist_seq>& graph,
                                int* dists, int start, int stop, bool& changed) {
        for (int i = start; i < stop; i++) {
          process_vertex_seq(graph, dists, i, changed);
        }
      }
      

      void process_par_by_edges(const adjlist<Adjlist_seq>& graph,
                                int * dists, int start, int stop, int * pref_sum, std::unordered_map<long long, int> & mid_map,  bool & changed, int & vertex_num) {
        int nb_edges = pref_sum[stop] - pref_sum[start];
        if (nb_edges < bellman_ford_par_serial_cutoff || stop - start <= 2) {
          process_vertices_seq(graph, dists, start, stop, changed);
        } else {
          long long id = ((long long) start) * vertex_num + stop;
          int mid = mid_map[id];
          sched::native::fork2([&] { process_par_by_edges(graph,  dists, start, mid, pref_sum, mid_map, changed, vertex_num); },
                               [&] { process_par_by_edges(graph,  dists, mid, stop, pref_sum, mid_map, changed, vertex_num); });        
        }
      }  
      
      
      bool update_minimum(std::atomic<int>& min_value, int const& value) {
        int prev_value = min_value;
        bool res = prev_value > value;
        while(prev_value > value &&
              !min_value.compare_exchange_weak(prev_value, value))
          ;
        return res;
      }
      
      
      
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Bellman-Ford; parallel BFS */
      /*---------------------------------------------------------------------*/
      
      
      class bfs_bellman_ford {
      public:      
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
        
        static bool should_call_communicate() {
#ifndef USE_CILK_RUNTIME
          return sched::threaddag::my_sched()->should_call_communicate();
#else
          return sched::native::my_deque_size() == 0;
#endif
        }
        
        static void reject() {
#ifndef USE_CILK_RUNTIME
          sched::threaddag::my_sched()->reject();
#else
#endif
        }
        
        static void unblock() {
#ifndef USE_CILK_RUNTIME
          sched::threaddag::my_sched()->unblock();
#else
#endif
        }
        
        
        static inline bool try_to_set_visited(vtxid_type & target, int & layer, std::atomic<int>* visited) {
          int cur_d = visited[target].load(std::memory_order_relaxed);
          if (cur_d == layer)
            return false;
          return visited[target].compare_exchange_strong(cur_d, layer);
        }
        
        template <class ET>
        static inline bool CAS(ET *ptr, ET oldv, ET newv) {
          if (sizeof(ET) == 8) {
            long* o = (long*) &oldv;
            long* n = (long*) &newv;
            return __sync_bool_compare_and_swap((long*)ptr, *o, *n);
          } else if (sizeof(ET) == 4) {
            int* o = (int*) &oldv;
            int* n = (int*) &newv;
            return __sync_bool_compare_and_swap((int*)ptr, *o, *n);
          } else {
            std::cout << "CAS bad length" << std::endl;
            abort();
          }
        }
        
        template <class ET>
        static inline bool writeMin(ET *a, ET b) {
          ET c; bool r=0;
          do c = *a; 
          while (c > b && !(r=CAS(a,c,b)));
          return r;
        }
        
        static inline bool try_to_update_dist(vtxid_type & target, int & candidate, int* dists) {
          return writeMin(&dists[target], candidate);
        }
        
        template <class Adjlist_alias, class Frontier>
        static void process_layer_par_lazy(const Adjlist_alias & graph_alias,
                                           std::atomic<int>* visited,
                                           Frontier& prev,
                                           Frontier& next,
                                           int * dists,
                                           int & layer, 
                                           std::atomic<int> & forked_first_cnt) {
          
          vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
          size_t nb_outedges = prev.nb_outedges();
          bool blocked = false;
          while (nb_outedges > 0) {
            if (nb_outedges <= bellman_ford_par_bfs_cutoff) {
              blocked = true;
              reject();
            }
            if (should_call_communicate()) {
              if (nb_outedges > bellman_ford_par_bfs_cutoff) {
                Frontier fr_in(graph_alias);
                Frontier fr_out(graph_alias);
                prev.split(prev.nb_outedges() / 2, fr_in);
                forked_first_cnt++;
                sched::native::fork2([&] { process_layer_par_lazy(graph_alias, visited, prev, next, dists, layer, forked_first_cnt); },
                                     [&] { process_layer_par_lazy(graph_alias, visited, fr_in, fr_out, dists, layer, forked_first_cnt); });
                next.concat(fr_out);
                
                if (blocked) // should always be false due to the order of the conditionals; yet, keep it for safety
                  unblock(); 
                return;
              }
              else
              {
                blocked = true;
                reject();
              }
            }
            prev.for_at_most_nb_outedges(communicate_cutoff, [&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
              int candidate = dists[from] + weight;
              if (try_to_update_dist(to, candidate, dists)) {
                if (try_to_set_visited(to, layer, visited)) {
                  next.push_vertex_back(to);
                }
              }
            });
            nb_outedges = prev.nb_outedges();
          }
          if (blocked)
            unblock();
        }
        
        
        template <class Adjlist_alias, class Frontier>
        static void process_layer_par(const Adjlist_alias & graph_alias,
                                      std::atomic<int>* visited,
                                      Frontier& prev,
                                      Frontier& next,
                                      int * dists,
                                      int & layer, 
                                      std::atomic<int> & forked_first_cnt) {
          if (prev.nb_outedges() <= bellman_ford_par_bfs_cutoff) {
            prev.for_each_outedge([&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
              int candidate = dists[from] + weight;
              if (try_to_update_dist(to, candidate, dists)) {
                if (try_to_set_visited(to, layer, visited)) {
                  next.push_vertex_back(to);
                }
              }
            });          
            prev.clear();
          } else {
            Frontier fr_in(graph_alias);
            Frontier fr_out(graph_alias);
            prev.split(prev.nb_outedges() / 2, fr_in);
            forked_first_cnt++;
            sched::native::fork2([&] { process_layer_par(graph_alias, visited, prev, next, dists, layer, forked_first_cnt); },
                                 [&] { process_layer_par(graph_alias, visited, fr_in, fr_out, dists, layer, forked_first_cnt); });
            next.concat(fr_out);
          }
        }
        
        static edgeweight_type* bellman_ford_par_bfs(const adjlist<Adjlist_seq>& graph,
                                                     typename adjlist<Adjlist_seq>::vtxid_type source, bool debug = true) {
          std::vector<typename adjlist<Adjlist_seq>::vtxid_type> sources  = {source};
          return bellman_ford_par_bfs(graph, sources, debug);
        }      
        
        static edgeweight_type* bellman_ford_par_bfs(const adjlist<Adjlist_seq>& graph, std::vector<typename adjlist<Adjlist_seq>::vtxid_type> sources, bool debug = true) {
          auto inf_dist = shortest_path_constants<edgeweight_type>::inf_dist;
          
          vtxid_type nb_vertices = graph.get_nb_vertices();
          edgeweight_type* dists = data::mynew_array<edgeweight_type>(nb_vertices);
          std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
          int unknown = 0;
          fill_array_par(visited, nb_vertices, unknown);
          
          fill_array_seq(dists, nb_vertices, inf_dist);
          
          
          
          LOG_BASIC(ALGO_PHASE);
          auto graph_alias = get_alias_of_adjlist(graph);
          Frontier frontiers[2];
          
          frontiers[0].set_graph(graph_alias);
          frontiers[1].set_graph(graph_alias);
          vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
          vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
          for (vtxid_type cur_source : sources) {
            dists[cur_source] = 0;   
            frontiers[0].push_vertex_back(cur_source);
          }
          
          int steps = 0;
          std::atomic<int> forked_first_cnt;
          
          forked_first_cnt.store(0);
          
          while (! frontiers[cur].empty()) {
            steps++;
            if (steps > nb_vertices) break;
            if (frontiers[cur].nb_outedges() <= bellman_ford_par_bfs_cutoff) {
              frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
                if (dists[to] > dists[from] + weight) {
                  dists[to] = dists[from] + weight;
                  if (visited[to] != steps) {
                    visited[to] = steps;
                    frontiers[nxt].push_vertex_back(to);                  
                  }
                }
              });
              frontiers[cur].clear_when_front_and_back_empty();
            } else {            
              self_type::process_layer_par_lazy(graph_alias, visited, frontiers[cur], frontiers[nxt], dists, steps, forked_first_cnt);
            }
            
            cur = 1 - cur;
            nxt = 1 - nxt;
          }
          
          
          if (debug) std::cout << "Rounds : " << steps << "; Forked = " << forked_first_cnt <<std::endl;
          delete(visited);
          return normalize(graph, dists);
        }  
      };


    };
       
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
