/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file geometryio.hpp
 * \brief Geometry data IO
 *
 */

#include "geometrydata.hpp"
#include "cmdline.hpp"

#ifndef _PCTL_GEOMETRY_IO_H_
#define _PCTL_GEOMETRY_IO_H_

/***********************************************************************/

namespace pasl {
namespace pctl {

template <class intT>
parray<point2d> load_points2d() {
  parray<point2d> points;
  intT n = (intT)pasl::util::cmdline::parse_or_default_long("n", 10);
  pasl::util::cmdline::argmap_dispatch d;
  d.add("from_file", [&] {
    pasl::util::atomic::die("todo");
  });
  d.add("by_generator", [&] {
    pasl::util::cmdline::argmap_dispatch d;
    d.add("plummer", [&] {
      points = plummer2d(n);
    });
    d.add("uniform", [&] {
      bool inSphere = pasl::util::cmdline::parse_or_default_bool("in_sphere", false);
      bool onSphere = pasl::util::cmdline::parse_or_default_bool("on_sphere", false);
      points = uniform2d(inSphere, onSphere, n);
    });
    d.find_by_arg_or_default_key("generator", "plummer")();
  });
  d.find_by_arg_or_default_key("load", "by_generator")();
  return points;
}

template <class intT, class uintT>
parray<point3d> load_points3d() {
  parray<point3d> points;
  intT n = (intT)pasl::util::cmdline::parse_or_default_long("n", 10);
  pasl::util::cmdline::argmap_dispatch d;
  d.add("from_file", [&] {
    pasl::util::atomic::die("todo");
  });
  d.add("by_generator", [&] {
    pasl::util::cmdline::argmap_dispatch d;
    d.add("plummer", [&] {
      points = plummer3d<intT,uintT>(n);
    });
    d.add("uniform", [&] {
      bool inSphere = pasl::util::cmdline::parse_or_default_bool("in_sphere", false);
      bool onSphere = pasl::util::cmdline::parse_or_default_bool("on_sphere", false);
      points = uniform3d<intT,uintT>(inSphere, onSphere, n);
    });
    d.find_by_arg_or_default_key("generator", "plummer")();
  });
  d.find_by_arg_or_default_key("load", "by_generator")();
  return points;
}

} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PCTL_GEOMETRY_IO_H_ */