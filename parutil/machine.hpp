/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file machine.hpp
 * \brief An interface to architecture details.
 *
 */

#ifndef _PASL_UTIL_MACHINE_H_
#define _PASL_UTIL_MACHINE_H_

#include <vector>
#ifdef HAVE_HWLOC
#include <hwloc.h>
#else
#include <sched.h>
#endif

#ifndef _TICKS_H_
#include "ticks.hpp"
#endif 
#include "aliases.hpp"

namespace pasl {
namespace util {
namespace machine {

/***********************************************************************/

/* \brief Size of a cache line in bytes */
extern int cache_line_szb;

/* \brief CPU frequency in gigaherz */
extern double cpu_frequency_ghz;

#ifdef HAVE_HWLOC
extern hwloc_topology_t    topology;
#endif
  
/*---------------------------------------------------------------------*/

typedef enum { HYPERTHREADING_DISABLED, 
	       HYPERTHREADING_IFNEEDED, 
	       HYPERTHREADING_USEALL } hyperthreading_mode_t;

hyperthreading_mode_t htmode_of_string(std::string s);

/*---------------------------------------------------------------------*/

//! Initializes the module
void init(hyperthreading_mode_t htmode);
//! Tears down the module
void destroy();

/*---------------------------------------------------------------------*/

/*! \class binding_policy
 *  \brief A policy which determines on which hardware processing element 
 *  each worker thread may execute.
 */
class binding_policy {
public:
  
  typedef enum {
    NONE, SPARSE, DENSE
  } policy_t;
  
  binding_policy() : nb_workers(0) { }

  /*! \brief Initializes object members
   *  \param policy the binding policy
   *  \param no0 if true, ensures that no worker can execute on CPU #0
   *  \param nb_workers the number of worker threads
   */
  void init(policy_t node_policy, bool no0, int nb_workers);
  //! Teardown
  void destroy();
  /*! \brief Binds the calling worker thread to a hardware processing
   *   element (as determined by the binding policy).
   *  \param my_id the id of the calling worker thread
   */
  void pin_calling_thread(worker_id_t my_id);
  //! Tries to convert the given string to a binding policy id
  static policy_t policy_of_string(std::string s);

#ifdef HAVE_HWLOC
  /* \brief Returns the set of NUMA nodes that are close to the given
   * worker. 
   * 
   * \warning The caller is responsible to free the return result
   * by calling hwloc_bitmap_free().
   */
  hwloc_nodeset_t nodeset_of_worker(worker_id_t my_id_or_undef);
#endif

protected:
  policy_t                    policy;
  bool                        no0;
  int                         nb_workers;
#ifdef HAVE_HWLOC
  /* \brief An array of CPU sets keyed by worker ID; Each item 
   * determines the set of CPUs on which the worker can execute. */
  hwloc_cpuset_t*             cpusets;

  void disable_nb_hyperthreads(hwloc_cpuset_t all_cpus, int nb);
  void disable_nb_hyperthreads_numa_sparse(worker_id_t worker, int nb);
#endif
};
  
typedef binding_policy* binding_policy_p;

extern binding_policy the_bindpolicy;
  
/*---------------------------------------------------------------------*/
  
//! NUMA node id
typedef int node_id_t;
  
const node_id_t node_undef = -1;

  //! \todo document

/*! \class numa
 *  \brief Maintains mapping between workers and NUMA nodes.
 *
 * 
 */
class numa {
private:
  binding_policy_p bpol;
  int nb_nodes;
  //! \todo use std::vector instead of arrays
  node_id_t* nodes;
  int* nb_workers_per_node;
  int* node_ranks;
  worker_id_t* leaders;
  typedef std::vector<worker_id_t> worker_set_t;
  typedef std::vector<worker_set_t> node_info_t;
  node_info_t node_info;
  
public:
  /*! \note Equivalent to `numa(the_bindpolicy)` */
  numa();
  numa(binding_policy_p bpol);
  ~numa();
  void init(int nb_workers);
  //! Returns the number of nodes that are allocated to workers
  int get_nb_nodes();
  /*! \brief Returns one of multiple nodes to which the given worker
   *  is bound.
   *  \param id id of the worker
   *  \return a node id on which the worker is bound
   *
   * - If the worker is not bound to a node, the return value is node 
   * `node_undef`.
   *
   * - If the worker is bound to multiple nodes, the return value is one of
   * those nodes chosen arbitrarily.
   */
  node_id_t node_of_worker(worker_id_t id);  
  /*! \brief Returns the relative position of the given worker in its 
   *  node. 
   */
  int rank_of_worker(worker_id_t id);
  /*! \brief Returns the id of the worker at the given position */
  worker_id_t worker_of_rank(node_id_t node, int rank);
  /*! \brief Returns the number of workers bound to the given node.
   */
  int nb_workers_of_node(node_id_t node);
  /*! \brief Returns the id of the leader of a given node.
   * 
   *  The leader is a worker chosen arbitrarily to represent the node.
   *
   *  If the set of workers is empty, the return value is
   *  `worker::undef`.
   */
  worker_id_t leader_of_node(node_id_t node);
  worker_id_t set_leader_of_node(node_id_t node, worker_id_t id);
};
  
extern numa the_numa;

/***********************************************************************/

} // namespace
} // namespace
} // namespace

#endif /*! _PASL_UTIL_MACHINE_H_ */
