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
  typedef control_by_cmdline control_type;
#elif PREDICTION
  typedef control_by_prediction control_type;
#elif CUTOFF_WITH_REPORTING
  typename control_by_cutoff_with_reporting control_type;
#elif CUTOFF_WITHOUT_REPORTING
  typename control_by_cutoff_without_reporting control_type;
#endif

int p;
int total = 0;
void h() {
  total++;
  int sum = 1;
  for (int i = 0; i < p; i++)
    total ^= 1;
}

control_type cg("function g");
int g_cutoff_const;
void g(int m) {
  if (m <= 1) {
    h();
  } else {
    cstmt(cg,
      [&] { return m <= g_cutoff_const;},
      [&] { return m;},
      [&] { fork2([&] {g(m / 2);}, [&] {g(m - m / 2);}); },
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
    cstmt(cf,
      [&] { return n * m <= f_cutoff_const;},
      [&] { return n * m;},
      [&] { fork2([&] {f(n / 2, m);}, [&] {f(n - n / 2, m);}); },
      [&] {
//        for (int i = 0; i < n; i++) g(m);
        f(n / 2, m);
        f(n - n / 2, m);
      }
    );
  }
}


/*---------------------------------------------------------------------*/

void initialization() {
  pasl::util::ticks::set_ticks_per_seconds(1000);
//  local_constants.init(undefined);
  cf.initialize(10000, 1);
  cg.initialize(1);
  execmode.init(dynidentifier<execmode_type>());
}

int main(int argc, char** argv) {
  int n, m;
  std::string running_mode;
  
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
    running_mode = pasl::util::cmdline::parse_or_default_string(
        "mode", std::string("by_force_sequential"));
    std::cout << "Using " << running_mode << " mode" << std::endl;;
    cf.set(running_mode);
    cg.set(running_mode);
  };
  auto run = [&] (bool sequential) {
    f(n, m);
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
