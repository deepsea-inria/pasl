/* 
 * All rights reserved.
 *
 * \file synthetic.hpp       
 * \brief Synthetic benchmarks
 *
 */

//#include "sparray.hpp"

#ifndef _MINICOURSE_SYNTHETIC_H_
#define _MINICOURSE_SYNTHETIC_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/

loop_controller_type sol_contr("synthetic outer loop");
loop_controller_type sil_contr("synthetic inner loop");
                                
int synthetic(int n, int m, int p) {
  int total = 0;
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
            total^= 1;//++;
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
             
  return total;
}
            
int synthetic_h(int p) {
  int total = 0;
  for (int i = 0; i < p; i++)
    total++;
  return total;
}
                       
controller_type sg_contr("function g");
int synthetic_g(int m, int p) {
  if (m <= 1) {
    return synthetic_h(p);
  } else {      
    int total = 0;
    pasl::sched::granularity::cstmt(sg_contr,
      [&] { return true;},
      [&] { return m;},
      [&] { pasl::sched::granularity::fork2([&] {total += synthetic_g(m / 2, p);}, [&] {total += synthetic_g(m - m / 2, p);}); },
      [&] {
        for (int i = 0; i < m; i++) total += synthetic_h(p);
      }
    );         
    return total;
  }
}
                
controller_type sf_contr("function f");
int synthetic_f(int n, int m, int p) {
  if (n <= 1) {        
    return synthetic_g(m, p);
  } else {
    int total = 0;
    pasl::sched::granularity::cstmt(sf_contr,
      [&] { return true;},
      [&] { return n * m;},
      [&] { pasl::sched::granularity::fork2([&] {total += synthetic_f(n / 2, m, p);}, [&] {total += synthetic_f(n - n / 2, m, p);}); },
      [&] {
        for (int i = 0; i < n; i++) total += synthetic_g(m, p);
      }
    );
    return total;
  }
}
/***********************************************************************/

#endif /*! _MINICOURSE_SYNTHETIC_H_ */
