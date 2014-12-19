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

/**
\mainpage

\section toc Table of Contents

- \ref introduction
  - \ref what
  - \ref why "Why Would I Want to Use It?"
  - \ref how "How to Get It?"
  - \ref relation
  - \ref license
- \ref using
  - \ref defining
  - \ref verifying
  - \ref specifying
  - \ref inspecting
  - \ref custom
    - \ref general
    - \ref special
    - \ref data
  - \ref printing
  - \ref verbose
  - \ref fixed
- \ref how2
  - \ref test
  - \ref code
  - \ref promote

\section introduction Introduction

\subsection what What is QuickCheck++

QuickCheck++ is a tool for testing C++ programs automatically, inspired by
<a href="http://www.cse.chalmers.se/~rjmh/QuickCheck/">QuickCheck</a>, a
similar library for Haskell programs.

In QuickCheck++, the application programmer provides a specification of parts
of its code in the form of \e properties which this code must satisfy. Then,
the QuickCheck++ utilities can check that these properties holds in a large
number of randomly generated test cases.

Specifications, \ie properties, are written in C++ by deriving
from the quickcheck::Property class. This class contains members not only to
express the specification but also to observe the distribution of test data
and to write custom test data generators.

The framework also allows the specification of \ref fixed "fixed test data", as
can be done with more traditional unit testing frameworks.

\warning QuickCheck++ has been around since a few years now, but should still
be considered an experimental, best-effort project. It comes with absolutely no
warranty, see \ref license for detail.

\subsection why Why Would I Want to Use QuickCheck++?

Because you believe in test-driven development to provide better-designed,
more robust, self-documented software. Moreover, you like:
- precise, formal specifications rather than a few ad-hoc test cases ;
- running hundreds of tests cases without having to write them down first ;
- reduce the risk of writing test cases with the same bias as when writing
  the code (\eg QuickCheck++ will not let you write every test
  cases of a factorial function with only positive arguments when it also
  accepts (and crash with) negative ones).

\subsection how How to Get QuickCheck++?

Last released version of QuickCheck++ is available from
http://software.legiasoft.com/archives/quickcheck_0.0.3.tar.bz2 while current
development version is available through \c git:

\verbatim
git clone http://software.legiasoft.com/git/quickcheck.git
\endverbatim

\subsection relation Relation to Haskell's QuickCheck

Although QuickCheck++ was inspired by Haskell's QuickCheck, it is very
loosely related, both from user and implementation points of view. We did
not tried to mimic QuickCheck Haskell implementation in C++ and the
interface is also very different.

We choose a more traditional, imperative implementation, both due to lack
of sufficient knowledge in C++, and to the feeling it would have been
awkward to use. We would however be delighted to be proved wrong and would
welcome a more elegant, functional implementation.

As we are new to C++ (and hacked this quickly), our imperative
implementation could also be seriously improved. We welcome patches for this
one as well.

\subsection license License

<em>QuickCheck++ is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.</em>

<em>QuickCheck++ is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
more details.</em>

<em>You should have received a copy of the GNU General Public License along
with QuickCheck++. If not, see <http://www.gnu.org/licenses/>.</em>

We choose GPLv3 because we have a strong commitment towards free software.
However, if you feel this license is overly restrictive and inadequate for
your usage, we are willing to hear your arguments.

\section using Using QuickCheck++

You can find all the examples given in this section in the file \c examples.cc
provided with QuickCheck++.

To use QuickCheck++, you must include <tt>quickcheck/quickcheck.hh</tt> and
optionally use the #quickcheck namespace.

\code
#include "quickcheck/quickcheck.hh"
using namespace quickcheck;
\endcode

\subsection defining Defining Properties

You can specify a property by deriving the quickcheck::Property class and
implementing its quickcheck::Property::holdsFor method.

Here is a simple example. Lets assume you just wrote a function to reverse a
sequence for a type named \c Vector. A property of a reverse function is
that if you apply it to a vector twice, you should get back the original
vector. With QuickCheck++, this property of your reverse function can be
expressed as follows:

\code
class PReverseCancelsReverse : public Property<Vector> {
   bool holdsFor(const Vector& xs) {
      Vector ys = xs;
      reverse(ys);
      reverse(ys);
      return xs == ys;
   }
};
\endcode

The property is a class deriving from quickcheck::Property, specifying the
argument type (here \c Vector). This class must implement
quickcheck::Property::holdsFor which takes a const reference to the type
used when deriving quickcheck::Property, and returns \c true when the
property holds for the given input argument (and \c false otherwise).

\subsection verifying Verifying Properties

To verify a property, simply instantiate it and call its
quickcheck::Property::check method.

\code
PReverseCancelsReverse revRev;
revRev.check();
\endcode

This will output:

\verbatim
OK, passed 100 tests.
\endverbatim

You can specify the number of random tests to run as the first
quickcheck::Property::check argument.

\code
revRev.check(300);
\endcode

would output

\verbatim
OK, passed 300 tests.
\endverbatim

In case of failure, QuickCheck++ will display the faulty input, \eg

\code
class PReverseIsIdentity : public Property<Vector> {
   bool holdsFor(const Vector& xs) {
      Vector ys = xs;
      reverse(ys);  // reverse done only once!
      return xs == ys;
   }
};

PReverseIsIdentity revId;
revId.check();
\endcode

will output something in the line of:

\verbatim
Falsifiable after 2 tests for input:
  0: [-2, 1]
\endverbatim

On average, QuickCheck++ tries shorter arguments first, so that the faulty
input is easier to read and to use for debug.

\subsection specifying Specifying a Precondition

Imagine you want to test an insertion function that should insert an element
at the right place in a sorted vector. You can express the fact that your
insert function preserves the fact that the vector is sorted as follows:

\code
class PInsertKeepsSorted : public Property<Elem, Vector> {
   bool holdsFor(const Elem& x, const Vector& xs) {
      Vector ys = xs;
      insert(x, ys);
      return isSorted(ys);
   }
};
\endcode

However, if you try to verify this property as is, it will not work:

\verbatim
Falsifiable after 5 tests for input:
  0: 2
  1: [-4, 1, -3]
\endverbatim

The problem is that nothing enforces the fact that the input vector should
be sorted.

This can be specified as part of the property in the form of a predicate on
input arguments. Only tests for which the randomly generated arguments
satisfies the predicate will be taken into account. The method that acts as
predicate is quickcheck::Property::accepts.

\code
class PInsertKeepsSorted2 : public Property<Elem, Vector> {
   bool holdsFor(const Elem& x, const Vector& xs) {
      Vector ys = xs;
      insert(x, ys);
      return isSorted(ys);
   }
   bool accepts(const Elem& x, const Vector& xs) {
      return isSorted(xs);
   }
};
\endcode

However, if you try to check this property with default parameters, you will
still observe problems, \ie

\verbatim
Arguments exhausted after 66 tests.
\endverbatim

To avoid looping indefinitely in the case the given predicate is never (or
rarely) met, QuickCheck++ only tries a finite number of times to generate
valid test cases. In our example, QuickCheck++ was only able to generate
66 valid test cases with the default allowed number of attempts (which is 5
times the number of tests to run).

You can increase the number of allowed attempts to generate valid input
arguments by specifying a second argument when calling
quickcheck::Property::check.

\code
PInsertKeepsSorted2 insertKeepsSorted2;
insertKeepsSorted2.check(100, 2000);
\endcode

gives

\verbatim
OK, passed 100 tests.
\endverbatim

However, if you need to increase this number of attempts to make your tests
successful, you should carefully inspect the input data distribution (as
explained in the next section). It is often the sign that you need a
\ref custom "custom input data generator".

\subsection inspecting Inspecting Input Data Distribution

When dealing with randomly generated test cases, it is often important to be
able to inspect input data distribution. This allows to check that all
\e interesting cases have been covered, and that no bias is introduced due
to an \ref specifying "input predicate" or a bad \ref custom "generator".

The simplest tool provided by QuickCheck++ for input data inspection is the
quickcheck::Property::isTrivialFor predicate. This predicate should return
\c true when the input data makes the property trivially true.

For example, if we return to our \ref defining "PReverseCancelsReverse" example,
we could consider that the property is trivially true for empty vectors and for
vectors of only one element.

\code
class PReverseCancelsReverse2 : public Property<Vector> {
   bool holdsFor(const Vector& xs) {
      Vector ys = xs;
      reverse(ys);
      reverse(ys);
      return xs == ys;
   }
   bool isTrivialFor(const Vector& xs) {
      return xs.size() < 2;
   }
};
\endcode

When we check this property, QuickCheck++ reports the proportion of test
cases that were \e trivial.

\verbatim
OK, passed 100 tests (12% trivial).
\endverbatim

This information reassures us on the fact that both trivial and non-trivial
cases were tested (with a sufficient number of the later).

Another, more powerful tool to inspect input data distribution is the
quickcheck::Property::classify method which assigns a tag to any combination of
input arguments. When a classifier is defined, QuickCheck++ displays the
distribution of test cases amongst the tags after the test result.

For example, let's inspect our insertion test to ensure we test both
insertions at head of the vector, at end of vector, and of course inside
vector.

\code
class PInsertKeepsSorted3 : public Property<Elem, Vector> {
   bool holdsFor(const Elem& x, const Vector& xs) {
      Vector ys = xs;
      insert(x, ys);
      return isSorted(ys);
   }
   bool accepts(const Elem& x, const Vector& xs) {
      return isSorted(xs);
   }
   const string classify(const Elem& x, const Vector& xs) {
      if (xs.empty())
         return "in empty";
      if (x < xs[0])
         return "at head";
      if (x > xs.back())
         return "at end";
      return "inside";
   }
};
\endcode

Our 100 tests (with a maximum of 2000 attempts) now gives:

\verbatim
OK, passed 100 tests.
  34% in empty
  32% at end
  18% at head
  16% inside
\endverbatim

We observe that more than a third of the tests concerned the insertion into an
empty vector and that only 16% of the tests were inserting inside the vector.
This indicates that the combination of the default input data generator and
input predicate induce a bias in our test cases.

We can further inspect generated data by adding the length of the input
vector to the tag. This is equivalent to the \c collect combinator in
original Haskell's QuickCheck, for those who know it. In QuickCheck++, there
is no \c collect and we use the more general \c classify instead (we had no
idea how to implement \c collect and it seems of limited use as the
following example shows).

Redefining the classifier as:

\code
const string classify(const Elem& x, const Vector& xs) {
   string s = toString(xs.size());
   if (xs.empty() || x <= xs[0])
      s += ", at head";
   if (xs.empty() || x >= xs.back())
      s += ", at end";
   if (!xs.empty() && x >= xs[0] && x <= xs.back())
      s += ", inside";
   return s;
}
\endcode

We now obtain:

\verbatim
OK, passed 100 tests.
  39% 0, at head, at end
  17% 1, at end
  14% 1, at head
   7% 2, inside
   6% 2, at end
   4% 3, inside
   3% 3, at end
   3% 2, at head
   2% 3, at head
   1% 5, inside
   1% 4, at head
   1% 4, at end, inside
   1% 3, at end, inside
   1% 1, at head, at end, inside
\endverbatim

We see the problem. As the length of our vector grows, the likelihood that
it is sorted decreases and empty or very small vectors are over-represented
in the test input data. We will solve this using a custom generator as
explained in the next section.

\subsection custom Custom Data Generators

There are two types of data generators:
- General purpose data generators are associated with a type and allows
  the generation of values of this type at random.
- Special purpose data generators are associated with a property and allows
  the generation of randomly generated arguments for this property.

\subsubsection general General Purpose Generators

General purpose generators are necessary for all types that can be received as
arguments by a property. The QuickCheck++ input argument generation process
relies on these generators to be defined for each type involved in a property.

They take the form of a function with signature quickcheck::generate(size_t,
A&) where \c A is the generated type.

\code
void quickcheck::generate(size_t n, A& out);
\endcode

The first argument \c n is a size hint that allows QuickCheck++ to increase the
size of generated values as the tests progress, in order to:
- fail preferably with shorter values which are easier to read and debug ;
- to allow tests to be fast when you are in hurry and only ask a few while
  being more demanding when you have time and ask for a lot of tests.

Properly used, (if you generate compound members of composite data structures
with a lower size hint than the one you received), it also ensures that
QuickCheck++ will not try to build an overly large or even infinite data
structure.

The second argument \c out is a reference to be initialised to a randomly
generated value.

For example, here is the definition of a structure and a corresponding
generator:

\code
struct Point {
   int x;
   int y;
};

void generate(size_t n, Point& out);
void generate(size_t n, Point& out)
{
   generate(n, out.x); // no need to decrease n here, as int is not composite
   generate(n, out.y);
}
\endcode

We could now write and verify properties taking \c Point arguments.

\subsubsection special Special Purpose Generators

We have seen in our insertion example that the default general purpose
generators may lead to inefficient, biased input data distribution (often when
an input predicate is filtering out too many possible test cases).

When that occurs, one can provide its own custom generator by re-implementing
the quickcheck::Property::generateInput method.

For example, if we modify \c PInsertKeepsSorted by adding a generator:

\code
class PInsertKeepsSorted5 : public Property<Elem, Vector> {
   bool holdsFor(const Elem& x, const Vector& xs) {
      Vector ys = xs;
      insert(x, ys);
      return isSorted(ys);
   }
   // no need for the predicate anymore as we generate only valid input
   // bool accepts(const Elem& x, const Vector& xs) {
   //    return isSorted(xs);
   // }
   const string classify(const Elem& x, const Vector& xs) {
      string s = toString(xs.size());
      if (xs.empty() || x <= xs[0])
         s += ", at head";
      if (xs.empty() || x >= xs.back())
         s += ", at end";
      if (!xs.empty() && x >= xs[0] && x <= xs.back())
         s += ", inside";
      return s;
   }
   void generateInput(size_t n, Elem& x, Vector& xs) {
      generate(n, x);
      generate(n, xs);
      sort(xs.begin(), xs.end());
   }
};
\endcode

We now obtain the much better distribution:

\verbatim
OK, passed 100 tests.
  6% 0, at head, at end
  4% 9, inside
  4% 27, inside
  4% 19, inside
  4% 15, inside
  3% 8, inside
  3% 8, at end
  3% 7, inside
  3% 5, inside
  3% 5, at head, inside
  3% 32, inside
  3% 2, at end
  3% 11, inside
  2% 5, at head
  2% 42, inside
  2% 4, inside
  2% 34, inside
  2% 31, inside
  2% 29, inside
  ...
\endverbatim

\subsubsection data Data Generator Primitives

QuickCheck++ comes with generators for all primitive types and provides
functions to help you write your own generators easily (see
quickcheck/generate.hh).

\subsection printing Printing Property Names

If you would like to print the property under examination before its results,
you can use the helper function template quickcheck::check which takes
a message to print as its first argument and a property class as its template
argument. This function will print the message, instantiate the property and
verify it. Apart from the message, it takes the same arguments as
quickcheck::Property::check.

For example, with the definitions of section \ref defining and \ref verifying

\code
check<PReverseCancelsReverse>("that reverse cancels reverse");
check<PReverseCancelsReverse>("that reverse cancels reverse", 200);
check<PReverseCancelsReverse>("that reverse cancels reverse", 200, 600);
check<PReverseIsIdentity>("that reverse is identity");
\endcode

would print

\verbatim
* Checking that reverse cancels reverse...
OK, passed 100 tests.

* Checking that reverse cancels reverse (200)...
OK, passed 200 tests.

* Checking that reverse cancels reverse (200, 600)...
OK, passed 200 tests.

* Checking that reverse is identity...
Falsifiable after 1 tests for input:
  0: [-2, -1, 1]
\endverbatim

\subsection verbose Verbose Checking

QuickCheck++ presents you with an example of faulty input when a properties
fails by returning \c false, but what if your program get stuck in an infinite
loop or crashes with a segfault?

If one of these cases occurs, you need QuickCheck++ to print the input
arguments \e before the tests, because it will not have the occasion to do it
afterwards. You can do precisely this by asking QuickCheck++ to do a \e
verbose check when calling quickcheck::Property::check.

For example:

\code
static int getTheAnswer(int i)
{
   while (i < 0) {
      // eat CPU cycles until interrupted
   }
   return 42;
}

class PBottom : Property<int> {
   bool holdsFor(const int& i) {
      return getTheAnswer(i) == 42;
   }
};

check<PBottom>("this function is stupid", 100, 0, true);
\endcode

would output something like

\verbatim
* Checking that this property is stupid...
Test 0:
  0: 1
Test 1:
  0: -3
^C
\endverbatim

where <tt>^C</tt> stands for the Ctrl-C necessary to interrupt the process.

Verbose checking can also be useful to visually verify what the random
generated input looks like.

\subsection fixed Fixed Test Cases

Although randomly generated test cases have their utility, one should not rely
on them too much to find corner cases (especially for large values, as these
could not be practically generated by the framework). Instead, one should also
provide carefully chosen test cases that put the tested code under stress.

Other reasons to use fixed test cases includes regression testing and common
use cases (to be sure at least these work).

To use QuickCheck++ with fixed test cases as in other classical \e xUnit
testing frameworks, use the quickcheck::Property::addFixed method. Each input
data added through this function will be used as input for your property before
any randomly-generated input each time quickcheck::Property::check is called.

For example, imagine the following iterative squaring function (\eg for an
architecture where multiplication is costly):

\code
static unsigned iterativeSquare(unsigned n)
{
   unsigned res = 0;
   for (unsigned i = 0; i < n; ++i)
      res += n;
   return res;
}
\endcode

One property of a squared number is that it is divisible by its square root:

\code
class PSquareDivisibleByRoot : public Property<unsigned> {
   bool holdsFor(const unsigned& i) {
      return unsigned(sqrt(iterativeSquare(i))) == i;
   }
};
\endcode

If we try this, all seems to fit:

\code
PSquareDivisibleByRoot squareDivisibleByRoot;
squareDivisibleByRoot.check();
\endcode

gives

\verbatim
OK, passed 100 tests.
\endverbatim

However, our function is not safe. It simply pass the tests because all
generated values for the argument are relatively small compared to the range of
unsigned integers. Let's add one test case for the biggest unsigned integer:

\code
squareDivisibleByRoot.addFixed(std::numeric_limits<unsigned>::max());
squareDivisibleByRoot.check();
\endcode

This time, we obtain

\verbatim
Falsifiable after 1 tests for input:
  0: 4294967295
\endverbatim

indicating that our function is partial in its argument (we should document and
assert accepted input values).

\section how2 How to Contribute?

\subsection test Test

We are interested in feedback about utility of QuickCheck++ and its design. If
you have time to give it a try, and you think something is great/stupid/could
be improved, let us know.

You can contact us by mail at the address mentioned in the source or just drop
a comment on <a
href="http://devmusings.legiasoft.com/quickcheck-like-framework-for-cpp">our
blog</a>.

\subsection code Code

QuickCheck++ is currently just a quick hack by a non-expert C++ programmer.
Code can be improved, and it should surely be extended to make the framework
more complete.

If you like QuickCheck++ and you got some spare time (or if you had to
implement things for your own use), we're gladly accepting patches (send them
to the address mentioned in the source).

\subsection promote Promote

If you think QuickCheck++ is great, tell others about it. The more people know
about the framework, the more people may find it useful and the more useful it
becomes!
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
