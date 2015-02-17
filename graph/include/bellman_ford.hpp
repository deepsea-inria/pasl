
#include "edgelist.hpp"
#include "adjlist.hpp"
#include "pcontainer.hpp"

#ifndef _PASL_GRAPH_BELLMAN_FORD_H_
#define _PASL_GRAPH_BELLMAN_FORD_H_

#define PUSH_ZERO_ARITY_VERTICES 0

/***********************************************************************/

namespace pasl {
namespace graph {

    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; serial */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_seq(const adjlist<Adjlist_seq>& graph,
                            typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);

        dists[source] = 0;
        
        LOG_BASIC(ALGO_PHASE);
        for (size_t step = 0; step < nb_vertices; step++) {
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
        return dists;
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel 1 */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_par1(const adjlist<Adjlist_seq>& graph,
                          typename adjlist<Adjlist_seq>::vtxid_type source) {
        int inf_dist = shortest_path_constants<int>::inf_dist;
        long nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);
        
        fill_array_seq(dists, nb_vertices, inf_dist);
        
        dists[source] = 0;
        
        LOG_BASIC(ALGO_PHASE);
        bool changed = false;

        for (int i = 0; i < nb_vertices; i++) {
            changed = false;
            process_vertices_par1(graph, dists, 0, nb_vertices, changed);
            if (!changed) break;
        }
        return dists;
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
    void process_vertices_par1(const adjlist<Adjlist_seq>& graph,
                            int* dists, int start, int stop, bool& changed) {
        int nb = stop - start;
        if (nb < 3) {
            process_vertices_seq(graph, dists, start, stop, changed);
        } else {
            int mid = (start + stop) / 2;
            sched::native::fork2([&] { process_vertices_par1(graph,  dists, start, mid, changed); },
                                 [&] { process_vertices_par1(graph,  dists, mid, stop, changed); });
            
        }
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel 2 */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_par2(const adjlist<Adjlist_seq>& graph,
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
        
        for (int i = 0; i < nb_vertices; i++) process_vertices_par2(graph, dists, 0, nb_vertices, pref_sum);
        return dists;
    }
    
    template <class Adjlist_seq>
    void process_vertices_par2(const adjlist<Adjlist_seq>& graph,
                               int * dists, int start, int stop, int * pref_sum) {
        int nb_edges = pref_sum[stop] - pref_sum[start];
        if (nb_edges < 1000000) {
            process_vertices_seq(graph, dists, start, stop);
        } else {
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
            
            sched::native::fork2([&] { process_vertices_par2(graph,  dists, start, left, pref_sum); },
                                 [&] { process_vertices_par2(graph,  dists, left, stop, pref_sum); });
            
        }
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel 3 */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_par3(const adjlist<Adjlist_seq>& graph,
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
        
        for (int i = 0; i < nb_vertices; i++) process_vertices_par3(graph, dists, 0, nb_vertices, pref_sum);
        return dists;
    }
    
    template <class Adjlist_seq>
    void process_vertices_par3(const adjlist<Adjlist_seq>& graph,
                               int * dists, int start, int stop, int * pref_sum) {
        int nb_edges = pref_sum[stop] - pref_sum[start];
        if (nb_edges < 1000000) {
            if (start - stop == 1) {
                int d;
                process_vertex_par(graph, start, 0, graph.adjlists[start].get_in_degree(), &d);
                dists[start] = d;
            } else {
                process_vertices_seq(graph, dists, start, stop);
            }
        } else {
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
            
            sched::native::fork2([&] { process_vertices_par3(graph,  dists, start, left, pref_sum); },
                                 [&] { process_vertices_par3(graph,  dists, left, stop, pref_sum); });
            
        }
    }
    
    template <class Adjlist_seq>
    void process_vertex_par(const adjlist<Adjlist_seq>& graph,
                            int vertex, int start, int stop, int * d) {
        int nb_edges = stop - start;
        if (nb_edges < 1000) {
            
        } else {
            
        }
    }
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Breadth-first search of a graph in adjacency-list format; parallel */
    
    // Parallel BFS using Shardl and Leiserson's algorithm:
    // Process each frontier by doing a parallel_for on the vertex ids
    // from the frontier, and handling the edges of each vertex using
    // a parallel_for.
    
    extern int ls_pbfs_cutoff;
    extern int ls_pbfs_loop_cutoff;
    
    template <bool idempotent = false>
    class ls_pbfs {
    public:
        
        using self_type = ls_pbfs<idempotent>;
        using idempotent_ls_pbfs = ls_pbfs<true>;
        
        //! \todo: move this to a common namespace
        template <class Index, class Item>
        static bool try_to_set_dist(Index target,
                                    Item unknown, Item dist,
                                    std::atomic<Item>* dists) {
            if (dists[target].load(std::memory_order_relaxed) != unknown)
                return false;
            if (idempotent)
                dists[target].store(dist, std::memory_order_relaxed);
            else if (! dists[target].compare_exchange_strong(unknown, dist))
                return false;
            return true;
        }
        
        template <class Adjlist_seq, class Frontier>
        static void process_layer(const adjlist<Adjlist_seq>& graph,
                                  std::atomic<typename adjlist<Adjlist_seq>::vtxid_type>* dists,
                                  typename adjlist<Adjlist_seq>::vtxid_type dist_of_next,
                                  typename adjlist<Adjlist_seq>::vtxid_type source,
                                  Frontier& prev,
                                  Frontier& next) {
            using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
            using size_type = typename Frontier::size_type;
            vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
            auto cutoff = [] (Frontier& f) {
                return f.size() <= vtxid_type(ls_pbfs_cutoff);
            };
            auto split = [] (Frontier& src, Frontier& dst) {
                src.split_approximate(dst);
            };
            auto append = [] (Frontier& src, Frontier& dst) {
                src.concat(dst);
            };
            sched::native::forkjoin(prev, next, cutoff, split, append, [&] (Frontier& prev, Frontier& next) {
                prev.for_each([&] (vtxid_type vertex) {
                    vtxid_type degree = graph.adjlists[vertex].get_out_degree();
                    vtxid_type* neighbors = graph.adjlists[vertex].get_out_neighbors();
                    data::pcontainer::combine(vtxid_type(0), degree, next, [&] (vtxid_type edge, Frontier& next) {
                        vtxid_type other = neighbors[edge];
                        if (try_to_set_dist(other, unknown, dist_of_next, dists)) {
                            if (PUSH_ZERO_ARITY_VERTICES || graph.adjlists[other].get_out_degree() > 0)
                                next.push_back(other);
                        }
                    }, ls_pbfs_loop_cutoff);
                });
                prev.clear();
            });
        }
        
        template <class Adjlist_seq, class Frontier>
        static std::atomic<typename adjlist<Adjlist_seq>::vtxid_type>*
        main(const adjlist<Adjlist_seq>& graph,
             typename adjlist<Adjlist_seq>::vtxid_type source) {
#ifdef GRAPH_SEARCH_STATS
            peak_frontier_size = 0l;
#endif
            using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
            using size_type = typename Frontier::size_type;
            vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
            vtxid_type nb_vertices = graph.get_nb_vertices();
            std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
            fill_array_par(dists, nb_vertices, unknown);
            LOG_BASIC(ALGO_PHASE);
            Frontier prev;
            Frontier next;
            vtxid_type dist = 0;
            prev.push_back(source);
            dists[source].store(dist);
            while (! prev.empty()) {
                dist++;
                if (prev.size() <= ls_pbfs_cutoff)
                    idempotent_ls_pbfs::process_layer(graph, dists, dist, source, prev, next);
                else
                    self_type::process_layer(graph, dists, dist, source, prev, next);
                prev.swap(next);
#ifdef GRAPH_SEARCH_STATS
                peak_frontier_size = std::max((size_t)prev.size(), peak_frontier_size);
#endif
            }
            return dists;
        }
        
    };


    /*---------------------------------------------------------------------*/
    
    // Parallel BFS using our frontier-segment-based algorithm:
    // Process each frontier by doing a parallel_for on the set of
    // outgoing edges, which is represented using our "frontier" data
    // structures that supports splitting according to the nb of edges.
    
    int our_bfs_cutoff = 8;
    
    template <bool idempotent = false>
    class our_bfs {
    public:
        
        using self_type = our_bfs<idempotent>;
        using idempotent_our_bfs = our_bfs<true>;
        
        template <class Adjlist_alias, class Frontier>
        static void process_layer(Adjlist_alias graph_alias,
                                  std::atomic<typename Adjlist_alias::vtxid_type>* dists,
                                  typename Adjlist_alias::vtxid_type& dist_of_next,
                                  typename Adjlist_alias::vtxid_type source,
                                  Frontier& prev,
                                  Frontier& next) {
            using vtxid_type = typename Adjlist_alias::vtxid_type;
            using size_type = typename Frontier::size_type;
            using edgelist_type = typename Frontier::edgelist_type;
            vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
            auto cutoff = [] (Frontier& f) {
                return f.nb_outedges() <= vtxid_type(our_bfs_cutoff);
            };
            auto split = [] (Frontier& src, Frontier& dst) {
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
                                        prev.for_each_outedge([&] (vtxid_type other) {
                                            if (ls_pbfs<idempotent>::try_to_set_dist(other, unknown, dist_of_next, dists))
                                                next.push_vertex_back(other);
                                            // warning: always does DONT_PUSH_ZERO_ARITY_VERTICES
                                        });
                                        prev.clear();
                                    });
        }
        
        template <class Adjlist, class Frontier>
        static std::atomic<typename Adjlist::vtxid_type>*
        main(const Adjlist& graph,
             typename Adjlist::vtxid_type source) {
            using vtxid_type = typename Adjlist::vtxid_type;
            using size_type = typename Frontier::size_type;
            vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
            vtxid_type nb_vertices = graph.get_nb_vertices();
            std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
            fill_array_par(dists, nb_vertices, unknown);
            LOG_BASIC(ALGO_PHASE);
            auto graph_alias = get_alias_of_adjlist(graph);
            vtxid_type dist = 0;
            dists[source].store(dist);
            Frontier frontiers[2];
            frontiers[0].set_graph(graph_alias);
            frontiers[1].set_graph(graph_alias);
            vtxid_type cur = 0; // either 0 or 1, depending on parity of dist
            vtxid_type nxt = 1; // either 1 or 0, depending on parity of dist
            frontiers[0].push_vertex_back(source);
            while (! frontiers[cur].empty()) {
                dist++;
                if (frontiers[cur].nb_outedges() <= our_bfs_cutoff) {
                    // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
                    // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
                    frontiers[cur].for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
                        if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
                            frontiers[nxt].push_vertex_back(other);
                    });
                    frontiers[cur].clear_when_front_and_back_empty();
                } else {
                    self_type::process_layer(graph_alias, dists, dist, source, frontiers[cur], frontiers[nxt]);
                }
                cur = 1 - cur;
                nxt = 1 - nxt;
            }
            return dists;
        }
        
        template <class Adjlist, class Frontier>
        static std::atomic<typename Adjlist::vtxid_type>*
        main_with_swap(const Adjlist& graph,
                       typename Adjlist::vtxid_type source) {
            using vtxid_type = typename Adjlist::vtxid_type;
            using size_type = typename Frontier::size_type;
            vtxid_type unknown = graph_constants<vtxid_type>::unknown_vtxid;
            vtxid_type nb_vertices = graph.get_nb_vertices();
            std::atomic<vtxid_type>* dists = data::mynew_array<std::atomic<vtxid_type>>(nb_vertices);
            fill_array_par(dists, nb_vertices, unknown);
            LOG_BASIC(ALGO_PHASE);
            auto graph_alias = get_alias_of_adjlist(graph);
            vtxid_type dist = 0;
            dists[source].store(dist);
            Frontier prev(graph_alias);
            Frontier next(graph_alias);
            prev.push_vertex_back(source);
            while (! prev.empty()) {
                dist++;
                if (prev.nb_outedges() <= our_bfs_cutoff) {
                    // idempotent_our_bfs::process_layer(graph_alias, dists, dist, source, prev, next);
                    // idempotent_our_bfs::process_layer_sequentially(graph_alias, dists, dist, source, prev, next);
                    prev.for_each_outedge_when_front_and_back_empty([&] (vtxid_type other) {
                        if (ls_pbfs<true>::try_to_set_dist(other, unknown, dist, dists))
                            next.push_vertex_back(other);
                    });
                    prev.clear_when_front_and_back_empty();
                } else {
                    self_type::process_layer(graph_alias, dists, dist, source, prev, next);
                }
                prev.swap(next);
            }
            return dists;
        }
    };
    
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /*---------------------------------------------------------------------*/
    /* Bellman-Ford; parallel */
    /*---------------------------------------------------------------------*/
    template <class Adjlist_seq>
    int* bellman_ford_par(const adjlist<Adjlist_seq>& graph,
                          typename adjlist<Adjlist_seq>::vtxid_type source) {
        using vtxid_type = typename adjlist<Adjlist_seq>::vtxid_type;
        using adjlist_type = adjlist<Adjlist_seq>;
        using adjlist_alias_type = typename adjlist_type::alias_type;        
        using frontiersegbag_type = pasl::graph::frontiersegbag<adjlist_alias_type>;
        
        std::atomic<vtxid_type>* bfsres = our_bfs<false>::main<adjlist_type, frontiersegbag_type>(graph, source);
        
        int inf_dist = shortest_path_constants<int>::inf_dist;
        vtxid_type nb_vertices = graph.get_nb_vertices();
        int* dists = data::mynew_array<int>(nb_vertices);

        for (size_t step = 0; step < nb_vertices; step++) {
            vtxid_type dist = bfsres[step].load();
            dists[step] = dist == -1 ? inf_dist : dist;
        }
        return dists;
    }


} // end namespace
} // end namespace

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
