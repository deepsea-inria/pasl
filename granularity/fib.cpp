#include "granularity-lite.hpp"
#include <iostream>
/*---------------------------------------------------------------------*/
/* Various client-code examples using naive recursive fib */

// sequential fib
static
long fib(long n) {
  if (n < 2)
    return n;
  long a,b;
  a = fib(n-1);
  b = fib(n-2);
  return a+b;
}

// complexity function for fib(n)
static
long phi_to_pow(long n) {
  constexpr double phi = 1.61803399;
  return (long)pow(phi, (double)n);
}

control_by_prediction cfib("fib");

// parallel fib with complexity function
static
long pfib1(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib, [&] { return phi_to_pow(n); }, [&] {
  fork2([&] { a = pfib1(n-1); },
        [&] { b = pfib1(n-2); }); });
  return a + b;
}

// parallel fib with complexity function and sequential body alternative
static
long pfib2(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib, [&] { return phi_to_pow(n); }, [&] {
  fork2([&] { a = pfib2(n-1); },
        [&] { b = pfib2(n-2); }); },
  [&] { a = fib(n-1);
        b = fib(n-2); });
  return a + b;
}

control_by_cutoff_without_reporting cfib2("fib");

constexpr long fib_cutoff = 20;

// parallel fib with manual cutoff and sequential body alternative
static
long pfib3(long n) {
  if (n < 2)
    return n;
  long a,b;
  cstmt(cfib2, [&] { return n <= fib_cutoff; }, [&] {
  fork2([&] { a = pfib3(n-1); },
        [&] { b = pfib3(n-2); }); },
  [&] { a = fib(n-1);
        b = fib(n-2); });
  return a + b;
}

/*---------------------------------------------------------------------*/

void initialization() {
  
  pasl::util::ticks::set_ticks_per_seconds(1000);
  local_constants.init(undefined);
  //execmode.init(dynidentifier<execmode_type>(Sequential));
  execmode.init(dynidentifier<execmode_type>());
}

int main(int argc, char** argv) {
  int n = 0;
  auto init = [&] {
    //cutoff = (long)pasl::util::cmdline::parse_or_default_int("cutoff", 25);
    n = (long)pasl::util::cmdline::parse_or_default_int("n", 10);
    initialization();
  };
  auto run = [&] (bool sequential) {
    std::cout << "fib(" << n << ")=" << fib(n) << std::endl;
    std::cout << "pfib1(" << n << ")=" << pfib1(n) << std::endl;
    std::cout << "pfib2(" << n << ")=" << pfib2(n) << std::endl;
    std::cout << "pfib3(" << n << ")=" << pfib3(n) << std::endl;
  };

  auto output = [&] {
    ;
    //std::cout << "result\t" << result << std::endl;
  };
  auto destroy = [&] {
    ;
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}

/***********************************************************************/
