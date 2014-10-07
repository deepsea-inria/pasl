/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2013 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Benchmarking worst-case for std::deque
 * \file pushpop_offset.cpp
 *
 */

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <sys/time.h>  
#include <stdint.h>

/*---------------------------------------------------------------------*/

using namespace std;

//typedef unsigned long long uint64_t;
typedef uint64_t size_t;

/*---------------------------------------------------------------------*/

uint64_t pushpop_at_offset(uint64_t nb_repeat, uint64_t offset) { 
  deque<uint64_t> d;
  uint64_t res = 0;
  for (size_t i = 0; i < offset; i++) {
    d.push_back(i);
  }
  for (size_t i = 0; i < nb_repeat; i++) {
    d.push_back(i);
    // asm volatile("" ::: "memory");  // prevent optimization
    res += d.back();
    d.pop_back();
  }
  return res;
}


/*---------------------------------------------------------------------*/

void error(string msg) {
  printf("Error %s\n", msg.c_str());
  exit(1);
}
typedef uint64_t microtime_t;

microtime_t microtime_now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000000l * ((microtime_t) tv.tv_sec) + ((microtime_t) tv.tv_usec);
}

double microtime_seconds_since(microtime_t t) {
  return ((double) (microtime_now() - t)) / 1000000l;
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  if (argc != 5)
    error("not the good number of arguments");
  string str_nb_repeat = string(argv[1]);
  uint64_t nb_repeat = atoi(argv[2]);
  string str_offset = string(argv[3]);
  uint64_t offset = atoi(argv[4]);
  if (str_nb_repeat != "-nb_repeat" || str_offset != "-offset")
    error("not the right numbers");
  microtime_t start_time = microtime_now();
  uint64_t res = pushpop_at_offset(nb_repeat, offset);
  double exec_time = microtime_seconds_since(start_time);
  printf ("exectime %lf\n", exec_time);
  printf("result %lld\n", res);
  return 0;
}


/*---------------------------------------------------------------------*/
