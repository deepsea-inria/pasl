#include "granularity-lite.hpp"
#include <iostream>
#include <cstring>
#include <math.h>
#include "sequencedata.hpp"
#include "io.hpp"
#include "container.hpp"

using std::pair;
using std::min;
using std::max;
/*---------------------------------------------------------------------*/

#ifdef CMDLINE
  typedef pasl::sched::granularity::control_by_cmdline control_type;
#elif PREDICTION
  typedef pasl::sched::granularity::control_by_prediction control_type;
#elif CUTOFF_WITH_REPORTING
  typename pasl::sched::granularity::control_by_cutoff_with_reporting control_type;
#elif CUTOFF_WITHOUT_REPORTING
  typename pasl::sched::granularity::control_by_cutoff_without_reporting control_type;
#endif

#ifdef BINARY
  typedef pasl::sched::granularity::loop_by_eager_binary_splitting<control_type> loop_type;
#elif LAZY_BINARY
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting<control_type> loop_type;
#elif SCHEDULING
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting_scheduling<control_type> loop_type;
#elif BINARY_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_eager_binary_splitting<control_type>> loop_type;
#elif BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_binary_search_splitting<control_type> loop_type;
#elif LAZY_BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_lazy_binary_search_splitting<control_type> loop_type;
#elif BINARY_SEARCH_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_binary_search_splitting<control_type>> loop_type;
#endif


int p;
int total = 0;
void h() {
  int sum = 1;
  for (int i = 0; i < p; i++)
    total++;
}

control_type cg("function g");
int g_cutoff_const;
void g(int m) {
  if (m <= 1) {
    h();
  } else {
    pasl::sched::granularity::cstmt(cg,
      [&] { return m <= g_cutoff_const;},
      [&] { return m;},
      [&] { pasl::sched::granularity::fork2([&] {g(m / 2);}, [&] {g(m - m / 2);}); },
      [&] {
        for (int i = 0; i < m; i++) h();
//        g(m / 2);
//        g(m - m / 2);
      }
    );
  }
}

control_type cf("function f");
int f_cutoff_const;

void f(int n, int m) {
  if (n <= 1) {
    g(m);
  } else {
    pasl::sched::granularity::cstmt(cf,
      [&] { return n * m <= f_cutoff_const;},
      [&] { return n * m;},
      [&] { pasl::sched::granularity::fork2([&] {f(n / 2, m);}, [&] {f(n - n / 2, m);}); },
      [&] {
        for (int i = 0; i < n; i++) g(m);
//        f(n / 2, m);
//        f(n - n / 2, m);
      }
    );
  }
}


loop_type sol_contr("synthetic outer loop");
loop_type sil_contr("synthetic inner loop");
                                
void synthetic(int n, int m, int p) {
#ifdef LITE
  pasl::sched::granularity::parallel_for(sol_contr,
    [&] (int L, int R) {return true;},        
    [&] (int L, int R) {return (R - L) * m;},
    int(0), n, [&] (int i) {
      pasl::sched::granularity::parallel_for(sil_contr,
        [&] (int L, int R) {return true;},
        [&] (int L, int R) {return (R - L);},
        int(0), m, [&] (int i) {
          for (int k = 0; k < p; k++) {
            total++;
          }
      });
  });
#elif STANDART
  pasl::sched::native::parallel_for(int(0), n, [&] (int i) {
      pasl::sched::native::parallel_for(int(0), m, [&] (int i) {
          for (int k = 0; k < p; k++) {
            total++;
          }
      });
  });
#endif
}


/*---------------------------------------------------------------------*/

void initialization() {
  pasl::util::ticks::set_ticks_per_seconds(1000);
  cf.initialize(10000, 1);
  cg.initialize(1);

  sol_contr.initialize(1, 10);
  sil_contr.initialize(1, 10);
}

int main(int argc, char** argv) {
  int n, m;
  std::string running_mode;
  int type;
  
  auto init = [&] {
    initialization();
    f_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "f_cutoff", 100);
    g_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "g_cutoff", 100); 
    n = pasl::util::cmdline::parse_or_default_int(
        "n", 2000);
    m = pasl::util::cmdline::parse_or_default_int(
        "m", 2000);
    p = pasl::util::cmdline::parse_or_default_int(
        "p", 100);
    type = pasl::util::cmdline::parse_or_default_int(
        "type", 1);
    running_mode = pasl::util::cmdline::parse_or_default_string(
        "mode", std::string("by_force_sequential"));
    std::cout << "Using " << running_mode << " mode" << std::endl;;

    cf.set(running_mode);
    cg.set(running_mode);

    sol_contr.set(running_mode);
    sil_contr.set(running_mode);
  };
  auto run = [&] (bool sequential) {
    if (type == 1)
      f(n, m);
    else
      synthetic(n, m, p);
  };

  auto output = [&] {
      std::cout << "The evaluation have finished" << std::endl;
      std::cout << total << std::endl;
  };
  auto destroy = [&] {
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}

/***********************************************************************/
