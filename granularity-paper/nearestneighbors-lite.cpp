/* 
 * All rights reserved.
 *
 * \file synthetic.hpp       
 * \brief Synthetic benchmarks
 *
 */

#include <iostream>
#include <cstring>      
#include <math.h>          
#include "granularity-lite.hpp"

#ifdef CMDLINE
  typedef pasl::sched::granularity::control_by_cmdline controller_type;
#elif PREDICTION
  typedef pasl::sched::granularity::control_by_prediction controller_type;
#elif CUTOFF_WITH_REPORTING
  typename pasl::sched::granularity::control_by_cutoff_with_reporting controller_type;
#elif CUTOFF_WITHOUT_REPORTING
  typename pasl::sched::granularity::control_by_cutoff_without_reporting controller_type;
#endif

#ifdef BINARY
  typedef pasl::sched::granularity::loop_by_eager_binary_splitting<controller_type> loop_controller_type;
#elif LAZY_BINARY
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting<controller_type> loop_controller_type;
#elif SCHEDULING
  typedef pasl::sched::granularity::loop_by_lazy_binary_splitting_scheduling<controller_type> loop_controller_type;
#elif BINARY_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_eager_binary_splitting<controller_type>> loop_controller_type;
#elif BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_binary_search_splitting<controller_type> loop_controller_type;
#elif LAZY_BINARY_SEARCH
  typedef pasl::sched::granularity::loop_by_lazy_binary_search_splitting<controller_type> loop_controller_type;
#elif BINARY_SEARCH_WITH_SAMPLING
  typedef pasl::sched::granularity::loop_control_with_sampling<loop_by_binary_search_splitting<controller_type>> loop_controller_type;
#endif                    

#include "nearestneighbors-lite.hpp"

void initialization() {
  pasl::util::ticks::set_ticks_per_seconds(1000);
  nn_build_contr.initialize(1);
  nn_run_contr.initialize(1, 10);
}

int main(int argc, char** argv) {
  int n = 0;
  AbstractRunnerNN* runner = NULL;
  
  auto init = [&] {
    initialization();
    n = pasl::util::cmdline::parse_or_default_int(
        "n", 1000);
    int d = pasl::util::cmdline::parse_or_default_int(
        "d", 2);
    int k = pasl::util::cmdline::parse_or_default_int(
        "k", 2);
    std::string gen_type = pasl::util::cmdline::parse_or_default_string(
        "gen", "uniform");
    bool inSphere = pasl::util::cmdline::parse_or_default_bool(
        "in-sphere", false);
    bool onSphere = pasl::util::cmdline::parse_or_default_bool(
        "on-sphere", false);
//        "run_cutoff", 1000);

    if (d == 2) {
      if (gen_type.compare("uniform") == 0) {
        pbbs::point2d* points = pbbs::uniform2d(inSphere, onSphere, n);
        runner = new RunnerNN<int, pbbs::point2d, 20>(preparePoints<pbbs::point2d, 20>(n, points), n, k);
      } else if (gen_type.compare("plummer") == 0) {        pbbs::point2d* points = pbbs::plummer2d(n);
        runner = new RunnerNN<int, pbbs::point2d, 20>(preparePoints<pbbs::point2d, 20>(n, points), n, k);
      } else {
        std::cerr << "Wrong generator type " << gen_type << "\n";
        exit(-1);
      }
    } else {
      if (gen_type.compare("uniform") == 0) {
        pbbs::point3d* points = pbbs::uniform3d<int, int>(inSphere, onSphere, n);
        runner = new RunnerNN<int, pbbs::point3d, 20>(preparePoints<pbbs::point3d, 20>(n, points), n, k);
      } else if (gen_type.compare("plummer") == 0) {
        pbbs::point3d* points = pbbs::plummer3d<int, int>(n);
        runner = new RunnerNN<int, pbbs::point3d, 20>(preparePoints<pbbs::point3d, 20>(n, points), n, k);
      } else {
        std::cerr << "Wrong generator type " << gen_type << "\n";
        exit(-1);
      }
    }
    std::string running_mode = pasl::util::cmdline::parse_or_default_string(
          "mode", std::string("by_force_sequential"));

    #ifdef CMDLINE
      std::cout << "Using " << running_mode << " mode" << std::endl;
    #elif PREDICTION
      std::cout << "Using by_prediction mode" << std::endl;
    #elif CUTOFF_WITH_REPORTING
      std::cout << "Using by_cutoff_with_reporting mode" << std::endl;
    #elif CUTOFF_WITHOUT_REPORTING        
      std::cout << "Using by_cutoff_without_reporting mode" << std::endl;
    #endif
                                                 
    nn_build_contr.set(running_mode);nn_run_contr.set(running_mode);
  };

  auto run = [&] (bool sequential) {
    runner->initialize();
    runner->run();
  };

  auto output = [&] {
    std::cout << "The evaluation have finished" << std::endl;
  };
  auto destroy = [&] {
    runner->free();
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

