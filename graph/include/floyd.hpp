#include "base_algo.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>
#include <unordered_map>
#include <stdio.h>
#include "bellman_ford.hpp"

#ifndef _PASL_GRAPH_FLOYD_H_
#define _PASL_GRAPH_FLOYD_H_

/***********************************************************************/

namespace pasl {
  namespace graph {
    extern int floyd_warshall_par_serial_cutoff;
    extern int floyd_warshall_par_bfs_cutoff;
    
    template <class Adjlist_seq>
    class floyd_algo: public base_algo<Adjlist_seq> {
    public:
      
      std::string get_impl_name(int index) {
        return fw_algo_names[index];
      }
      
      int get_impl_count() {
        return FW_NB_ALGO;
      }
      
      void print_res(int* res, int vertices, std::ofstream& to) {
        for (int i = 0; i < vertices; i++) {
          to << "Distances from " << i << " vertex \n";
          for (int j = 0; j < vertices; j++) {
            to << j << " = " << res[(long long) i * vertices + j] << "\n";            
          }
        }
      }
      
      int* get_dist(int algo_id, const adjlist<Adjlist_seq>& graph, int source) {
        switch (algo_id) {
          case FW_SERIAL_CLASSIC:
            return floyd_seq_classic(graph);
            break;
          case FW_PAR_CLASSIC:
            return floyd_par_classic(graph);
            break;
          case FW_PAR_BFS:
            return floyd_warshall_par_bfs(graph);
            break;
          case FW_PAR_BFS_OPT:
            return floyd_warshall_par_bfs_opt(graph);
            break;
          case FW_PAR_BFS2:
            return floyd_warshall_par_bfs2(graph);
            break;
          default:
            return floyd_seq_classic(graph);
        }
      }
      
    private:
      using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
      using adjlist_type = adjlist<Adjlist_seq>;
      using adjlist_alias_type = typename adjlist_type::alias_type;      
      using Frontier = pasl::graph::frontiersegbag<adjlist_alias_type>;

      enum FW_ALGO { 	
        FW_SERIAL_CLASSIC,
        FW_PAR_CLASSIC,      
        FW_PAR_BFS,
        FW_PAR_BFS_OPT,      
        FW_PAR_BFS2,        
        FW_NB_ALGO 
      };   
      
      std::string const fw_algo_names[5] = {
        "SerialClassic", 
        "ParClassic",      
        "ParBFSForAllVertices",
        "ParBFSForAllVertices (with big vertex optimization)",
        "ParBFSForEveryVertex"
      };    
      
      static void print_dist(int nb_vertices, int * dist) {
        std::cout << "Distances : " << std::endl;
        int cur = 0;
        for (int i = 0; i < nb_vertices; i++) {
          for (int j = 0; j < nb_vertices; j++) {
            std::cout << dist[cur++] << " ";
          }          
          std::cout << std::endl;
        }
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; serial classic */
      /*---------------------------------------------------------------------*/
      int* floyd_seq_classic(const adjlist<Adjlist_seq>& graph) {
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices * nb_vertices);
        fill_array_seq(dists, nb_vertices * nb_vertices, inf_dist);
        
        for (size_t i = 0; i < nb_vertices; i++) {
          dists[i * nb_vertices + i] = 0;
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            dists[i * nb_vertices + other] = w;
          }
        }
        
        for (int k = 0; k < nb_vertices; k++) {
          for (int i = 0; i < nb_vertices; i++) {
            for (int j = 0; j < nb_vertices; j++) {
              dists[i * nb_vertices + j] = std::min(dists[i * nb_vertices + j], 
                                                    dists[i * nb_vertices + k] + dists[k * nb_vertices + j]);
            }
          }
          
        }
        return dists;
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel classic */
      /*---------------------------------------------------------------------*/
      int* floyd_par_classic(const adjlist<Adjlist_seq>& graph) {
        return floyd_seq_classic(graph);
      }
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /* One bfs for all the vertices */      
      /*---------------------------------------------------------------------*/
      static adjlist<Adjlist_seq> modify_graph(const adjlist<Adjlist_seq>& graph, int dist_root_vertices) {
        adjlist<Adjlist_seq> adj;        
        auto nb_vertices = (long long) dist_root_vertices * graph.get_nb_vertices();
        auto nb_offsets = nb_vertices + 1;
        auto nb_edges = (long long) graph.nb_edges * dist_root_vertices;
        auto contents_sz = nb_offsets + nb_edges * 2;
        char* contents = (char*)data::mynew_array<vtxid_type>(contents_sz);
        char* contents_in = (char*)data::mynew_array<vtxid_type>(contents_sz);
        
        adj.adjlists.init(contents, contents_in, nb_vertices, nb_edges);
        vtxid_type* offsets = adj.adjlists.offsets;
        vtxid_type* offsets_in = adj.adjlists.offsets_in;        
        vtxid_type* edges = adj.adjlists.edges;
        vtxid_type* edges_in = adj.adjlists.edges_in;
        vtxid_type init_nb_vertices = graph.get_nb_vertices();
        
        sched::native::parallel_for(0, dist_root_vertices, [&] (int i) {
          const int cur_offset = i * graph.adjlists.offsets[init_nb_vertices];    
          const int cur_offset_in = i * graph.adjlists.offsets_in[init_nb_vertices];
          const int cur_id = i * init_nb_vertices;

          sched::native::parallel_for(0, init_nb_vertices, [&] (int j) {
            // offsets
            offsets[cur_id + j] = cur_offset + graph.adjlists.offsets[j];
            offsets_in[cur_id + j] = cur_offset_in + graph.adjlists.offsets_in[j];            
          });
        });
        offsets[nb_offsets - 1] = dist_root_vertices * graph.adjlists.offsets[init_nb_vertices];
        offsets_in[nb_offsets - 1] = dist_root_vertices * graph.adjlists.offsets_in[init_nb_vertices];  
        
        
        sched::native::parallel_for(0, dist_root_vertices, [&] (int i) {
          const int off = i * init_nb_vertices;
          const int cur_id = off;
          sched::native::parallel_for(0, init_nb_vertices, [&] (int j) {
            // edges
            int start = offsets[cur_id + j], num = offsets[cur_id + j + 1] - offsets[cur_id + j];
						memcpy(edges + start, graph.adjlists.edges + graph.adjlists.offsets[j], sizeof(vtxid_type) * num);            
            for (int k = 0; k < num / 2; k++) {
              int to = edges[start + k];
              edges[start + k] += off;
            }
            start = offsets_in[cur_id + j], num = offsets_in[cur_id + j + 1] - offsets_in[cur_id + j];
            memcpy(edges_in + start, graph.adjlists.edges_in + graph.adjlists.offsets_in[j], sizeof(vtxid_type) * num);            
            for (int k = 0; k < num / 2; k++) {
              edges_in[start + k] += off;
            }

          });
        });
        adj.nb_edges = nb_edges;
        return adj;
      }
      
      static int* floyd_warshall_par_bfs(const adjlist<Adjlist_seq>& init_graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();
        auto graph = modify_graph(init_graph, nb_vertices);
        std::cout << "Finished modifiyng graph" << std::endl;
        std::vector<vtxid_type> sources;
        for (int i = 0; i < nb_vertices; ++i) {
          sources.push_back(i * nb_vertices + i);
        }
        return bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::bellman_ford_par_bfs(graph, sources);
      }   
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /* with big vertex optimization (WARNING: ONLY FOR UNDIRECTED UNWEIGHTED GRAPH) */      
      /*---------------------------------------------------------------------*/
      
      static inline bool try_to_update_mask() {
        // TODO: write it
        return true;
      }
          
      template <class Adjlist_alias, class Frontier>
      static void process_layer_par_lazy(const Adjlist_alias & graph_alias,
                                         Frontier& frontier) {
        
        vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
        size_t nb_outedges = frontier.nb_outedges();
        bool blocked = false;
        while (nb_outedges > 0) {
          if (nb_outedges <= bellman_ford_par_bfs_cutoff) {
            blocked = true;
            bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::reject();
          }
          if (bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::should_call_communicate()) {
            if (nb_outedges > bellman_ford_par_bfs_cutoff) {
              Frontier fr_in(graph_alias);
              frontier.split(frontier.nb_outedges() / 2, fr_in);
              sched::native::fork2([&] { process_layer_par_lazy(graph_alias, frontier); },
                                   [&] { process_layer_par_lazy(graph_alias, fr_in); });                
              if (blocked) // should always be false due to the order of the conditionals; yet, keep it for safety
                bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::unblock(); 
              return;
            }
            else
            {
              blocked = true;
              bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::reject();
            }
          }
          frontier.for_at_most_nb_outedges(communicate_cutoff, [&] (vtxid_type from, vtxid_type to, vtxid_type weight) {
            // TODO: update mask
          });
          nb_outedges = frontier.nb_outedges();
        }
        if (blocked)
          bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::unblock();
      }
          
      static const int deep = 1;

      static void calc_for_big_vertex(int big_vertex, const adjlist<Adjlist_seq>& graph, vtxid_type & nb_vertices, int* dists) {
        int* num_vertices_at_level = data::mynew_array<int>(nb_vertices);
        int* vertex_id_at_level = data::mynew_array<int>(nb_vertices);
        int* level_offset = data::mynew_array<int>(nb_vertices);

        fill_array_seq(vertex_id_at_level, nb_vertices, 0);
        fill_array_seq(num_vertices_at_level, nb_vertices, 0);

        const int off = big_vertex * nb_vertices;
        for (int i = 0; i < nb_vertices; i++) {
          int val = dists[off + i];
          for (int j = val - deep; j <= val + deep; j++) {
            if (0 <= j && j < nb_vertices) {
              num_vertices_at_level[j]++;
            }
          }
        }
        level_offset[0] = 0;
        for (int i = 1; i < nb_vertices; i++) {
          level_offset[i] = level_offset[i - 1] + num_vertices_at_level[i - 1];
        }
        int* vertices_at_level = data::mynew_array<int>(level_offset[nb_vertices - 1] + num_vertices_at_level[nb_vertices - 1]);
        for (int i = 0; i < nb_vertices; i++) {
          int val = dists[off + i];
          for (int j = val - deep; j <= val + deep; j++) {
            if (0 <= j && j < nb_vertices) {            
              vertices_at_level[level_offset[j] + vertex_id_at_level[j]] = i;
              vertex_id_at_level[j]++;            
            }
          }
        }
        
        for (int i = 0; i < nb_vertices; i++) {
          std::cout << "Vertices at level " << i << std::endl; 
          for (int j = 0; j < num_vertices_at_level[i]; j++) {
            std::cout << vertices_at_level[level_offset[i] + j] << " ";          
          }
          std::cout << std::endl;
        }
        
        auto graph_alias = get_alias_of_adjlist(graph);
        Frontier frontier(graph_alias);
        for (int layer = 0; layer < nb_vertices; layer++) {
          frontier.clear();
          for (int i = level_offset[layer]; i < level_offset[layer] + num_vertices_at_level[layer]; i++) {
            frontier.push_vertex_back(vertices_at_level[i]);
          }
          process_layer_par_lazy(graph_alias, frontier);
        }      
      }
        
      
      
      static int* floyd_warshall_par_bfs_opt(const adjlist<Adjlist_seq>& graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        
        vtxid_type nb_vertices = graph.get_nb_vertices();
        vtxid_type nb_edges = graph.nb_edges;
        
        int max_deg = -1, max_num = -1;
        for (int i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_in_degree();
          if (degree > max_deg) {
            max_deg = degree;
            max_num = i;
          }
        };
        bool* used = data::mynew_array<bool>(nb_vertices);
        fill_array_seq(used, nb_vertices, false);
        vtxid_type degree = graph.adjlists[max_num].get_in_degree();
        for (vtxid_type edge = 0; edge < degree; edge++) {
          vtxid_type other = graph.adjlists[max_num].get_in_neighbor(edge);
          used[other] = true;
        }
       	int num_calc = nb_vertices - max_deg;
        std::cout << "Num calc " << num_calc << std::endl;
        int* vertices_to_calc = data::mynew_array<int>(num_calc);
        int vertices_to_calc_id = 0;
        for (int i = 0; i < nb_vertices; i++) {
          if (!used[i]) {
            vertices_to_calc[vertices_to_calc_id++] = i;
          }               
        }
        int* dists = data::mynew_array<int>((long long) nb_vertices * nb_vertices);
        process(0, num_calc, graph, nb_vertices, dists, vertices_to_calc);
        calc_for_big_vertex(max_num, graph, nb_vertices, dists);
        return dists;
      }  
     
      
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /*---------------------------------------------------------------------*/
      /* Floyd-Warshall; parallel bfs */
      /* Bfs for every vertex */      
      /*---------------------------------------------------------------------*/          
      static int* floyd_warshall_par_bfs2(const adjlist<Adjlist_seq>& init_graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();
        int* dists = data::mynew_array<int>((long long) nb_vertices * nb_vertices);
        process(0, nb_vertices, init_graph, nb_vertices, dists);
        return dists;
      } 
      
      static void process(int index_from, int index_to, const adjlist<Adjlist_seq>& graph, vtxid_type & nb_vertices, int* dists, int * vertices_to_calc = nullptr) {
        if (index_to - index_from < 1000) {
          for (int i = index_from; i < index_to; ++i) {
            int cur_vertex = i;
            if (vertices_to_calc != nullptr) cur_vertex = vertices_to_calc[i];
            
            bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::bellman_ford_par_bfs(graph, cur_vertex, false, dists + (long long) cur_vertex * nb_vertices);             
          }
        } else {
          sched::native::fork2([&] { process(index_from, (index_from + index_to) / 2, graph, nb_vertices, dists); },
                               [&] { process((index_from + index_to) / 2, index_to, graph, nb_vertices, dists); });
        }
        
      }
      
    };
    
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
