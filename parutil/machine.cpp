/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 */

#ifdef TARGET_MAC_OS
#include <sys/param.h>
#include <sys/sysctl.h>
#endif
#include <stdlib.h>
#include <vector>

#include "machine.hpp"
#include "worker.hpp"
#include "cmdline.hpp"
#include "atomic.hpp"

namespace pasl {
namespace util {
namespace machine {

/***********************************************************************/

/* to represent the information we mine from /proc/cpuinfo */
struct cpuinfo_t {
  float cpu_frequency_mhz;
  int   nb_cpus;
  int   cache_line_szb;
};

/*---------------------------------------------------------------------*/
/* Globals */

int              cache_line_szb = 0;
double           cpu_frequency_ghz;
#ifdef HAVE_HWLOC
hwloc_topology_t topology;
#endif
binding_policy the_bindpolicy;
hyperthreading_mode_t htmode;

/*---------------------------------------------------------------------*/
/* Locals */

static int                 nb_pus;

/*---------------------------------------------------------------------*/

hyperthreading_mode_t htmode_of_string(std::string s) {
  if (s == "disabled")
    return HYPERTHREADING_DISABLED;
  else if (s == "ifneeded")
    return HYPERTHREADING_IFNEEDED;
  else if (s == "useall")
    return HYPERTHREADING_USEALL;
  else
    ;
  printf("bogus hyperthreading mode %s\n", s.c_str(), false);
  exit(1);
}

static struct cpuinfo_t mine_cpuinfo () {
  struct cpuinfo_t cpuinfo = { 0., 0, 0 };
#ifdef TARGET_LINUX
  /* Get information from /proc/cpuinfo.  The interesting
   * fields are:
   *
   * cpu MHz         : <float>             # cpu frequency in MHz
   *
   * cache_alignment : <int>               # cache alignment in bytes
   */
  FILE *cpuinfo_file = fopen("/proc/cpuinfo", "r");
  char buf[1024];
  int cache_line_szb;
  if (cpuinfo_file != NULL) {
    while (fgets(buf, sizeof(buf), cpuinfo_file) != 0) {
      if (sscanf(buf, "cpu MHz : %f", &(cpuinfo.cpu_frequency_mhz)) == 1) {
        cpuinfo.nb_cpus++;
      } else if (sscanf(buf, "cache_alignment : %d", &cache_line_szb) == 1) {
        cpuinfo.cache_line_szb = cache_line_szb;
      }
    }
    fclose (cpuinfo_file);
  }
#endif
#ifdef TARGET_MAC_OS
  uint64_t freq = 0;
  size_t size;
  size = sizeof(freq);
  if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) < 0) {
    perror("sysctl");
  }
  cpuinfo.cpu_frequency_mhz = (float)freq / 1000000.;
  uint64_t ncpu = 0;
  size = sizeof(ncpu);
  if (sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0) < 0) {
    perror("sysctl");
  }
  cpuinfo.nb_cpus = (int)ncpu;
  uint64_t cache_lineszb = 0;
  size = sizeof(cache_lineszb);
  if (sysctlbyname("hw.cachelinesize", &cache_lineszb, &size, NULL, 0) < 0) {
    perror("sysctl");
  }
  cpuinfo.cache_line_szb = (int)cache_lineszb;
#endif
  if (cpuinfo.cpu_frequency_mhz == 0.) {
    atomic::die("Failed to read CPU frequency\n");
  } else if (cpuinfo.cache_line_szb == 0) {
    atomic::die("Failed to read cache line size\n");
  } else if (cpuinfo.nb_cpus == 0) {
    atomic::die("Failed to read number of CPUs\n");
  } else {
    // no problem
  }
  return cpuinfo;
}

#ifdef HAVE_HWLOC

static hwloc_obj_type_t get_leaf_processing_unit_type() {
  return htmode == HYPERTHREADING_DISABLED ? HWLOC_OBJ_CORE : HWLOC_OBJ_PU;
}

/*! \brief returns the number of processing units in machine */
static int get_nb_pus () {
  int depth = hwloc_get_type_depth (topology, get_leaf_processing_unit_type());
  if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
    printf ("Warning: The number of Pus is unknown\n");
    return 1024;
  } else {
    return hwloc_get_nbobjs_by_depth (topology, depth);
  }
}

#endif

void init(hyperthreading_mode_t _htmode) {
  htmode = _htmode;
  /* Get machine parameters from various config files */
  struct cpuinfo_t cpuinfo = mine_cpuinfo ();
  cache_line_szb = cpuinfo.cache_line_szb;
  cpu_frequency_ghz = (double)(cpuinfo.cpu_frequency_mhz / 1000.0);
  ticks::set_ticks_per_seconds(cpuinfo.cpu_frequency_mhz * 1000000.);

#ifdef HAVE_HWLOC
  hwloc_topology_init (&topology);
  hwloc_topology_load (topology);
  nb_pus = get_nb_pus ();
#else
  nb_pus = cpuinfo.nb_cpus;
#endif
}

void destroy() {
  cache_line_szb = 0;
#ifdef HAVE_HWLOC
  hwloc_topology_destroy(topology);
#endif
}

/*---------------------------------------------------------------------*/

binding_policy::policy_t binding_policy::policy_of_string(std::string s) {
  policy_t bind_pol;
  if (s.compare("none") == 0) {
    bind_pol = NONE;
  } else if (s.compare("sparse") == 0) {
    bind_pol = SPARSE;
  } else if (s.compare("dense") == 0) {
    bind_pol = DENSE;
  } else {
    printf("Error: bogus cpu-binding policy %s\n", s.c_str());
    exit(-1);
  }
  return bind_pol;
}

#ifdef HAVE_HWLOC

static void get_children_of_type(hwloc_obj_type_t type,
                                 hwloc_obj_t obj,
                                 std::vector<hwloc_obj_t>& dst) {
  if (obj->arity == 0)
    return;
  if (obj->children[0]->type == type) {
    for (unsigned i = 0; i < obj->arity; i++)
      dst.push_back(obj->children[i]);
  } else {
    for (unsigned i = 0; i < obj->arity; i++)
      get_children_of_type(type, obj->children[i], dst);
  }
}

static bool can_allocate(int nb, std::vector<int>& nb_available_by_slot) {
  int total_nb_available = 0;
  for (int i = 0; i < nb_available_by_slot.size(); i++)
    total_nb_available += nb_available_by_slot[i];
  return total_nb_available >= nb;
}

template <class Resource_id>
void allocate_balls_from_bins_to_slots(
                           binding_policy::policy_t policy,
                           Resource_id undef,
                           std::vector<Resource_id>& resource_id_by_bin,
                           std::vector<int>& nb_balls_by_bin,
                           std::vector<Resource_id>& assignments) {
  int nb_slots = int(assignments.size());
  int nb_bins = int(resource_id_by_bin.size());
  assert(can_allocate(nb_slots, nb_balls_by_bin));
  for (int slot = 0; slot < nb_slots; slot++)
    assignments[slot] = undef;
  int bin = 0;
  for (int slot = 0; slot < nb_slots; slot++) {
    while (nb_balls_by_bin[bin] == 0)
      bin = (bin + 1) % nb_bins;
    nb_balls_by_bin[bin]--;
    assignments[slot] = resource_id_by_bin[bin];
    if (policy == binding_policy::SPARSE)
      bin = (bin + 1) % nb_bins;
    else
      assert(policy == binding_policy::DENSE);
  }
  for (int slot = 0; slot < nb_slots; slot++)
    assert(assignments[slot] != undef);
}

void binding_policy::disable_nb_hyperthreads(hwloc_cpuset_t all_cpus, int nb) {
  bool print_excluded_cpuids = cmdline::parse_or_default_bool("print_excluded_cpuids", false);
  hwloc_cpuset_t all_nonhyperthreads = hwloc_bitmap_dup(all_cpus);
  int nb_pus = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
  for (int pu = 0; pu < nb_pus && nb > 0; pu++) {
    hwloc_obj_t pu_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, pu);
    if (pu_node->sibling_rank % 2 == 1) {
      hwloc_bitmap_andnot(all_nonhyperthreads, all_nonhyperthreads, pu_node->cpuset);
      if (print_excluded_cpuids)
	printf("%d %d\n",pu_node->os_index,nb);
      nb--;
    }
  }
  for (worker_id_t worker = 0; worker < nb_workers; worker++) {
    hwloc_obj_t pu_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0);
    hwloc_bitmap_and(cpusets[worker], cpusets[worker], all_nonhyperthreads);
  }
  hwloc_bitmap_free(all_nonhyperthreads);
}

void binding_policy::disable_nb_hyperthreads_numa_sparse(worker_id_t worker, int nb) {
  hwloc_obj_t first = hwloc_get_next_obj_inside_cpuset_by_type(topology, cpusets[worker], HWLOC_OBJ_PU, NULL);
  hwloc_obj_t prev = first;
  while (prev != NULL) {
    if (nb <= 0)
      break;
    if (prev->sibling_rank % 2 == 1) {
      //      printf("idx=%d disable(%lld)\n",prev->logical_index,worker);
      hwloc_bitmap_andnot(cpusets[worker], cpusets[worker], prev->cpuset);
      nb--;
    }
    prev = hwloc_get_next_obj_inside_cpuset_by_type(topology, cpusets[worker], HWLOC_OBJ_PU, prev);
  }
}

#endif /* DISABLE_HWLOC */

static void check_enough_pus_available(int nb_workers, int nb_pus_available) {
  if (nb_workers > nb_pus_available) {
    fprintf (stderr, "Cannot satisfy cpu binding request for %d cpus, as "
             "system has only %d pu available (one pu is reserved"
             "for the OS, to avoid interference; use -no0 0 to disable this) \n",
             (int)nb_workers, (int)nb_pus_available);
    exit (1);
  }
}

void binding_policy::init(policy_t node_policy, bool no0, int nb_workers) {
  // if this assert fails, you probably forgot to call worker::init()
  assert(cache_line_szb != 0);
  // if this assert fails, you called init multiple times
  assert(this->nb_workers == 0);
  this->policy = policy;
  this->no0 = no0;
  this->nb_workers = nb_workers;
  if (no0 && (nb_workers == 1)) {
    // if there is just one pu, we do not exclude pu 0
    no0 = false;
  }
  int nb_pus_available = no0 ? nb_pus - 1 : nb_pus;
  check_enough_pus_available(nb_workers, nb_pus_available);

  /* Informs the OS scheduler that each worker is a long-lived,
   * heavy-weight thread. */
  pthread_setconcurrency ((int)nb_workers);

#ifdef HAVE_HWLOC
  hwloc_obj_type_t pu_type = get_leaf_processing_unit_type();
  cpusets = new hwloc_cpuset_t[nb_workers];
  hwloc_cpuset_t all_cpus =
    hwloc_bitmap_dup (hwloc_topology_get_topology_cpuset (topology));
  int node_depth = hwloc_get_type_or_below_depth (topology, HWLOC_OBJ_NODE);
  int nb_nodes = hwloc_get_nbobjs_by_depth (topology, node_depth);
  int nb_cores = hwloc_get_nbobjs_by_type (topology, HWLOC_OBJ_CORE);
  std::vector<int> nb_pus_avail_by_node(nb_nodes, 0);
  for (int node_id = 0; node_id < nb_nodes; node_id++) {
    hwloc_obj_t node = hwloc_get_obj_by_depth (topology, node_depth, node_id);
    nb_pus_avail_by_node[node_id] =
      hwloc_get_nbobjs_inside_cpuset_by_type (topology, node->cpuset, pu_type);
  }

  std::vector<node_id_t> node_assignments(nb_workers, node_undef);
  std::vector<node_id_t> node_ids(nb_nodes, node_undef);
  for (node_id_t node = 0; node < nb_nodes; node++)
    node_ids[node] = node;

  if (node_policy != NONE)
    allocate_balls_from_bins_to_slots(node_policy, node_undef, node_ids,
                                      nb_pus_avail_by_node, node_assignments);

  for (worker_id_t worker = 0; worker < nb_workers; worker++) {
    node_id_t node = node_assignments[worker];
    hwloc_cpuset_t cpuset;
    if (node == node_undef)
      cpuset = all_cpus;
    else
      cpuset = hwloc_get_obj_by_depth (topology, node_depth, node)->cpuset;
    cpusets[worker] = hwloc_bitmap_dup (cpuset);
    if (node_policy == SPARSE && htmode == HYPERTHREADING_IFNEEDED) {
      hwloc_obj_t node_obj = hwloc_get_obj_by_depth (topology, node_depth, node);
      int nb_pus =
      hwloc_get_nbobjs_inside_cpuset_by_type (topology, node_obj->cpuset, pu_type);
      int nb_pus_unused = nb_pus_avail_by_node[node];
      disable_nb_hyperthreads_numa_sparse(worker, nb_pus_unused);
    }
  }

  if (htmode == HYPERTHREADING_DISABLED)
    disable_nb_hyperthreads(all_cpus, nb_cores);
  else if (htmode == HYPERTHREADING_IFNEEDED && node_policy == NONE)
    disable_nb_hyperthreads(all_cpus, nb_pus - nb_workers);
  else
    ;

#endif
}

void binding_policy::destroy() {
#ifdef HAVE_HWLOC
  for (worker_id_t id = 0; id < nb_workers; id++) {
    hwloc_bitmap_free (cpusets[id]);
  }
  delete [] cpusets;
#endif
}

void binding_policy::pin_calling_thread(worker_id_t my_id) {
  // if this assert fails, you forgot to call the_binding_policy.init()
  assert(this->nb_workers != 0);
#ifdef HAVE_HWLOC
  int flags = HWLOC_CPUBIND_STRICT | HWLOC_CPUBIND_THREAD;
  if (hwloc_set_cpubind (topology, cpusets[my_id], flags)) {
    char *str;
    int error = errno;
    hwloc_bitmap_asprintf (&str, cpusets[my_id]);
    printf ("Couldn't bind to cpuset %s: %s\n", str, strerror(error));
    free (str);
  }
#elif HAVE_SCHED_SETAFFINITY
  /* If hwloc is disabled, the only cpu-binding policy that we support
   * is no0. */
  if (no0) {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    for (int cpu = 1; cpu < nb_pus; cpu++) {
      CPU_SET(cpu, &cpus);
    }
    if (sched_setaffinity (0, sizeof(cpu_set_t), &cpus) == -1) {
      printf ("unable to set affinity to processor %lld\n", my_id);
    }
  }
#else
  // in this case, cpu binding is not supported
#endif
}

#ifdef HAVE_HWLOC
hwloc_nodeset_t binding_policy::nodeset_of_worker(worker_id_t my_id_or_undef) {
  worker_id_t my_id = (my_id_or_undef == worker::undef) ? 0 : my_id_or_undef;
  hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
  hwloc_cpuset_t cpuset = cpusets[(int)my_id];
  hwloc_cpuset_to_nodeset(topology, cpuset, nodeset);
  return nodeset;
}
#endif

/*---------------------------------------------------------------------*/

void numa::init(int nb_workers) {
#ifdef HAVE_HWLOC
  bool numa_alloc_interleaved = (nb_workers == 0) ? false : true;
  numa_alloc_interleaved =
    cmdline::parse_or_default_bool("numa_alloc_interleaved", numa_alloc_interleaved, false);
  if (numa_alloc_interleaved) {
    hwloc_cpuset_t all_cpus =
      hwloc_bitmap_dup (hwloc_topology_get_topology_cpuset (topology));
    int err = hwloc_set_membind(topology, all_cpus, HWLOC_MEMBIND_INTERLEAVE, 0);
    if (err < 0)
      printf("Warning: failed to set NUMA round-robin allocation policy\n");
  }
#endif
  nodes = new node_id_t[nb_workers];
  int max_node_id = 0;
  for (worker_id_t id = 0; id < nb_workers; id++) {
    int node_id = node_undef;
#ifdef HAVE_HWLOC
    machine::binding_policy_p bindpolicy = bpol;
    hwloc_nodeset_t nodeset = bindpolicy->nodeset_of_worker(id);
    hwloc_bitmap_foreach_begin(node_id, nodeset)
      break;
    hwloc_bitmap_foreach_end();
    hwloc_bitmap_free(nodeset);
#else
    node_id = 0;
#endif
    nodes[id] = node_id;
    max_node_id = std::max(node_id, max_node_id);
  }
  nb_nodes = max_node_id + 1;
  //-----------------
  nb_workers_per_node = new int[nb_nodes];
  node_ranks = new int[nb_workers];
  int node_ids[nb_workers];
  leaders = new worker_id_t[nb_nodes];
  for (int i = 0; i < nb_nodes; i++) {
    nb_workers_per_node[i] = 0;
    leaders[i] = worker::undef;
  }
  for (worker_id_t worker_id = 0; worker_id < nb_workers; worker_id++) {
    int node_id = node_of_worker(worker_id);
    node_ids[worker_id] = node_id;
    node_ranks[worker_id] = nb_workers_per_node[node_id];
    nb_workers_per_node[node_id]++;
    leaders[node_id] = worker_id;
  }
  // This hack ensures that thread 0 is leader of the node where it executes.
  leaders[node_of_worker(0)] = 0;

  //-----------------
  for (machine::node_id_t node = 0; node < nb_nodes; node++) {
    worker_set_t set;
    node_info.push_back(set);
  }
  for (worker_id_t id = 0; id < nb_workers; id++) {
    node_id_t node = node_of_worker(id);
    node_info[node].push_back(id);
  }
  //-----------------
  for (machine::node_id_t node = 0; node < nb_nodes; node++)
    for (int i = 0; i < (int)node_info[node].size(); i++)
      assert(rank_of_worker(worker_of_rank(node, i)) == i);
}

numa::numa() {
  this->bpol = &the_bindpolicy;
}

numa::numa(binding_policy_p bpol) {
  this->bpol = bpol;
}

numa::~numa() {
  delete [] nodes;
  delete [] nb_workers_per_node;
  delete [] node_ranks;
  delete [] leaders;
}

int numa::get_nb_nodes() {
  return nb_nodes;
}

node_id_t numa::node_of_worker(worker_id_t id) {
  return nodes[id];
}

int numa::rank_of_worker(worker_id_t id) {
  return node_ranks[id];
}

worker_id_t numa::worker_of_rank(node_id_t node, int rank) {
  worker_set_t& set = node_info[node];
  return set[rank];
}

int numa::nb_workers_of_node(node_id_t node) {
  return nb_workers_per_node[node];
}

worker_id_t numa::leader_of_node(node_id_t node) {
  return leaders[node];
}

worker_id_t numa::set_leader_of_node(node_id_t node, worker_id_t id) {
  worker_id_t old = leaders[node];
  leaders[node] = id;
  return old;
}

numa the_numa;

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace
