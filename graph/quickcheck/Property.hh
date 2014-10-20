/*
 * Copyright (C) 2009-2012  Cyril Soldani
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
 * \file Property.hh
 *
 * Contains classes to define properties about code.
 */

#ifndef QUICKCHECK_PROPERTY_H
#define QUICKCHECK_PROPERTY_H

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "generate.hh"

namespace quickcheck {

/**
 * A dummy type to help simulating variadic templates. The name comes from type
 * theory (\c Unit is a type containing a single element, generally also called
 * \c unit, and often denoted by \c ()).
 */
enum Unit { UNIT };

/**
 * Prints an argument. This template is used internally in
 * PropertyBase to print input in case of failure or when
 * verbose checking is requested.
 *
 * \tparam A   the argument type
 *
 * \param out the output stream to use
 * \param n   the argument number
 * \param a   the argument
 */
template<class A>
void printArgument(std::ostream& out, size_t n, const A& a)
{
   out << "  " << n << ": " << a << std::endl;
}

/**
 * Specialisation of #printArgument for \c Unit. Prints nothing.
 */
template<>
void printArgument(std::ostream&, size_t, const Unit&) { }

/**
 * Generic property. This class models a verifiable property about some code
 * fragment. It is not meant to be used or derived from directly but rather
 * serves as a base for the various <em>n</em>-argument classes Property. This
 * is just an artefact due to the fact that C++ 98 does not support variadic
 * templates. It can model up to 5-arguments properties.
 *
 * \tparam A first argument type
 * \tparam B second argument type or \c Unit if less than two arguments
 * \tparam C third argument type or \c Unit if less than three arguments
 * \tparam D fourth argument type or \c Unit if less than four arguments
 * \tparam E fifth argument type or \c Unit if less than five arguments
 */
template<class A, class B, class C, class D, class E>
class PropertyBase {

   public:

      /** Constructor. */
      explicit PropertyBase();

      /** Destructor. */
      virtual ~PropertyBase();

      /**
       * Checks this property. This method will verify that this property holds
       * for all the fixed inputs and for a number of randomly generated
       * inputs. It prints messages about success or failure (and optionally
       * process) on the given output stream.
       *
       * \param n         the number of random tests to run
       * \param max       the maximum number of attempts to generate valid
       *                  input (defaults to <tt>5 * n</tt> if lower than \c n)
       * \param isVerbose \c true if input should be printed before each test
       * \param out       the output stream to use
       *
       * \return \c true if all tests succeeded and \c false if a test failed
       * or if a sufficient number of tests could not be generated
       */
      bool check(size_t n = 100, size_t max = 0, bool isVerbose = false,
                 std::ostream& out = std::cout);

   protected:

      /**
       * Adds a fixed test case for with the given arguments. This function
       * allows to ensure that some carefully chosen test cases will be checked
       * in addition to the randomly-generated ones.
       *
       * \param a the first argument of the test case
       * \param b the second argument of the test case
       * \param c the third argument of the test case
       * \param d the fourth argument of the test case
       * \param e the fifth argument of the test case
       */
      void _addFixed(const A& a, const B& b, const C& c, const D& d,
                     const E& e);

   private:

      /**
       * Input type. Just a 5-tuple corresponding to the possible 5 property
       * input arguments.
       */
      struct Input {
         A a;
         B b;
         C c;
         D d;
         E e;
      };

      /**
       * Prints the given input arguments on the given output stream.
       *
       * \param out the output stream to use
       * \param a   the first argument
       * \param b   the second argument or \c UNIT if less than two arguments
       * \param c   the third argument or \c UNIT if less than three arguments
       * \param d   the fourth argument or \c UNIT if less than four arguments
       * \param e   the fifth argument or \c UNIT if less than five arguments
       */
      void printInput(std::ostream& out, const A& a, const B& b, const C& c,
                      const D& d, const E& e);

      /**
       * Converts the random test number into a size hint.
       *
       * \param testNo the number of the current random test
       *
       * \return a size hint for #_generateInput
       */
      virtual size_t sizeHint(size_t testNo);

      /**
       * Generic wrapper for Property::accepts.
       *
       * \param a the first argument
       * \param b the second argument or \c UNIT if less than two arguments
       * \param c the third argument or \c UNIT if less than three arguments
       * \param d the fourth argument or \c UNIT if less than four arguments
       * \param e the fifth argument or \c UNIT if less than five arguments
       *
       * \return \c true if arguments forms valid input and \c false otherwise
       */
      virtual bool _accepts(const A& a, const B& b, const C& c, const D& d,
                            const E& e) = 0;

      /**
       * Generic wrapper for Property::classify.
       *
       * \param a the first argument
       * \param b the second argument or \c UNIT if less than two arguments
       * \param c the third argument or \c UNIT if less than three arguments
       * \param d the fourth argument or \c UNIT if less than four arguments
       * \param e the fifth argument or \c UNIT if less than five arguments
       *
       * \return a string representing the input class
       */
      virtual const std::string _classify(const A& a, const B& b, const C& c,
                                          const D& d, const E& e) = 0;

      /**
       * Generic wrapper for Property::generateInput.
       *
       * \param n the size hint
       * \param a the first argument
       * \param b the second argument or \c UNIT if less than two arguments
       * \param c the third argument or \c UNIT if less than three arguments
       * \param d the fourth argument or \c UNIT if less than four arguments
       * \param e the fifth argument or \c UNIT if less than five arguments
       */
      virtual void _generateInput(size_t n, A& a, B& b, C& c, D& d, E& e) = 0;

      /**
       * Generic wrappper for Property::holdsFor.
       *
       * \param a the first argument
       * \param b the second argument or \c UNIT if less than two arguments
       * \param c the third argument or \c UNIT if less than three arguments
       * \param d the fourth argument or \c UNIT if less than four arguments
       * \param e the fifth argument or \c UNIT if less than five arguments
       *
       * \return \c true if the property holds and \c false otherwise
       */
      virtual bool _holdsFor(const A& a, const B& b, const C& c, const D& d,
                             const E& e) = 0;

      /**
       * Generic wrapper for Property::isTrivialFor.
       *
       * \param a the first argument
       * \param b the second argument or \c UNIT if less than two arguments
       * \param c the third argument or \c UNIT if less than three arguments
       * \param d the fourth argument or \c UNIT if less than four arguments
       * \param e the fifth argument or \c UNIT if less than five arguments
       *
       * \return \c true if the property is trivial for given arguments and \c
       * false otherwise
       */
      virtual bool _isTrivialFor(const A& a, const B& b, const C& c,
                                 const D& d, const E& e) = 0;

      /**
       * A vector of fixed inputs that are tested in addition to
       * randomly-generated inputs.
       */
      std::vector<Input> _fixedInputs;

};

template<class A, class B, class C, class D, class E>
PropertyBase<A, B, C, D, E>::PropertyBase() :
   _fixedInputs(std::vector<Input>())
{
   // Nothing more to do than what was done in initialisation list
}

template<class A, class B, class C, class D, class E>
PropertyBase<A, B, C, D, E>::~PropertyBase()
{
   // Nothing to do, only defined for derivation purpose
}

template<class A, class B, class C, class D, class E>
bool PropertyBase<A, B, C, D, E>::check(size_t n, size_t max, bool isVerbose,
                                        std::ostream& out)
{
   if (max < n)
      max = 5 * n;
   size_t nTrivial = 0;
   // ClassMap maps input classes to the number of their occurrence in the
   // tests
   typedef std::map<const std::string, size_t> ClassMap;
   ClassMap classes;
   // First verifies fixed inputs
   size_t len = _fixedInputs.size();
   size_t testNo;
   for (testNo = 0; testNo < len; ++testNo) {
      const Input& input = _fixedInputs[testNo];
      if (_isTrivialFor(input.a, input.b, input.c, input.d, input.e))
         ++nTrivial;
      ++classes[_classify(input.a, input.b, input.c, input.d, input.e)];
      if (isVerbose) {
         out << "Test " << testNo << ":" << std::endl;
         printInput(out, input.a, input.b, input.c, input.d, input.e);
      }
      if (!_holdsFor(input.a, input.b, input.c, input.d, input.e)) {
         out << "Falsifiable after " << testNo + 1 << " tests for input:" <<
            std::endl;
         printInput(out, input.a, input.b, input.c, input.d, input.e);
         return false;
      }
   }
   // And now the random tests
   n += len;
   size_t attemptNo;
   for (attemptNo = 0; attemptNo < max && testNo < n; ++attemptNo) {
      // Generates random arguments (that may or may not form valid input).
      // Size hint is random test number, so that we begin with small values
      // before going on with larger values.
      A a;
      B b;
      C c;
      D d;
      E e;
      _generateInput(sizeHint(testNo - len), a, b, c, d, e);
      // Only do the test if generated input is valid
      if (_accepts(a, b, c, d, e)) {
         if (_isTrivialFor(a, b, c, d, e))
            ++nTrivial;
         ++classes[_classify(a, b, c, d, e)];
         if (isVerbose) {
            out << "Test " << testNo << ":" << std::endl;
            printInput(out, a, b, c, d, e);
         }
         if (!_holdsFor(a, b, c, d, e)) {
            out << "Falsifiable after " << testNo + 1 << " tests for input:"
               << std::endl;
            printInput(out, a, b, c, d, e);
            return false;
         }
         ++testNo;
      }
   }
   // Prints summary results
   if (testNo < n)
      out << "Arguments exhausted after ";
   else
      out << "OK, passed ";
   out << testNo << " tests";
   if (nTrivial > 0)
      out << " (" << nTrivial * 100 / testNo << "% trivial)";
   out << "." << std::endl;
   // Prints input distribution if necessary
   classes.erase("");
   if (!classes.empty()) {
      // We sort input classes by number of occurrences by building a sorted
      // vector of (occurrences, class) from the class map
      typedef std::vector< std::pair<size_t, std::string> > ClassVec;
      ClassVec sorted;
      for (ClassMap::const_iterator i = classes.begin(); i != classes.end();
           ++i) {
         std::pair<size_t, std::string> elem = make_pair(i->second, i->first);
         ClassVec::iterator pos = lower_bound(sorted.begin(), sorted.end(),
                                              elem);
         sorted.insert(pos, elem);
      }
      std::reverse(sorted.begin(), sorted.end());
      for (ClassVec::const_iterator i = sorted.begin(); i != sorted.end(); ++i)
         out << std::setw(4) << i->first * 100 / testNo << "% " << i->second
            << std::endl;
   }
   return (testNo == n);
}

template<class A, class B, class C, class D, class E>
void PropertyBase<A, B, C, D, E>::_addFixed(const A& a, const B& b, const C& c,
                                            const D& d, const E& e)
{
   Input i = { a, b, c, d, e };
   _fixedInputs.push_back(i);
}

template<class A, class B, class C, class D, class E>
void PropertyBase<A, B, C, D, E>::printInput(std::ostream& out, const A& a,
                                             const B& b, const C& c,
                                             const D& d, const E& e)
{
   // We call printArgument for all arguments as it should print nothing when
   // given UNIT
   printArgument(out, 0, a);
   printArgument(out, 1, b);
   printArgument(out, 2, c);
   printArgument(out, 3, d);
   printArgument(out, 4, e);
}

template<class A, class B, class C, class D, class E>
size_t PropertyBase<A, B, C, D, E>::sizeHint(size_t testNo)
{
   return testNo / 2 + 3;
}

/**
 * 5-argument property. This class models a property (see PropertyBase) with
 * 5-argument input. Default parameters serve the purpose of simulating
 * variadic templates.
 *
 * \eg Property<A, B, C> (which is the same as Property<A, B, C, Unit, Unit>)
 * is in fact a property of 3 arguments.
 *
 * All templates arguments should have both a
 * \ref quickcheck::generate "generator" and an output stream operator.
 *
 * Specialisation of this template class are provided for properties of 1, 2, 3
 * and 4 arguments.
 *
 * \tparam A type of first argument
 * \tparam B type of second argument (if \c Unit, then all later arguments
 *           should also be \c Unit and 1-argument property will be used
 *           instead)
 * \tparam C type of third argument (if \c Unit, then all later arguments
 *           should also be \c Unit and <em>n</em>-argument property (
 *           <em>n</em> = 1 or 2) will be used instead)
 * \tparam D type of fourth argument (if \c Unit, then all later arguments
 *           should also be \c Unit and <em>n</em>-argument property (
 *           <em>n</em> = 1, 2 or 3) will be used instead)
 * \tparam E type of fourth argument (if \c Unit, then <em>n</em>-argument
 *           property (<em>n</em> = 1, 2, 3 or 4) will be used instead)
 */
template<class A, class B = Unit, class C = Unit, class D = Unit,
         class E = Unit>
class Property : public PropertyBase<A, B, C, D, E> {

   public:

      /**
       * \copybrief PropertyBase::_addFixed
       *
       * \param a the first argument of the test case
       * \param b the second argument of the test case
       * \param c the third argument of the test case
       * \param d the fourth argument of the test case
       * \param e the fifth argument of the test case
       */
      virtual void addFixed(const A& a, const B& b, const C& c, const D& d,
                            const E& e);

   private:

      /**
       * Tells whether or not this property should accept given arguments. This
       * predicate is used to filter generated input arguments to respect this
       * property possible precondition.
       *
       * Note that if too much inputs are rejected, the property may fail
       * verification due to the impossibility to generate a sufficient number
       * of valid inputs in the given number of attempts. If this occurs, you
       * can augment the maximum number of allowed attempts when calling #check
       * or define a custom generator #generateInput.
       *
       * Also pay attention to the fact that filtering input arguments may lead
       * to a biased distribution which is not representative of the set of
       * valid inputs. You should inspect your input distribution with
       * #isTrivialFor or #classify when using this predicate and you will
       * often do better by defining a custom generator by re-implementing
       * #generateInput.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       * \param e the fifth argument
       *
       * \return \c true if arguments forms valid input and \c false otherwise
       */
      virtual bool accepts(const A& a, const B& b, const C& c, const D& d,
                           const E& e);

      /**
       * Classifies input to allow observation of input distribution. An input
       * class is simply represented by a string which is used both as an input
       * class identifier (\ie same class string means same input class) and as
       * output to the user. The empty string "" is used as a no-class tag and
       * is not printed in input distribution.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       * \param e the fifth argument
       *
       * \return the input class associated to input or empty string "" if none
       */
      virtual const std::string classify(const A& a, const B& b, const C& c,
                                         const D& d, const E& e);

      /**
       * Generates input randomly. Override this method if you need a custom
       * input generator.
       *
       * All input arguments given here are <em>out</em> values, \ie they can
       * be assumed default or empty on entry and should be initialised to a
       * sensible random value by this method.
       *
       * The size of generated values should be somehow correlated to the size
       * hint \c n. When calling generators recursively to build compound data
       * structures, you should pass a lower size hint to compound member
       * generators to avoid the construction of overly large (or even
       * infinite) data structures.
       *
       * The #quickcheck namespace contains a lot of generators that can be
       * used to build your owns.
       *
       * \param n the size hint
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       * \param e the fifth argument
       */
      virtual void generateInput(size_t n, A& a, B& b, C& c, D& d, E& e);

      /**
       * Tells whether or not this property holds for the given input. This
       * method is the core of the property which define what the property is
       * all about. If the code under test is correct, it should return \c true
       * for all valid inputs. If the code under test is not correct, it should
       * return \c false for at least some valid input.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       * \param e the fifth argument
       *
       * \return \c true if the property holds for given input and \c false
       * otherwise
       */
      virtual bool holdsFor(const A& a, const B& b, const C& c, const D& d,
                            const E& e) = 0;

      /**
       * Tells whether or not the property is trivially true for the given
       * input. This predicate allows to ensure that a sufficient number of
       * tests were <em>interesting</em>. It is a restricted form of input
       * classification.
       *
       * \see classify
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       * \param e the fifth argument
       *
       * \return \c true if the property is trivial for given input and \c
       * false otherwise
       */
      virtual bool isTrivialFor(const A& a, const B& b, const C& c, const D& d,
                                const E& e);

      bool _accepts(const A& a, const B& b, const C& c, const D& d,
                    const E& e);

      const std::string _classify(const A& a, const B& b, const C& c,
                                  const D& d, const E& e);

      void _generateInput(size_t n, A& a, B& b, C& c, D& d, E& e);

      bool _holdsFor(const A& a, const B& b, const C& c, const D& d,
                     const E& e);

      bool _isTrivialFor(const A& a, const B& b, const C& c, const D& d,
                         const E& e);

};

template<class A, class B, class C, class D, class E>
void Property<A, B, C, D, E>::addFixed(const A& a, const B& b, const C& c,
                                       const D& d, const E& e)
{
   this->_addFixed(a, b, c, d, e);
}

template<class A, class B, class C, class D, class E>
bool Property<A, B, C, D, E>::accepts(const A&, const B&, const C&, const D&,
                                      const E&)
{
   return true;
}

template<class A, class B, class C, class D, class E>
const std::string Property<A, B, C, D, E>::classify(
   const A&, const B&, const C&, const D&, const E&)
{
   return "";
}

template<class A, class B, class C, class D, class E>
void Property<A, B, C, D, E>::generateInput(size_t n,
                                            A& a, B& b, C& c, D& d, E& e)
{
   generate(n, a);
   generate(n, b);
   generate(n, c);
   generate(n, d);
   generate(n, e);
}

template<class A, class B, class C, class D, class E>
bool Property<A, B, C, D, E>::isTrivialFor(const A&, const B&, const C&,
                                           const D&, const E&)
{
   return false;
}

template<class A, class B, class C, class D, class E>
bool Property<A, B, C, D, E>::_accepts(const A& a, const B& b, const C& c,
                                       const D& d, const E& e)
{
   return accepts(a, b, c, d, e);
}

template<class A, class B, class C, class D, class E>
const std::string Property<A, B, C, D, E>::_classify(
   const A& a, const B& b, const C& c, const D& d, const E& e)
{
   return classify(a, b, c, d, e);
}

template<class A, class B, class C, class D, class E>
void Property<A, B, C, D, E>::_generateInput(size_t n,
                                             A& a, B& b, C& c, D& d, E& e)
{
   generateInput(n, a, b, c, d, e);
}

template<class A, class B, class C, class D, class E>
bool Property<A, B, C, D, E>::_holdsFor(const A& a, const B& b, const C& c,
                                        const D& d, const E& e)
{
   return holdsFor(a, b, c, d, e);
}

template<class A, class B, class C, class D, class E>
bool Property<A, B, C, D, E>::_isTrivialFor(const A& a, const B& b, const C& c,
                                            const D& d, const E& e)
{
   return isTrivialFor(a, b, c, d, e);
}

/**
 * 4-argument property. This class models a property (see PropertyBase) with
 * 4-argument input.
 *
 * \tparam A the type of the first argument
 * \tparam B the type of the second argument
 * \tparam C the type of the third argument
 * \tparam D the type of the fourth argument
 */
template<class A, class B, class C, class D>
class Property<A, B, C, D> : public PropertyBase<A, B, C, D, Unit> {

   public:

      /**
       * \copybrief Property::addFixed
       *
       * 4-argument counterpart of Property::addFixed.
       *
       * \param a the first argument of the test case
       * \param b the second argument of the test case
       * \param c the third argument of the test case
       * \param d the fourth argument of the test case
       */
      virtual void addFixed(const A& a, const B& b, const C& c, const D& d);

   private:

      /**
       * \copybrief Property::accepts
       *
       * 4-argument counterpart of Property::accepts.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       */
      virtual bool accepts(const A& a, const B& b, const C& c, const D& d);

      /**
       * \copybrief Property::classify
       *
       * 4-argument counterpart of Property::classify.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       */
      virtual const std::string classify(const A& a, const B& b, const C& c,
                                         const D& d);

      /**
       * \copybrief Property::generateInput
       *
       * 4-argument counterpart of Property::generateInput.
       *
       * \param n the size hint
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       */
      virtual void generateInput(size_t n, A& a, B& b, C& c, D& d);

      /**
       * \copybrief Property::holdsFor
       *
       * 4-argument counterpart of Property::holdsFor.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       */
      virtual bool holdsFor(const A& a, const B& b, const C& c, const D& d)
         = 0;

      /**
       * \copybrief Property::isTrivialFor
       *
       * 4-argument counterpart of Property::isTrivialFor.
       *
       * \param a the first argument
       * \param b the second argument
       * \param c the third argument
       * \param d the fourth argument
       */
      virtual bool isTrivialFor(const A& a, const B& b, const C& c,
                                const D& d);

      bool _accepts(const A& a, const B& b, const C& c, const D& d,
                    const Unit& e);

      const std::string _classify(const A& a, const B& b, const C& c,
                                  const D& d, const Unit& e);

      void _generateInput(size_t n, A& a, B& b, C& c, D& d, Unit& e);

      bool _holdsFor(const A& a, const B& b, const C& c, const D& d,
                     const Unit& e);

      bool _isTrivialFor(const A& a, const B& b, const C& c, const D& d,
                         const Unit& e);

};

template<class A, class B, class C, class D>
void Property<A, B, C, D>::addFixed(const A& a, const B& b, const C& c,
                                    const D& d)
{
   this->_addFixed(a, b, c, d, UNIT);
}

template<class A, class B, class C, class D>
bool Property<A, B, C, D>::accepts(const A&, const B&, const C&, const D&)
{
   return true;
}

template<class A, class B, class C, class D>
const std::string Property<A, B, C, D>::classify(const A&, const B&, const C&,
                                                 const D&)
{
   return "";
}

template<class A, class B, class C, class D>
void Property<A, B, C, D>::generateInput(size_t n, A& a, B& b, C& c, D& d)
{
   generate(n, a);
   generate(n, b);
   generate(n, c);
   generate(n, d);
}

template<class A, class B, class C, class D>
bool Property<A, B, C, D>::isTrivialFor(const A&, const B&, const C&,
                                        const D&)
{
   return false;
}

template<class A, class B, class C, class D>
bool Property<A, B, C, D>::_accepts(const A& a, const B& b, const C& c,
                                    const D& d, const Unit&)
{
   return accepts(a, b, c, d);
}

template<class A, class B, class C, class D>
const std::string Property<A, B, C, D>::_classify(
   const A& a, const B& b, const C& c, const D& d, const Unit&)
{
   return classify(a, b, c, d);
}

template<class A, class B, class C, class D>
void Property<A, B, C, D>::_generateInput(size_t n, A& a, B& b, C& c, D& d, 
                                          Unit&)
{
   generateInput(n, a, b, c, d);
}

template<class A, class B, class C, class D>
bool Property<A, B, C, D>::_holdsFor(const A& a, const B& b, const C& c,
                                     const D& d, const Unit&)
{
   return holdsFor(a, b, c, d);
}

template<class A, class B, class C, class D>
bool Property<A, B, C, D>::_isTrivialFor(const A& a, const B& b, const C& c,
                                         const D& d, const Unit&)
{
   return isTrivialFor(a, b, c, d);
}

/**
 * 3-argument property. This class models a property (see PropertyBase) with
 * 3-argument input.
 *
 * \tparam A the type of the first argument
 * \tparam B the type of the second argument
 * \tparam C the type of the third argument
 */
template<class A, class B, class C>
class Property<A, B, C> : public PropertyBase<A, B, C, Unit, Unit> {

   public:

   /**
    * \copybrief Property::addFixed
    *
    * 3-argument counterpart of Property::addFixed.
    *
    * \param a the first argument of the test case
    * \param b the second argument of the test case
    * \param c the third argument of the test case
    */
   virtual void addFixed(const A& a, const B& b, const C& c);

   private:

   /**
    * \copybrief Property::accepts
    *
    * 3-argument counterpart of Property::accepts.
    *
    * \param a the first argument
    * \param b the second argument
    * \param c the third argument
    */
   virtual bool accepts(const A& a, const B& b, const C& c);

   /**
    * \copybrief Property::classify
    *
    * 3-argument counterpart of Property::classify.
    *
    * \param a the first argument
    * \param b the second argument
    * \param c the third argument
    */
   virtual const std::string classify(const A& a, const B& b, const C& c);

   /**
    * \copybrief Property::generateInput
    *
    * 3-argument counterpart of Property::generateInput.
    *
    * \param n the size hint
    * \param a the first argument
    * \param b the second argument
    * \param c the third argument
    */
   virtual void generateInput(size_t n, A& a, B& b, C& c);

   /**
    * \copybrief Property::holdsFor
    *
    * 3-argument counterpart of Property::holdsFor.
    *
    * \param a the first argument
    * \param b the second argument
    * \param c the third argument
    */
   virtual bool holdsFor(const A& a, const B& b, const C& c) = 0;

   /**
    * \copybrief Property::isTrivialFor
    *
    * 3-argument counterpart of Property::isTrivialFor.
    *
    * \param a the first argument
    * \param b the second argument
    * \param c the third argument
    */
   virtual bool isTrivialFor(const A& a, const B& b, const C& c);

   bool _accepts(const A& a, const B& b, const C& c, const Unit& d,
                 const Unit& e);

   const std::string _classify(const A& a, const B& b, const C& c,
                               const Unit& d, const Unit& e);

   void _generateInput(size_t n, A& a, B& b, C& c, Unit& d, Unit& e);

   bool _holdsFor(const A& a, const B& b, const C& c, const Unit& d,
                  const Unit& e);

   bool _isTrivialFor(const A& a, const B& b, const C& c, const Unit& d,
                      const Unit& e);

};

template<class A, class B, class C>
void Property<A, B, C>::addFixed(const A& a, const B& b, const C& c)
{
   this->_addFixed(a, b, c, UNIT, UNIT);
}

template<class A, class B, class C>
bool Property<A, B, C>::accepts(const A&, const B&, const C&)
{
   return true;
}

template<class A, class B, class C>
const std::string Property<A, B, C>::classify(const A&, const B&, const C&)
{
   return "";
}

template<class A, class B, class C>
void Property<A, B, C>::generateInput(size_t n, A& a, B& b, C& c)
{
   generate(n, a);
   generate(n, b);
   generate(n, c);
}

template<class A, class B, class C>
bool Property<A, B, C>::isTrivialFor(const A&, const B&, const C&)
{
   return false;
}

template<class A, class B, class C>
bool Property<A, B, C>::_accepts(const A& a, const B& b, const C& c,
                                 const Unit&, const Unit&)
{
   return accepts(a, b, c);
}

template<class A, class B, class C>
const std::string Property<A, B, C>::_classify(
   const A& a, const B& b, const C& c, const Unit&, const Unit&)
{
   return classify(a, b, c);
}

template<class A, class B, class C>
void Property<A, B, C>::_generateInput(size_t n, A& a, B& b, C& c, Unit&, 
                                       Unit&)
{
   generateInput(n, a, b, c);
}

template<class A, class B, class C>
bool Property<A, B, C>::_holdsFor(const A& a, const B& b, const C& c,
                                  const Unit&, const Unit&)
{
   return holdsFor(a, b, c);
}

template<class A, class B, class C>
bool Property<A, B, C>::_isTrivialFor(const A& a, const B& b, const C& c,
                                      const Unit&, const Unit&)
{
   return isTrivialFor(a, b, c);
}

/**
 * 2-argument property. This class models a property (see PropertyBase) with
 * 2-argument input.
 *
 * \tparam A the type of the first argument
 * \tparam B the type of the second argument
 */
template<class A, class B>
class Property<A, B> : public PropertyBase<A, B, Unit, Unit, Unit> {

   public:

      /**
       * \copybrief Property::addFixed
       *
       * 2-argument counterpart of Property::addFixed.
       *
       * \param a the first argument of the test case
       * \param b the second argument of the test case
       */
      virtual void addFixed(const A& a, const B& b);

   private:

      /**
       * \copybrief Property::accepts
       *
       * 2-argument counterpart of Property::accepts.
       *
       * \param a the first argument
       * \param b the second argument
       */
      virtual bool accepts(const A& a, const B& b);

      /**
       * \copybrief Property::classify
       *
       * 2-argument counterpart of Property::classify.
       *
       * \param a the first argument
       * \param b the second argument
       */
      virtual const std::string classify(const A& a, const B& b);

      /**
       * \copybrief Property::generateInput
       *
       * 2-argument counterpart of Property::generateInput.
       *
       * \param n the size hint
       * \param a the first argument
       * \param b the second argument
       */
      virtual void generateInput(size_t n, A& a, B& b);

      /**
       * \copybrief Property::holdsFor
       *
       * 2-argument counterpart of Property::holdsFor.
       *
       * \param a the first argument
       * \param b the second argument
       */
      virtual bool holdsFor(const A& a, const B& b) = 0;

      /**
       * \copybrief Property::isTrivialFor
       *
       * 2-argument counterpart of Property::isTrivialFor.
       *
       * \param a the first argument
       * \param b the second argument
       */
      virtual bool isTrivialFor(const A& a, const B& b);

      bool _accepts(const A& a, const B& b, const Unit& c, const Unit& d,
                    const Unit& e);

      const std::string _classify(const A& a, const B& b, const Unit& c,
                                  const Unit& d, const Unit& e);

      void _generateInput(size_t n, A& a, B& b, Unit& c, Unit& d, Unit& e);

      bool _holdsFor(const A& a, const B& b, const Unit& c, const Unit& d,
                     const Unit& e);

      bool _isTrivialFor(const A& a, const B& b, const Unit& c, const Unit& d,
                         const Unit& e);

};

template<class A, class B>
void Property<A, B>::addFixed(const A& a, const B& b)
{
   this->_addFixed(a, b, UNIT, UNIT, UNIT);
}

template<class A, class B>
bool Property<A, B>::accepts(const A&, const B&)
{
   return true;
}

template<class A, class B>
const std::string Property<A, B>::classify(const A&, const B&)
{
   return "";
}

template<class A, class B>
void Property<A, B>::generateInput(size_t n, A& a, B& b)
{
   generate(n, a);
   generate(n, b);
}

template<class A, class B>
bool Property<A, B>::isTrivialFor(const A&, const B&)
{
   return false;
}

template<class A, class B>
bool Property<A, B>::_accepts(const A& a, const B& b, const Unit&,
                              const Unit&, const Unit&)
{
   return accepts(a, b);
}

template<class A, class B>
const std::string Property<A, B>::_classify(
   const A& a, const B& b, const Unit&, const Unit&, const Unit&)
{
   return classify(a, b);
}

template<class A, class B>
void Property<A, B>::_generateInput(size_t n, A& a, B& b, Unit&, Unit&, Unit&)
{
   generateInput(n, a, b);
}

template<class A, class B>
bool Property<A, B>::_holdsFor(const A& a, const B& b, const Unit&,
                               const Unit&, const Unit&)
{
   return holdsFor(a, b);
}

template<class A, class B>
bool Property<A, B>::_isTrivialFor(const A& a, const B& b, const Unit&,
                                   const Unit&, const Unit&)
{
   return isTrivialFor(a, b);
}

/**
 * 1-argument property. This class models a property (see PropertyBase) with
 * 1-argument input.
 *
 * \tparam A the type of the only argument
 */
template<class A>
class Property<A> : public PropertyBase<A, Unit, Unit, Unit, Unit> {

   public:

      /**
       * \copybrief Property::addFixed
       *
       * 1-argument counterpart of Property::addFixed.
       *
       * \param a the first argument of the test case
       */
      virtual void addFixed(const A& a);

   private:

      /**
       * \copybrief Property::accepts
       *
       * 1-argument counterpart of Property::accepts.
       *
       * \param a the only argument
       */
      virtual bool accepts(const A& a);

      /**
       * \copybrief Property::classify
       *
       * 1-argument counterpart of Property::classify.
       *
       * \param a the only argument
       */
      virtual const std::string classify(const A& a);

      /**
       * \copybrief Property::generateInput
       *
       * 1-argument counterpart of Property::generateInput.
       *
       * \param n the size hint
       * \param a the only argument
       */
      virtual void generateInput(size_t n, A& a);

      /**
       * \copybrief Property::holdsFor
       *
       * 1-argument counterpart of Property::holdsFor.
       *
       * \param a the only argument
       */
      virtual bool holdsFor(const A& a) = 0;

      /**
       * \copybrief Property::isTrivialFor
       *
       * 1-argument counterpart of Property::isTrivialFor.
       *
       * \param a the only argument
       */
      virtual bool isTrivialFor(const A& a);

      bool _accepts(const A& a, const Unit& b, const Unit& c, const Unit& d,
                    const Unit& e);

      const std::string _classify(const A& a, const Unit& b, const Unit& c,
                                  const Unit& d, const Unit& e);

      void _generateInput(size_t n, A& a, Unit& b, Unit& c, Unit& d, Unit& e);

      bool _holdsFor(const A& a, const Unit& b, const Unit& c, const Unit& d,
                     const Unit& e);

      bool _isTrivialFor(const A& a, const Unit& b, const Unit& c,
                         const Unit& d, const Unit& e);

};

template<class A>
void Property<A>::addFixed(const A& a)
{
   this->_addFixed(a, UNIT, UNIT, UNIT, UNIT);
}

template<class A>
bool Property<A>::accepts(const A&)
{
   return true;
}

template<class A>
const std::string Property<A>::classify(const A&)
{
   return "";
}

template<class A>
void Property<A>::generateInput(size_t n, A& a)
{
   generate(n, a);
}

template<class A>
bool Property<A>::isTrivialFor(const A&)
{
   return false;
}

template<class A>
bool Property<A>::_accepts(const A& a, const Unit&, const Unit&, const Unit&,
                           const Unit&)
{
   return accepts(a);
}

template<class A>
const std::string Property<A>::_classify(const A& a, const Unit&, const Unit&,
                                         const Unit&, const Unit&)
{
   return classify(a);
}

template<class A>
void Property<A>::_generateInput(size_t n, A& a, Unit&, Unit&, Unit&, Unit&)
{
   generateInput(n, a);
}

template<class A>
bool Property<A>::_holdsFor(const A& a, const Unit&, const Unit&, const Unit&,
                            const Unit&)
{
   return holdsFor(a);
}

template<class A>
bool Property<A>::_isTrivialFor(const A& a, const Unit&, const Unit&,
                                const Unit&, const Unit&)
{
   return isTrivialFor(a);
}

}

#endif // !QUICKCHECK_PROPERTY_H
