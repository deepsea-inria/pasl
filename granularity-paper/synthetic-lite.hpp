/* 
 * All rights reserved.
 *
 * \file synthetic.hpp       
 * \brief Synthetic benchmarks
 *
 */

#include "sparray.hpp"

#ifndef _MINICOURSE_SYNTHETIC_H_
#define _MINICOURSE_SYNTHETIC_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/

pasl::data::perworker::cell<long> synthetic_total(0l);

loop_controller_type sol_contr("synthetic outer loop");
par::loop_by_eager_binary_splitting<par::control_by_force_sequential> sil_contr("synthetic inner loop");
//loop_controller_type sil_contr("synthetic inner loop");
                                
void synthetic(int n, int m, int p) {
#ifdef LITE
  pasl::sched::granularity::parallel_for(sol_contr,
    [&] (int L, int R) {return true;},        
    [&] (int L, int R) {return (R - L) * m;},
    int(0), n, [&] (int i) {
      pasl::sched::granularity::parallel_for(sil_contr,
        [&] (int L, int R) {return true;},
        [&] (int L, int R) {return (R - L);},
        int(0), m, [&] (int j) {//i) {
          int value = 1;
          for (int k = 0; k < p; k++) {
            value += value ^ k;
          }
          synthetic_total.mine() += value;
      });
  });
#elif STANDART
  pasl::sched::native::parallel_for(int(0), n, [&] (int i) {
      pasl::sched::native::parallel_for(int(0), m, [&] (int i) {
          int value = 1;
          for (int k = 0; k < p; k++) {
            value += value ^ k;
          }
          synthetic_total.mine() += value;
      });
  });

#endif
}
            
void synthetic_h(int p) {
  int value = 1;
  for (int i = 0; i < p; i++)
    value += value ^ i;
  synthetic_total.mine() += value;
}
                       
controller_type sg_contr("function g");
void synthetic_g(int m, int p) {
  if (m <= 1) {
    synthetic_h(p);
  } else {      
    pasl::sched::granularity::cstmt(sg_contr,
      [&] { return true;},
      [&] { return m;},
      [&] { pasl::sched::granularity::fork2([&] {synthetic_g(m / 2, p);}, [&] {synthetic_g(m - m / 2, p);}); },
      [&] {
        for (int i = 0; i < m; i++) synthetic_h(p);
      }
    );         
  }
}
                
controller_type sf_contr("function f");
void synthetic_f(int n, int m, int p) {
  if (n <= 1) {        
    synthetic_g(m, p);
  } else {
    pasl::sched::granularity::cstmt(sf_contr,
      [&] { return true;},
      [&] { return n * m;},
      [&] { pasl::sched::granularity::fork2([&] {synthetic_f(n / 2, m, p);}, [&] {synthetic_f(n - n / 2, m, p);}); },
      [&] {
        for (int i = 0; i < n; i++) synthetic_g(m, p);
      }
    );
  }
}
/***********************************************************************/

#endif /*! _MINICOURSE_SYNTHETIC_H_ */
