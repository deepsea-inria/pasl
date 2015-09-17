#include "benchmark.hpp"
#include "granularity.hpp"
#include "hash.hpp"

bool heads(long a, long b) {
  return (hash_signed(a) + hash_signed(b)) % 2 == 0;
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

/****************************************************************************/

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

/****************************************************************************/

const long MAX_DEGREE = 4l;
const long NOT_A_VERTEX = -1l;

struct Node {
  bool alive;
  long neighbors[MAX_DEGREE];
};

struct Forest {
  long n;
  Node* nodes;
};

Forest* blank_forest(long n) {
  Forest* F = my_malloc<Forest>(1);
  F->n = n;
  F->nodes = my_malloc<Node>(n);
  return F;
}

void free_forest(Forest* F) {
  free(F->nodes);
  free(F);
}

long degree(Forest* F, long v) {
  long d = 0;
  for (long i = 0; i < MAX_DEGREE; i++) {
    if (F->nodes[v].neighbors[i] != NOT_A_VERTEX) d++;
  }
  return d;
}

bool alive(Forest* F, long v) {
  return F->nodes[v].alive;
}

void kill(Forest* F, long v) {
  F->nodes[v].alive = false;
}

long size_forest(Forest* F) {
  return F->n;
}

/* SEQUENTIAL AND PARALLEL-UNSAFE STUFF *************************************/

void display_forest(Forest* F) {
  std::cout << "=======================" << std::endl;
  for (long v = 0; v < size_forest(F); v++) {
    std::cout << v << ": ";
    if (!alive(F, v)) std::cout << "DEAD";
    else for (int i = 0; i < MAX_DEGREE; i++) {
      if (F->nodes[v].neighbors[i] == NOT_A_VERTEX)
        std::cout << ". ";
      else
        std::cout << F->nodes[v].neighbors[i] << " ";
    }
    std::cout << std::endl;
  }
  std::cout << "=======================" << std::endl;
}

void display_forest_with_alive_set(Forest* F, long* alive_nodes, long num_alive) {
  long n = size_forest(F);
  bool alive[n];
  for (long v = 0; v < n; v++) alive[v] = false;
  for (long i = 0; i < num_alive; i++) alive[alive_nodes[i]] = true;

  std::cout << "=======================" << std::endl;
  for (long v = 0; v < n; v++) {
    std::cout << v << ": ";
    if (!alive[v]) std::cout << "DEAD";
    else for (int i = 0; i < MAX_DEGREE; i++) {
      if (F->nodes[v].neighbors[i] == NOT_A_VERTEX)
        std::cout << ". ";
      else
        std::cout << F->nodes[v].neighbors[i] << " ";
    }
    std::cout << std::endl;
  }
  std::cout << "=======================" << std::endl;
}

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

void format_empty(Forest* F) {
  for (long v = 0; v < size_forest(F); v++) {
    F->nodes[v].alive = true;
    for (long i = 0; i < MAX_DEGREE; i++) {
      F->nodes[v].neighbors[i] = NOT_A_VERTEX;
    }
  }
}

/* PARALLEL-SAFE STUFF ******************************************************/

long neighbor(Forest* F, long v) {
  for (long i = 0; i < MAX_DEGREE; i++)
    if (F->nodes[v].neighbors[i] != NOT_A_VERTEX) return F->nodes[v].neighbors[i];
}

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
  FF->nodes[v].alive = false;
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
  FF->nodes[v].alive = false;
  for (long i = 0; i < MAX_DEGREE; i++) {
    // replace v with w as neighbor of u in FF
    if (F->nodes[u].neighbors[i] == v) FF->nodes[u].neighbors[i] = w;
    // replace v with u as neighbor of w in FF
    if (F->nodes[w].neighbors[i] == v) FF->nodes[w].neighbors[i] = u;
  }
}

void copy_to_next_forest(Forest* F, Forest* FF, long v) {
  FF->nodes[v].alive = F->nodes[v].alive;
  for (long i = 0; i < MAX_DEGREE; i++) {
    FF->nodes[v].neighbors[i] = F->nodes[v].neighbors[i];
  }
}

loop_controller_type has_edges_contr("has_edges_contr");

bool has_edges(Forest* F, long* alive_nodes, long num_alive) {
  std::atomic<bool> found_edge;
  found_edge.store(false);

  par::parallel_for(has_edges_contr, 0l, num_alive, [&] (long i) {
    if (degree(F, alive_nodes[i]) > 0) {
      bool orig = false;
      found_edge.compare_exchange_strong(orig, true);
    }
  });

  return found_edge.load();
}

loop_controller_type contract_contr_1("contract_contr_1");
loop_controller_type contract_contr_2("contract_contr_2");

std::pair<long*, long> contract(Forest* F, Forest* FF, long* alive_nodes, long num_alive, long round) {
  par::parallel_for(contract_contr_1, 0l, num_alive, [&] (long i) {
    copy_to_next_forest(F, FF, alive_nodes[i]);
  });

  par::parallel_for(contract_contr_2, 0l, num_alive, [&] (long i) {
    long v = alive_nodes[i];
    if (degree(F, v) == 1) {
      long u = neighbor(F, v);
      if (degree(F, u) > 1 || v > u)
        rake(F, FF, v, u);
    }
    else if (degree(F, v) == 2) {
      std::pair<long, long> uw = neighbors(F, v);
      long u = uw.first;
      long w = uw.second;
      if (degree(F, u) > 1 && degree(F, w) > 1 && !heads(u, round) && heads(v, round) && !heads(w, round))
        compress(F, FF, u, v, w);
    }
  });

  return filter([&] (long v) { return alive(FF, v); }, alive_nodes, num_alive);
}

loop_controller_type init_contr("init_contr");

Forest* forest_contract(Forest* F) {
  long n = size_forest(F);

  Forest* FF = blank_forest(n);
  long* alive_nodes = my_malloc<long>(n);
  par::parallel_for(init_contr, 0l, n, [&] (long i) {
    alive_nodes[i] = i;
  });

  long round = 0;
  long num_alive = n;
  Forest* temp;
  while (has_edges(F, alive_nodes, num_alive)) {
    // display_forest_with_alive_set(F, alive_nodes, num_alive);
    // std::cout << std::endl << std::endl;
    // std::cout << "ROUND " << round << std::endl;

    auto pair = contract(F, FF, alive_nodes, num_alive, round);
    free(alive_nodes);
    alive_nodes = pair.first;
    num_alive = pair.second;

    temp = FF;
    FF = F;
    F = temp;

    round++;
  }
  // display_forest_with_alive_set(F, alive_nodes, num_alive);
  return F;
}


/****************************************************************************/

// void check_max_degree(long lower) {
//   if (MAX_DEGREE < lower) {
//     std::cout << "Error: MAX_DEGREE must be at least " << lower << std::endl;
//     abort();
//   }
// }

int main (int argc, char** argv) {

  long n; // number of vertices
  double f; // chain factor
  long seed;
  bool print;
  Forest* F;
  Forest* result;

  auto init = [&] {
    n = std::max(2l, pasl::util::cmdline::parse_or_default_long("n", 10));
    f = pasl::util::cmdline::parse_or_default_double("f", 0.5);
    seed = pasl::util::cmdline::parse_or_default_long("seed", 42);
    print = pasl::util::cmdline::parse_or_default_bool("print", false);

    F = blank_forest(n);
    format_empty(F);

    long b = MAX_DEGREE - 1;
    long r = std::max(n - lround(f * (double) n), 2l);
    for (long i = 1; i < r; i++) {
      if (heads(i, i/b)) insert_edge(F, i, i/b);
    }

    //display_forest(F);

    srand(seed);
    for (long i = r; i < n; i++) {
      long v = rand() % i;
      while (degree(F, v) == 0) v = rand() % i;
      long u = ith_neighbor(F, v, rand() % degree(F, v));
      //std::cout << "replacing (" << v << "," << u << ") with (" << v << "," << i << ") and (" << i << "," << u << ")" << std::endl;
      delete_edge(F, v, u);
      insert_edge(F, v, i);
      insert_edge(F, i, u);
      //display_forest(F);
    }

    if (print) display_forest(F);

  };

  auto run = [&] (bool sequential) {
    result = forest_contract(F);
  };

  auto output = [&] {
    if (print) display_forest(result);
  };

  auto destroy = [&] {
    ;
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}
