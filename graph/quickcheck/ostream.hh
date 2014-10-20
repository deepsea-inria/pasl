/*
 * Copyright (C) 2009  Cyril Soldani
 * 
 * This file is part of QuickCheck++.
 * 
 * QuickCheck++ is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 * 
 * QuickCheck++ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * QuickCheck++. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file ostream.hh
 *
 * Implements insertion operator << for some basic types.
 *
 * All QuickCheck++ testable elements should be printable. This files extends
 * standard library with an implementation of insertion operator << for some
 * common types.
 *
 * \todo implement insertion operator for more containers
 */

#ifndef QUICKCHECK_OSTREAM_H
#define QUICKCHECK_OSTREAM_H

#include <iostream>
#include <vector>

namespace quickcheck {

/**
 * Insertion operator for vectors.
 *
 * \param out the output stream to be printed on
 * \param xs  the vector to print
 *
 * \return the output stream after printing
 */
template<class A>
std::ostream& operator<<(std::ostream& out, const std::vector<A>& xs)
{
   out << "[";
   for (size_t i = 0; i < xs.size(); ++i)
      if (i == xs.size() - 1)
         out << xs[i];
      else
         out << xs[i] << ", ";
   return out << "]";
}

}

#endif // !QUICKCHECK_OSTREAM_H
