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
        int(0), m, [&] (int i) {
          int value = 1;
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
             
  return total;
}

/***********************************************************************/

#endif /*! _MINICOURSE_SYNTHETIC_H_ */
