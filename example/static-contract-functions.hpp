#include "benchmark.hpp"
#include "granularity.hpp"
#include "hash.hpp"
#include <vector>

namespace contraction {
namespace hash {

bool heads(long a, long b) {
  return (hash_signed(a * 100000 + b)) % 2 == 0;
}

template <class T>
T* my_malloc(long n) {
  return (T*)malloc(n*sizeof(T));
}

namespace par = pasl::sched::granularity;

#if defined(CONTROL_BY_FORCE_SEQUENTIAL)
using controller_type = par::control_by_force_sequential;
#elif defined(CONTROL_BY_FORCE_PARALLEL)
using controller_type = par::control_by_force_parallel;
#else
using controller_type = par::control_by_prediction;
#endif
using loop_controller_type = par::loop_by_eager_binary_splitting<controller_type>;

/****************************************************************************
 ************** REIMPLEMENTATION OF FILTER (AVOIDING SPARRAYS) **************
 ****************************************************************************/

template <class LIFT>
class plus_up_sweep_contr {
public:
  static controller_type contr;
};

template <class LIFT>
controller_type plus_up_sweep_contr<LIFT>::contr("plus_up_sweep" + par::string_of_template_arg<LIFT>());

template <class LIFT>
void plus_up_sweep(const LIFT& lift, long* xs, long lo, long hi, long* tree, long tree_idx) {
  long n = hi - lo;

  if (n == 0) return;

  if (n == 1) {
    tree[tree_idx] = lift(xs[lo]);
    return;
  }

  par::cstmt(plus_up_sweep_contr<LIFT>::contr, [&] { return n; }, [&] {
    long half = n/2;
    long left_idx = tree_idx + 1;
    long right_idx = tree_idx + 2*half;
    par::fork2( [&] { plus_up_sweep(lift, xs, lo     , lo+half, tree, left_idx); }
              , [&] { plus_up_sweep(lift, xs, lo+half, hi     , tree, right_idx); }
              );
    tree[tree_idx] = tree[left_idx] + tree[right_idx]; // associative op is +
  });
}

controller_type plus_down_sweep_contr("down_sweep");

void plus_down_sweep(long left_val, long* tree, long tree_idx, long* out, long lo, long hi) {
  long n = hi - lo;

  if (n == 0) return;

  if (n == 1) {
    out[lo+1] = left_val + tree[tree_idx]; // associative op is +
    return;
  }

  par::cstmt(plus_down_sweep_contr, [&] { return n; }, [&] {
    // PARALLEL
    long half = n/2;
    long left_idx = tree_idx + 1;
    long right_idx = tree_idx + 2*half;
    long right_left_val = left_val + tree[left_idx]; // associative op is +
    par::fork2( [&] { plus_down_sweep(left_val      , tree, left_idx , out, lo     , lo+half); }
              , [&] { plus_down_sweep(right_left_val, tree, right_idx, out, lo+half, hi     ); }
              );
  });
}

template <class LIFT>
long* plus_scan(const LIFT& lift, long id, long* xs, long n) {
  long* tree = my_malloc<long>(2*n - 1);
  plus_up_sweep(lift, xs, 0, n, tree, 0);

  long* out = my_malloc<long>(n + 1);
  plus_down_sweep(id, tree, 0, out, 0, n);
  out[0] = id;

  free(tree);
  return out;
}

template <class PRED>
class filter_contr {
public:
  static loop_controller_type contr;
};

template <class PRED>
loop_controller_type filter_contr<PRED>::contr("filter" + par::string_of_template_arg<PRED>());

template <class PRED>
std::pair<long*, long> filter(const PRED& pred, long* xs, long n) {
  long* offsets = plus_scan(pred, 0l, xs, n);
  long final_len = offsets[n];

  long* out = my_malloc<long>(final_len);
  par::parallel_for(filter_contr<PRED>::contr, 0l, n, [&] (long i) {
    if ((offsets[i] + 1) == offsets[i+1])
      out[offsets[i]] = xs[i];
  });

  free(offsets);
  return std::pair<long*, long>(out, final_len);
}

/****************************************************************************
 ************************** FOREST AND CONTRACTION **************************
 ****************************************************************************/

const long MAX_DEGREE = 5l;
const long NOT_A_VERTEX = -1l;

struct Node {
  long neighbors[MAX_DEGREE];
};

struct Forest {
  long num_nodes; // total number of nodes
  Node* nodes;

  long num_alive; // number of nodes currently alive
  long* alive;    // IDs of currently alive nodes
};

void display_forest(Forest* F) {
  std::cout << "=======================" << std::endl;
  for (long i = 0; i < F->num_alive; i++) {
    long v = F->alive[i];
    std::cout << v << ": ";
    for (int j = 0; j < MAX_DEGREE; j++) {
      if (F->nodes[v].neighbors[j] == NOT_A_VERTEX)
        std::cout << ". ";
      else
        std::cout << F->nodes[v].neighbors[j] << " ";
    }
    std::cout << std::endl;
  }
  std::cout << "=======================" << std::endl;
}

Forest* blank_forest(long n) {
  Forest* F = my_malloc<Forest>(1);
  F->num_nodes = n;
  F->nodes = my_malloc<Node>(n);
  return F;
}

long degree(Forest* F, long v) {
  long d = 0;
  for (long i = 0; i < MAX_DEGREE; i++) {
    if (F->nodes[v].neighbors[i] != NOT_A_VERTEX) d++;
  }
  return d;
}

long num_nodes(Forest* F) {
  return F->num_nodes;
}

long num_alive(Forest* F) {
  return F->num_alive;
}

template <class ACTION>
class apply_to_each_contr {
public:
  static loop_controller_type contr;
};

template <class ACTION>
loop_controller_type apply_to_each_contr<ACTION>::contr("apply_to_each" + par::string_of_template_arg<ACTION>());

template <class ACTION>
void apply_to_each_alive_node(Forest* F, const ACTION& action) {
  par::parallel_for(apply_to_each_contr<ACTION>::contr, 0l, num_alive(F), [&] (long i) {
    action(F->alive[i]);
  });
}

// Assuming v has degree 1 in F, return its neighbor
long neighbor(Forest* F, long v) {
  for (long i = 0; i < MAX_DEGREE; i++)
    if (F->nodes[v].neighbors[i] != NOT_A_VERTEX)
      return F->nodes[v].neighbors[i];
}

// Assuming v has degree 2 in F, return its two neighbors
std::pair<long,long> neighbors(Forest* F, long v) {
  long i = 0;
  while (i < MAX_DEGREE && F->nodes[v].neighbors[i] == NOT_A_VERTEX) i++;
  long u = F->nodes[v].neighbors[i];
  i++;
  while (i < MAX_DEGREE && F->nodes[v].neighbors[i] == NOT_A_VERTEX) i++;
  long w = F->nodes[v].neighbors[i];

  return std::pair<long,long>(u,w);
}

// In the forest F, rake v into u
// (don't modify F -- make changes in FF)
void rake(Forest* F, Forest* FF, long v, long u) {
  for (long i = 0; i < MAX_DEGREE; i++) {
    // remove v as neighbor of u in FF
    if (F->nodes[u].neighbors[i] == v) {
      FF->nodes[u].neighbors[i] = NOT_A_VERTEX;
      return;
    }
  }
}

// In the forest F, compress v by adding the edge (u,w)
// (don't modify F -- make changes in FF)
void compress(Forest* F, Forest* FF, long u, long v, long w) {
  for (long i = 0; i < MAX_DEGREE; i++) {
    // replace v with w as neighbor of u in FF
    if (F->nodes[u].neighbors[i] == v) FF->nodes[u].neighbors[i] = w;
    // replace v with u as neighbor of w in FF
    if (F->nodes[w].neighbors[i] == v) FF->nodes[w].neighbors[i] = u;
  }
}


void copy_to_next_forest(Forest* F, Forest* FF, long v) {
  for (int i = 0; i < MAX_DEGREE; i++)
    FF->nodes[v].neighbors[i] = F->nodes[v].neighbors[i];
}

void set_alive(Forest* F, long* alive, long num_alive) {
  F->alive = alive;
  F->num_alive = num_alive;
}

bool has_edges(Forest* F) {
  std::atomic<bool> found_edge;
  found_edge.store(false);
  apply_to_each_alive_node(F, [&] (long v) {
    if (degree(F, v) > 0) {
      bool orig = false;
      found_edge.compare_exchange_strong(orig, true);
    }
  });
  return found_edge.load();
}


void contract(Forest* F, Forest* FF, bool* is_alive, long round) {
  apply_to_each_alive_node(F, [&] (long v) {
    copy_to_next_forest(F, FF, v);
  });

  apply_to_each_alive_node(F, [&] (long v) {
    if (degree(F, v) == 0) {
      is_alive[v] = false;
    }
    else if (degree(F, v) == 1) {
      long u = neighbor(F, v);
      if (degree(F, u) > 1 || v > u) {
        is_alive[v] = false;
        rake(F, FF, v, u);
      }
    }
    else if (degree(F, v) == 2) {
      std::pair<long, long> uw = neighbors(F, v);
      long u = uw.first;
      long w = uw.second;
      if (degree(F, u) > 1 && degree(F, w) > 1 && !heads(u, round) && heads(v, round) && !heads(w, round)) {
        is_alive[v] = false;
        compress(F, FF, u, v, w);
      }
    }
  });

  auto pair = filter([&] (long v) { return is_alive[v]; }, F->alive, F->num_alive);
  set_alive(FF, pair.first, pair.second);
}



loop_controller_type init_contr("init_contr");

Forest* forest_contract(Forest* F) {
  long n = num_nodes(F);

  bool* is_alive = my_malloc<bool>(n);
  par::parallel_for(init_contr, 0l, n, [&] (long i) {
    is_alive[i] = true;
  });

  Forest* FF = blank_forest(n);

  long round = 0;
  while (has_edges(F)) {
    contract(F, FF, is_alive, round);
    free(F->alive);

    Forest* temp = FF;
    FF = F;
    F = temp;

    round++;
  }

  free(is_alive);
  free(FF->nodes);
  free(FF);
  return F;
}


/***************************************************************************
 ************************ INITIALIZATION-ONLY STUFF ************************
 ***************************************************************************/

long find_empty_neighbor_slot(Forest* F, long v) {
  for (long i = 0; i < MAX_DEGREE; i++)
    if (F->nodes[v].neighbors[i] == NOT_A_VERTEX) return i;
}

void insert_edge(Forest* F, long u, long v) {
  F->nodes[v].neighbors[find_empty_neighbor_slot(F, v)] = u;
  F->nodes[u].neighbors[find_empty_neighbor_slot(F, u)] = v;
}

void delete_edge(Forest* F, long u, long v) {
  for (long i = 0; i < MAX_DEGREE; i++) {
    if (F->nodes[u].neighbors[i] == v) F->nodes[u].neighbors[i] = NOT_A_VERTEX;
    if (F->nodes[v].neighbors[i] == u) F->nodes[v].neighbors[i] = NOT_A_VERTEX;
  }
}

long ith_neighbor(Forest* F, long v, long i) {
  long count = 0;
  for (long j = 0; j < MAX_DEGREE; j++) {
    long u = F->nodes[v].neighbors[j];
    if (u != NOT_A_VERTEX) {
      if (count == i) return u;
      else count++;
    }
  }
  return NOT_A_VERTEX;
}

void initialize_empty(Forest* F) {
  F->alive = my_malloc<long>(num_nodes(F));
  F->num_alive = num_nodes(F);
  for (long v = 0; v < num_nodes(F); v++) {
    F->alive[v] = v;
    for (long i = 0; i < MAX_DEGREE; i++) {
      F->nodes[v].neighbors[i] = NOT_A_VERTEX;
    }
  }
}

/***************************************************************************/

Forest* initialization_forest(int n, std::vector<int>* children, int* parent) {
  Forest* forest = blank_forest(n);
  initialize_empty(forest);
  for (int i = 0; i < n; i++) {
    if (parent[i] != i) { 
      insert_edge(forest, parent[i], i);
    }
  }
  return forest;
}

}
}