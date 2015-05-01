#include "base_algo.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"
#include <thread>
#include <unordered_map>
#include <stdio.h>
#include "bellman_ford.hpp"
#include <stdio.h>
#include <emmintrin.h>
#include <vector>
#include <array>
#include <bitset>
#include <queue>

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
      
      template <class T>
      struct mask {
      public:    
        mask() {}   
        
        mask(int size) {
          set_elements_num(size);
        }        
        ~mask() {
//          if (allocated) delete(masks);
        }
        void set_elements_num(int _elements_num) {
          bits_per_block = 8 * sizeof(T); 
          elements_num = _elements_num;
          masks_num = elements_num / bits_per_block;
          if (elements_num % bits_per_block != 0) masks_num++;
          
          masks = data::mynew_array<T>(masks_num);
          fill_array_seq(masks, masks_num, T(0));
          allocated = true;
        }
        
        void set_bit(int i) {
          masks[i / bits_per_block] |= (one << (i % bits_per_block));
        }
        
        int get_bit(int i) {
          T bit = masks[i / bits_per_block] & (one << (i % bits_per_block));
          return bit == 0 ? 0 : 1;
        }
        
        void print() {
          for (int i = 0; i < elements_num; ++i) {
            std::cout << get_bit(i);
          }
          std::cout << std::endl;
        }
        
        mask& operator &=(const mask& rhs) {
          for (int i = 0; i < masks_num; ++i) {
            masks[i] &= rhs.masks[i];
          }
          return *this; 
        }

        mask& operator |=(const mask& rhs) {
          for (int i = 0; i < masks_num; ++i) {
            masks[i] |= rhs.masks[i];
          }
          return *this; 
        }
        
        mask operator~() const {
          mask res(elements_num);
          for (int i = 0; i < masks_num; i++) {
            res.masks[i] = ~masks[i];
          }
          return res;
        }
        
        mask& operator=(mask other) {
          for (int i = 0; i < masks_num; ++i) {
            masks[i] = other.masks[i];
          }
          return *this;
        }

        
        bool allocated = false;
        T* masks;
        T one = T(1);
        int masks_num;
        int elements_num;
        int bits_per_block;
      };
      
      static inline int layer_index_for_vertex(int & vertex, int layer, int* dists, long long &big_vertex, int &nb_vertices) {
        int dist_to_vertex = dists[big_vertex * nb_vertices + vertex];
        return layer - dist_to_vertex + deep;
      }
      
      
      static inline bool check_index(int layer_index) {
        return 0 <= layer_index && layer_index < 2 * deep + 1;
      }
      
      template <class MasksClass>
      static inline void try_to_update_mask(MasksClass& masks, int from, int to, int &layer, int* dists, long long &big_vertex, int &nb_vertices) {
        int from_layer_index = layer_index_for_vertex(from, layer, dists, big_vertex, nb_vertices);
        int to_layer_index = layer_index_for_vertex(to, layer + 1, dists, big_vertex, nb_vertices);

        if (check_index(to_layer_index) && check_index(from_layer_index)) masks[to][to_layer_index] |= masks[from][from_layer_index];

      }
          
      template <class Adjlist_alias, class Frontier, class MasksClass>
      static void process_layer_par_lazy(const Adjlist_alias & graph_alias,
                                         Frontier& frontier,
                                         MasksClass& masks, 
                                         int & layer,
                                         int* dists, 
                                         long long &big_vertex,
                                         int &nb_vertices) {
        
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
              sched::native::fork2([&] { process_layer_par_lazy(graph_alias, frontier, masks, layer, dists, big_vertex, nb_vertices); },
                                   [&] { process_layer_par_lazy(graph_alias, fr_in, masks, layer, dists, big_vertex, nb_vertices); });                
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
            try_to_update_mask(masks, from, to, layer, dists, big_vertex, nb_vertices);
          });
          nb_outedges = frontier.nb_outedges();
        }
        if (blocked)
          bellman_ford_algo<Adjlist_seq>::bfs_bellman_ford::unblock();
      }
          
      static const int deep = 4;

      static void calc_for_big_vertex(long long big_vertex, const adjlist<Adjlist_seq>& graph, vtxid_type & nb_vertices, int* dists, int* handle_vertices, int handle_vertices_num) {
        int* num_vertices_at_level = data::mynew_array<int>(nb_vertices);
        int* vertex_id_at_level = data::mynew_array<int>(nb_vertices);
        int* level_offset = data::mynew_array<int>(nb_vertices);

        fill_array_seq(vertex_id_at_level, nb_vertices, 0);
        fill_array_seq(num_vertices_at_level, nb_vertices, 0);

        auto off = (long long) big_vertex * nb_vertices;
        int max_level = -1;
        for (int i = 0; i < nb_vertices; i++) {
          int val = dists[off + i];
          for (int j = val - deep; j <= val + deep; j++) {
            if (0 <= j && j < nb_vertices) {
              num_vertices_at_level[j]++;
              max_level = std::max(max_level, j);
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
        
       // for (int i = 0; i <= max_level; i++) {
       //   std::cout << "Vertices at level " << i << std::endl; 
       //   for (int j = 0; j < num_vertices_at_level[i]; j++) {
       //     std::cout << vertices_at_level[level_offset[i] + j] << " ";          
       //   }
       //   std::cout << std::endl;
       // }
        
        std::cout << "Initialization..." << std::endl; 
        auto graph_alias = get_alias_of_adjlist(graph);
        Frontier frontier(graph_alias);
        std::vector<std::array<mask<long long>, 2 * deep + 1> > masks(nb_vertices);
        std::vector<std::array<mask<long long>, 2 * deep + 1> > masks_calculated(nb_vertices);
        for (int i = 0; i < nb_vertices; ++i) {
          for (int j = -deep; j <= deep; j++) {
            masks[i][j + deep].set_elements_num(handle_vertices_num);
            masks_calculated[i][j + deep].set_elements_num(handle_vertices_num);
          }          
        }
        for (int i = 0; i < handle_vertices_num; i++) {
          int layer_vertex_index = layer_index_for_vertex(handle_vertices[i], 0, dists, big_vertex, nb_vertices);
          if (check_index(layer_vertex_index)) {
            masks[handle_vertices[i]][layer_vertex_index].set_bit(i);
            masks_calculated[handle_vertices[i]][layer_vertex_index].set_bit(i);            
          }
        }        

        for (int layer = 1; layer <= max_level; layer++) {
	  			std::cout << "Processing layer " << layer << std::endl;
          frontier.clear();
          int frontier_level = layer - 1;
          for (int i = level_offset[frontier_level]; i < level_offset[frontier_level] + num_vertices_at_level[frontier_level]; i++) {
            frontier.push_vertex_back(vertices_at_level[i]);
          }
          process_layer_par_lazy(graph_alias, frontier, masks, frontier_level, dists, big_vertex, nb_vertices);

          for (int i = level_offset[layer]; i < level_offset[layer] + num_vertices_at_level[layer]; i++) {
            int vertex = vertices_at_level[i];
            int layer_vertex_index = layer_index_for_vertex(vertex, layer, dists, big_vertex, nb_vertices);
            masks_calculated[vertex][layer_vertex_index] = masks[vertex][layer_vertex_index];   
            if (layer_vertex_index > 0) {
              masks[vertex][layer_vertex_index] &= (~masks_calculated[vertex][layer_vertex_index - 1]);
					    masks_calculated[vertex][layer_vertex_index] |= masks_calculated[vertex][layer_vertex_index - 1];
            }   
          }
        }     

        std::cout << "Finished processing" << std::endl;
        sched::native::parallel_for(0, nb_vertices, [&] (int i) {
          int dist_to_i = dists[big_vertex * nb_vertices + i];
          for (int j = std::max(-deep, -dist_to_i); j <= deep; ++j) {
            sched::native::parallel_for(0, handle_vertices_num, [&] (int k) {
              if (masks[i][j + deep].get_bit(k)) {
                long long from = handle_vertices[k];
                dists[from * nb_vertices + i] = dist_to_i + j;
              }              
            });
          };
        });
      }
      
      template <class Masks>
      static void print_info_for_layer(int layer, Masks & masks, Masks & masks_calculated, int * dists, long long big_vertex, int & nb_vertices) {
        std::cout << "Layer # " << layer << std::endl;
        for (int vertex = 0; vertex < nb_vertices; vertex++) {
          int layer_vertex_index = layer_index_for_vertex(vertex, layer, dists, big_vertex, nb_vertices);
          if (layer_vertex_index >= 2 * deep + 1 || layer_vertex_index < 0) {
            std::cout << "Vertex " << vertex << " not  presented at this level" << std::endl;
            continue;
          }
          std::cout << "Mask       of Vertex " << vertex << " = ";
          masks[vertex][layer_vertex_index].print();
          std::cout << "Calculated of Vertex " << vertex << " = ";
          masks_calculated[vertex][layer_vertex_index].print();
        }
      }
        
      static int build_near_vertex_set(int start_vertex, int* used, const adjlist<Adjlist_seq>& graph) {
        std::queue<int> q;
        q.push(start_vertex);
        used[start_vertex] = 0;
        int num = 0;
        while (!q.empty()) {
          int cur = q.front();
          if (used[cur] >= deep) break;
          q.pop();
          vtxid_type degree = graph.adjlists[cur].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[cur].get_out_neighbor(edge);
            if (used[other] == -1) {
              used[other] = used[cur] + 1;
              q.push(other);
              num++;
            }
          }
        }
        return num;
      }
      
      static int* floyd_warshall_par_bfs_opt(const adjlist<Adjlist_seq>& graph) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;        
        vtxid_type nb_vertices = graph.get_nb_vertices();
        
        int max_deg = -1, max_num = -1;
        for (int i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_in_degree();
          if (degree > max_deg) {
            max_deg = degree;
            max_num = i;
          }
        };
        int* used = data::mynew_array<int>(nb_vertices);        
        fill_array_seq(used, nb_vertices, -1);

        int num_handled = build_near_vertex_set(max_num, used, graph);
       	int num_calc = nb_vertices - num_handled;
        std::cout << "Num calc " << num_calc << std::endl;
        std::cout << "Num to be handled by big vertex " << nb_vertices - num_calc << std::endl;

        int* vertices_to_calc = data::mynew_array<int>(num_calc);
        int* vertices_to_handle_by_big = data::mynew_array<int>(nb_vertices - num_calc);
        int vertices_to_calc_id = 0, vertices_to_handle_by_big_id = 0;
        for (int i = 0; i < nb_vertices; i++) {
          if (used[i] == -1 || max_num == i) {
            vertices_to_calc[vertices_to_calc_id++] = i;
          } else {
            vertices_to_handle_by_big[vertices_to_handle_by_big_id++] = i;
          }
        }
        int* dists = data::mynew_array<int>((long long) nb_vertices * nb_vertices);
        process(0, num_calc, graph, nb_vertices, dists, vertices_to_calc);
        calc_for_big_vertex(max_num, graph, nb_vertices, dists, vertices_to_handle_by_big, nb_vertices - num_calc);
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
