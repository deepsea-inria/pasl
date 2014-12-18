/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file mcss.hpp
 * \brief Maximum contiguous subsequence
 *
 */

#include "hash.hpp"
#include "sparray.hpp"

#ifndef _MINICOURSE_MCSS_H_
#define _MINICOURSE_MCSS_H_

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Maximum contiguous subsequence */

value_type mcss_seq(const sparray& xs) {
  if (xs.size() == 0)
    return VALUE_MIN;
  value_type max_so_far = xs[0];
  value_type curr_max = xs[0];
  for (long i = 1; i < xs.size(); i++) {
    curr_max = std::max(xs[i], curr_max+xs[i]);
    max_so_far = std::max(max_so_far, curr_max);
  }
  return max_so_far;
}

value_type mcss_par(const sparray& xs) {
  sparray ys = prefix_sums_incl(xs);
  scan_excl_result m = scan_excl(min_fct, 0l, ys);
  sparray zs = tabulate([&] (long i) { return ys[i]-m.partials[i]; }, xs.size());
  return max(zs);
}

value_type mcss(const sparray& xs) {
#ifdef SEQUENTIAL_BASELINE
  return mcss_seq(xs);
#else
  return mcss_par(xs);
#endif
}

/***********************************************************************/

#endif /*! _MINICOURSE_MCSS_H_ */
