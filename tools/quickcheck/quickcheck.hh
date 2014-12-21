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
 * \file quickcheck.hh
 *
 * QuickCheck++ utility header.
 *
 * This file includes necessary files to use QuickCheck++ and defines some
 * macros for easy property creation and verification.
 */


#ifndef QUICKCHECK_QUICKCHECK_H
#define QUICKCHECK_QUICKCHECK_H

#include <iostream>

#include "ostream.hh"
#include "Property.hh"

/**
 * All classes, data generators and printers exported by QuickCheck++ are in
 * the quickcheck namespace.
 *
 * \todo tools to build test suites
 */
namespace quickcheck {

/**
 * Creates and verifies a property. Prints "Checking that\c msg..." and then
 * instantiate and verifies the property \c Prop.
 *
 * \tparam Prop the property type
 *
 * \param msg       the message identifying the property
 * \param n         the number of random tests to run
 * \param max       the maximum number of attempts to generate valid
 *                  input (defaults to <tt>5 * n</tt> if lower than \c n)
 * \param isVerbose \c true if input should be printed before each test
 * \param out       the output stream to use
 *
 * \return \c true if verification succeeded and \c false otherwise
 */
template<class Prop>
bool check(const char *msg, size_t n = 100, size_t max = 0,
           bool isVerbose = false, std::ostream& out = std::cout)
{
   std::cout << "* Checking that " << msg << "..." << std::endl;
   Prop theProperty;
   bool status = theProperty.check(n, max, isVerbose, out);
   std::cout << std::endl;
   return status;
}

}

#endif // !QUICKCHECK_QUICKCHECK_H
