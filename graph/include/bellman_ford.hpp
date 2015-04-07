
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
    } // end namespace util
    
    enum { 	
      SERIAL_CLASSIC,
      SERIAL_YEN,      
      SERIAL_BFS,
      SERIAL_BFS_SLOW,      
      PAR_NUM_VERTICES,
      PAR_NUM_EDGES,
      PAR_BFS,
      PAR_BFS2,
      PAR_COMBINED,
      NB_BF_ALGO 
    };      			
    
    std::string const algo_names[] = {
      "SerialClassic", 
      "SerialYen",       
      "SerialBFS", 
      "SerialBFSSlow", 
      "ParNumVertices", 
      "ParNumEdges", 
      "ParBFS", 
      "ParBFS2", 
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
      using edge_type = wedge<vtxid_type>;
      using edgelist_bag_type = pasl::data::array_seq<edge_type>;
      using edgelist_type = edgelist<edgelist_bag_type>;
      int inf_dist = shortest_path_constants<int>::inf_dist;
      vtxid_type nb_vertices = graph.get_nb_vertices();

      edgelist_type edg_plus;
      edgelist_type edg_minus;
      auto num_edges = graph.nb_edges;
      auto num_less = 0;
      for (size_t from = 0; from < nb_vertices; from++) {
        vtxid_type degree = graph.adjlists[from].get_out_degree();
        for (vtxid_type edge = 0; edge < degree; edge++) {
          vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
          if (from < to) num_less++;
        }
      }
      
      edg_plus.edges.alloc(num_less);
      edg_minus.edges.alloc(num_edges - num_less);
      auto num_plus = 0;
      auto num_minus = 0;
      
      
//      for (size_t from = 0; from < nb_vertices; from++) {
//        vtxid_type degree = graph.adjlists[from].get_out_degree();
//        for (vtxid_type edge = 0; edge < degree; edge++) {
//          vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
//          if (from < to) {
//            edg_plus.edges[num_plus] = edge_type(from, to, graph.adjlists[from].get_out_neighbor_weight(edge));
//
//          	num_less++;  
//          }
//        }
//      }
//      sched::native::parallel_for(edgeid_type(0), num_edges, [&] (edgeid_type i) {
//        dst.edges[i] = edge_type(0, vtxid_type(i + 1));
//      });
//      dst.nb_vertices = vtxid_type(nb_edges + 1);
//      dst.check();
      

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
      int* visited = data::mynew_array<int>(nb_vertices);   
      
      fill_array_seq(dists, nb_vertices, inf_dist);      
      fill_array_seq(visited, nb_vertices, -1);      
      dists[source] = 0; 
      
      LOG_BASIC(ALGO_PHASE);
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
      return util::normalize(graph, dists);
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; slow serial bfs */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq_bfs_slow(const adjlist<Adjlist_seq>& graph,
                              typename adjlist<Adjlist_seq>::vtxid_type source) {
      
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      int inf_dist = shortest_path_constants<int>::inf_dist;
      vtxid_type nb_vertices = graph.get_nb_vertices();
      int* dists = data::mynew_array<int>(nb_vertices);      
      fill_array_seq(dists, nb_vertices, inf_dist);      
      dists[source] = 0; 
      
      LOG_BASIC(ALGO_PHASE);
      std::queue<vtxid_type> cur;
      std::unordered_set<vtxid_type> next;
      cur.push(source);
      int steps = 0;
      double total_size = 0.;
      
      while (steps < nb_vertices && !cur.empty()) {
        steps++;
        if (steps > nb_vertices) break; 
        next.clear();
        total_size += cur.size();
        while (!cur.empty()) {
          vtxid_type from = cur.front();
          cur.pop();
          vtxid_type degree = graph.adjlists[from].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[from].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[from].get_out_neighbor_weight(edge);
            
            if (dists[other] > dists[from] + w) {
              next.insert(other);
            }
          }               
        }
        for (const vtxid_type& vertex: next) {
          cur.push(vertex);
          long degree = graph.adjlists[vertex].get_in_degree();
          for (int edge = 0; edge < degree; edge++) {
            long other = graph.adjlists[vertex].get_in_neighbor(edge);
            long w = graph.adjlists[vertex].get_in_neighbor_weight(edge);
            if (dists[vertex] > dists[other] + w) {
              dists[vertex] = dists[other] + w;
            }
          }

        }
        
      }
      std::cout << "Rounds : " << steps << "; Avg queue size : " << total_size / steps << std::endl;
      return util::normalize(graph, dists);
    }
    
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel by number of vertices */
    /*---------------------------------------------------------------------*/
    
    extern int bellman_ford_par_by_vertices_cutoff;
    
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
    
    extern int bellman_ford_par_by_edges_cutoff;
    
    void build_plan(std::unordered_map<long long, int> & mid_map, int start, int stop, int * pref_sum, long & vertex_num, int & forked_cnt) {
      int nb_edges = pref_sum[stop] - pref_sum[start];
      if (nb_edges >= bellman_ford_par_by_edges_cutoff && stop - start > 2) {
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
        mid_map[start * vertex_num + stop] = left;
        forked_cnt++;
        build_plan(mid_map, start, left, pref_sum, vertex_num, forked_cnt);
        build_plan(mid_map, left, stop, pref_sum, vertex_num, forked_cnt);        
      }
    }  

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
      std::cout << "Rounds : " << steps << std::endl;
      std::cout << "Forked per round : " << forked_cnt << std::endl;

      
      return util::normalize(graph, dists);
    }
    
    template <class Adjlist_seq>
    void process_par_by_edges(const adjlist<Adjlist_seq>& graph,
                               int * dists, int start, int stop, int * pref_sum, std::unordered_map<long long, int> & mid_map,  bool & changed, long & vertex_num) {
      int nb_edges = pref_sum[stop] - pref_sum[start];
      if (nb_edges < bellman_ford_par_by_edges_cutoff || stop - start <= 2) {
        process_vertices_seq(graph, dists, start, stop, changed);
      } else {
        int mid = mid_map[start * vertex_num + stop];
        sched::native::fork2([&] { process_par_by_edges(graph,  dists, start, mid, pref_sum, mid_map, changed, vertex_num); },
                             [&] { process_par_by_edges(graph,  dists, mid, stop, pref_sum, mid_map, changed, vertex_num); });        
      }
    }  
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel BFS-like */
    /*---------------------------------------------------------------------*/
        
    extern int bellman_ford_bfs_process_layer_cutoff;
    extern int bellman_ford_bfs_process_next_vertices_cutoff;    
    
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
      using measure_type = typename cache_type::measure_type;

      
      static bool try_to_set_visited(vtxid_type & target, int & layer, std::atomic<int>* visited) {
        int cur_d = visited[target].load(std::memory_order_relaxed);
        if (cur_d == layer)
          return false;
        return visited[target].compare_exchange_strong(cur_d, layer);
      }
      
      template <class Adjlist_alias, class Frontier, class WeightedSeq>
      static void process_layer_par(const Adjlist_alias & graph_alias,
                                 std::atomic<int>* visited,
                                 Frontier& prev,
                                 Frontier& next,
                                 WeightedSeq& next_vertices,
                                 int * dists,
                                 int & layer, 
                                 measure_type & meas,   
                                std::atomic<int> & forked_first_cnt) {
        if (prev.nb_outedges() <= vtxid_type(bellman_ford_bfs_process_layer_cutoff)) {
          prev.for_each_outedge([&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
            if (dists[to] > dists[from] + weight) {
              if (try_to_set_visited(to, layer, visited)) {
                next.push_vertex_back(to);
                next_vertices.push_back(to);
              }
            }
          });          
          prev.clear();
        } else {
          Frontier fr_in;
          Frontier fr_out;
          fr_in.set_graph(graph_alias);
          fr_out.set_graph(graph_alias);
          WeightedSeq ver_out;
          ver_out.set_measure(meas);
          prev.split(prev.nb_outedges() / 2, fr_in);
          forked_first_cnt++;
          sched::native::fork2([&] { process_layer_par(graph_alias, visited, prev, next, next_vertices, dists, layer, meas, forked_first_cnt); },
                               [&] { process_layer_par(graph_alias, visited, fr_in, fr_out, ver_out, dists, layer, meas, forked_first_cnt); });
          next.concat(fr_out);
					next_vertices.concat(ver_out);
        }
      }
      
      template <class Adjlist, class WeightedSeq>
      static void process_next_vert(Adjlist&  graph,
                             WeightedSeq& next_vertices,
                             int * dists, 
                             std::atomic<int> & forked_second_cnt) {
        using vtxid_type = typename Adjlist::vtxid_type;
        if (next_vertices.get_cached() < bellman_ford_bfs_process_next_vertices_cutoff) {
          next_vertices.for_each([&] (vtxid_type vertex) {
            long degree = graph.adjlists[vertex].get_in_degree();
            for (int edge = 0; edge < degree; edge++) {
              auto from = graph.adjlists[vertex].get_in_neighbor(edge);
              auto w = graph.adjlists[vertex].get_in_neighbor_weight(edge);
              if (dists[vertex] > dists[from] + w) {
                dists[vertex] = dists[from] + w;
              }
            }
          });
        } else {
          forked_second_cnt++;
          WeightedSeq other;
          auto nb = next_vertices.get_cached() / 2;
          next_vertices.split([nb] (vtxid_type n) { return nb <= n; }, other);
          sched::native::fork2([&] { process_next_vert(graph, next_vertices, dists, forked_second_cnt); },
                               [&] { process_next_vert(graph, other, dists, forked_second_cnt); });
          
        }
      }

      static edgeweight_type* bellman_ford_par_bfs(const adjlist<Adjlist_seq>& graph,
                             typename adjlist<Adjlist_seq>::vtxid_type source) {
        auto inf_dist = shortest_path_constants<edgeweight_type>::inf_dist;

        vtxid_type nb_vertices = graph.get_nb_vertices();
        edgeweight_type* dists = data::mynew_array<edgeweight_type>(nb_vertices);
        std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
        int unknown = 0;
        fill_array_par(visited, nb_vertices, unknown);

        fill_array_seq(dists, nb_vertices, inf_dist);
        dists[source] = 0;

        
        LOG_BASIC(ALGO_PHASE);
        auto graph_alias = get_alias_of_adjlist(graph);
        Frontier frontiers[2];
        seq_type next_vertices;
        
        graph_env_help env(graph_alias);
        measure_type meas(env);
        next_vertices.set_measure(meas);
        
        frontiers[0].set_graph(graph_alias);
        frontiers[1].set_graph(graph_alias);
        vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
        vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
        frontiers[0].push_vertex_back(source);
        
        int steps = 0;
        std::atomic<int> forked_first_cnt;
        std::atomic<int> forked_second_cnt;
        
        forked_first_cnt.store(0);
        forked_second_cnt.store(0);
        
        while (! frontiers[cur].empty()) {
          next_vertices.clear();
          steps++;
          if (steps > nb_vertices) break;
          if (frontiers[cur].nb_outedges() <= bellman_ford_bfs_process_layer_cutoff) {
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
            self_type::process_layer_par(graph_alias, visited, frontiers[cur], frontiers[nxt], next_vertices, dists, steps, meas, forked_first_cnt);
            self_type::process_next_vert(graph, next_vertices, dists, forked_second_cnt);
          }
          // TODO: optimize for "if (frontiers[cur].nb_outedges() <= bellman_ford_bfs_process_layer_cutoff) {

          cur = 1 - cur;
          nxt = 1 - nxt;
        }
        
        
        std::cout << "Rounds : " << steps << "; ForkedFirst = " << forked_first_cnt << "; ForkedSecond = " << forked_second_cnt <<std::endl;
        delete(visited);
        return util::normalize(graph, dists);
      }            
    };
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel BFS-like SECOND VERSION */
    /*---------------------------------------------------------------------*/
    
    extern int bellman_ford_bfs_process_layer_cutoff;
    const int communicate_cutoff = 256;

    template <class Adjlist_seq>
    class bfs_bellman_ford2 {
    public:

      
      using self_type = bfs_bellman_ford2;
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
      static void process_layer_par(const Adjlist_alias & graph_alias,
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
          if (nb_outedges <= bellman_ford_bfs_process_layer_cutoff) {
            blocked = true;
            reject();
          }
          if (should_call_communicate()) {
            if (nb_outedges > bellman_ford_bfs_process_layer_cutoff) {
              Frontier fr_in(graph_alias);
              Frontier fr_out(graph_alias);
              prev.split(prev.nb_outedges() / 2, fr_in);
              forked_first_cnt++;
              sched::native::fork2([&] { process_layer_par(graph_alias, visited, prev, next, dists, layer, forked_first_cnt); },
                                   [&] { process_layer_par(graph_alias, visited, fr_in, fr_out, dists, layer, forked_first_cnt); });
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

      
      static edgeweight_type* bellman_ford_par_bfs(const adjlist<Adjlist_seq>& graph,
                                                   typename adjlist<Adjlist_seq>::vtxid_type source) {
        auto inf_dist = shortest_path_constants<edgeweight_type>::inf_dist;
        
        vtxid_type nb_vertices = graph.get_nb_vertices();
        edgeweight_type* dists = data::mynew_array<edgeweight_type>(nb_vertices);
        std::atomic<int>* visited = data::mynew_array<std::atomic<int>>(nb_vertices);
        int unknown = 0;
        fill_array_par(visited, nb_vertices, unknown);
        
        fill_array_seq(dists, nb_vertices, inf_dist);
        dists[source] = 0;
        
        
        LOG_BASIC(ALGO_PHASE);
        auto graph_alias = get_alias_of_adjlist(graph);
        Frontier frontiers[2];
                
        frontiers[0].set_graph(graph_alias);
        frontiers[1].set_graph(graph_alias);
        vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
        vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
        frontiers[0].push_vertex_back(source);
        
        int steps = 0;
        std::atomic<int> forked_first_cnt;
        std::atomic<int> forked_second_cnt;
        
        forked_first_cnt.store(0);
        forked_second_cnt.store(0);
        
        while (! frontiers[cur].empty()) {
          steps++;
          if (steps > nb_vertices) break;
          if (frontiers[cur].nb_outedges() <= bellman_ford_bfs_process_layer_cutoff) {
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
            self_type::process_layer_par(graph_alias, visited, frontiers[cur], frontiers[nxt], dists, steps, forked_first_cnt);
          }
          
          cur = 1 - cur;
          nxt = 1 - nxt;
        }
        
        
        std::cout << "Rounds : " << steps << "; ForkedFirst = " << forked_first_cnt << "; ForkedSecond = " << forked_second_cnt <<std::endl;
        delete(visited);
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
