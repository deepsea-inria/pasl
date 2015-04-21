#include "base_algo.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>
#include <unordered_map>
#include <stdio.h>

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
          case FW_PAR_BFSmemory:
            return floyd_warshall_par_bfs_memory_opt(graph);
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

      enum FW_ALGO { 	
        FW_SERIAL_CLASSIC,
        FW_PAR_CLASSIC,      
        FW_PAR_BFS,
        FW_PAR_BFSmemory,      
        FW_PAR_BFS2,        
        FW_NB_ALGO 
      };   
      
      std::string const fw_algo_names[5] = {
        "SerialClassic", 
        "ParClassic",      
        "ParBFSForAllVertices",
        "ParBFSForAllVertices (Memory Optimized)",
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
      static adjlist<Adjlist_seq> modify_graph(const adjlist<Adjlist_seq>& graph) {
        adjlist<Adjlist_seq> adj;
        
        auto nb_vertices = (long long) graph.get_nb_vertices() * graph.get_nb_vertices();
        auto nb_offsets = nb_vertices + 1;
        auto nb_edges = (long long) graph.nb_edges * graph.get_nb_vertices();
        auto contents_sz = nb_offsets + nb_edges * 2;
        char* contents = (char*)data::mynew_array<vtxid_type>(contents_sz);
        char* contents_in = (char*)data::mynew_array<vtxid_type>(contents_sz);
        
        adj.adjlists.init(contents, contents_in, nb_vertices, nb_edges);
        vtxid_type* offsets = adj.adjlists.offsets;
        vtxid_type* offsets_in = adj.adjlists.offsets_in;        
        vtxid_type* edges = adj.adjlists.edges;
        vtxid_type* edges_in = adj.adjlists.edges_in;
        vtxid_type init_nb_vertices = graph.get_nb_vertices();
        
        int cur_offset = 0, cur_offset_in = 0, cur_id = 0;
        for (int i = 0; i < init_nb_vertices; ++i) {          
          sched::native::parallel_for(0, init_nb_vertices, [&] (int j) {
            // offsets
            offsets[cur_id + j] = cur_offset + graph.adjlists.offsets[j];
            offsets_in[cur_id + j] = cur_offset_in + graph.adjlists.offsets_in[j];            
          });
          cur_offset += graph.adjlists.offsets[init_nb_vertices];    
          cur_offset_in += graph.adjlists.offsets_in[init_nb_vertices];
          cur_id += init_nb_vertices;
        }
        offsets[nb_offsets - 1] = cur_offset;
        offsets_in[nb_offsets - 1] = cur_offset_in;  
        
        
        cur_id = 0;
        for (int i = 0; i < init_nb_vertices; ++i) {
          const int off = i * init_nb_vertices;
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
          cur_id += init_nb_vertices;
        }
        adj.nb_edges = nb_edges;
        return adj;
      }
      
      static int* floyd_warshall_par_bfs(const adjlist<Adjlist_seq>& init_graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();
        auto graph = modify_graph(init_graph);
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
      /* One bfs for all the vertices */      
      /*---------------------------------------------------------------------*/
      static int* floyd_warshall_par_bfs_memory_opt(const adjlist<Adjlist_seq>& init_graph) {
        // TODO : write it
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        vtxid_type nb_vertices = init_graph.get_nb_vertices();        
        return floyd_warshall_par_bfs(init_graph);
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
        long long cur_off = 0;
        for (int i = 0; i < nb_vertices; ++i) {
          bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::bellman_ford_par_bfs(init_graph, i, false, dists + cur_off);
          cur_off += nb_vertices;
        }
        return dists;
      }  
      
    };
    
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
