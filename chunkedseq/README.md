Package description                         {#mainpage}
===================

This package is a C++ template library that implements ordered,
in-memory containers that are based on a B-tree-like data structure.

Like STL deque, our chunkedseq data structure supports fast
constant-time update operations on the two ends of the sequence; and
like balanced tree structures, such as STL rope, our chunkedseq
supports efficient logarithmic-time split (at a specified position)
and merge operations. However, unlike prior data structures, ours
provides all of these operations simultaneously. Our [research
paper](http://deepsea.inria.fr/chunkedseq) presents evidence to back
this claim.

Key features of chunkedseq are:

- Fast constant-time push and pop operations on the two ends of the sequence.
- Logarithmic-time split at a specified position.
- Logarithmic-time concatenation.
- Familiar STL-style container interface.
- A *segment* abstraction to expose to clients of the chunked sequence
the contiguous regions of memory that exist inside chunks.

Supported abstract data types
-----------------------------

- \ref deque
- \ref stack
- \ref bag
- \ref map

Advanced features
-----------------

- \ref parallel_processing
- \ref weighted_container
- \ref stl_iterator
- \ref segments
- \ref cached_measurement

Credits
=======

The [chunkedseq](http://deepsea.inria.fr/chunkedseq) package is maintained
by the members of the [Deepsea Project](http://deepsea.inria.fr/).
Primary authors include:
- [Umut Acar](http://umut-acar.org)
- [Arthur Chargueraud](http://chargueraud.org)
- [Michael Rainey](http://gallium.inria.fr/~rainey).

\defgroup deque Doubly ended queue
@{

The deque structure implements a fast doubly ended queue that supports
logarithmic-time operations for both weighted split and
concatenation.

Runtime interface
=================

The [deque interface](@ref pasl::data::chunkedseq::chunkedseqbase) has
much of the interface of the [STL
deque](http://www.cplusplus.com/reference/deque/deque/).  All
operations for accessing the front and back of the container (e.g.,
`front`, `push_front`, `pop_front`, etc.)  are supported.  In
addition, the deque supports splitting and concatenation in
logarithmic time and provides a random-access iterator.

Template interface
==================

Like the STL deque, the following deque constructor takes
template parameters for the `Item` and `Item_alloc` types.
The constructor takes three additional template parameters:

- The `Chunk_capacity` specifies the maximum number of items that can
  fit in each chunk.
  Although each chunk can store *at most* `Chunk_capacity` items,
  the container can only guarantee that at most half of the cells
  of any given chunk are filled.
- The `Cache` type specifies the strategy to be used internally
  by the deque to maintain monoid-cached measurements of groups
  of items (see \ref cached_measurement).
- The `Chunk_struct` type specifies the fixed-capacity ring-buffer
  representation to be used for storing items (see \ref fixedcapacity).

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace pasl {
namespace data {
namespace chunkedseq {
namespace bootstrapped {

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  template <
    class Chunk_item,
    int Capacity,
    class Chunk_item_alloc=std::allocator<Item>
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptrx,
  class Item_alloc = std::allocator<Item>
>
using deque;

}}}}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example: push and pop
=====================

\snippet examples/chunkedseq_2.cpp deque_example

Output
------

    mydeque contains: 4 9 3 8 2 7 1 6 0 5

Example: split and concat
=========================

\snippet examples/chunkedseq_5.cpp split_example

Output
------

    Just after split:
    contents of mydeque: 0 1 8888
    contents of mydeque2: 9999 4 5
    Just after merge:
    contents of mydeque: 0 1 8888 9999 4 5
    contents of mydeque2:

@}

\defgroup stack Stack
@{

The stack is a container that supports the same set of
operations as the [deque](@ref deque), but has two key differences:

- Thanks to using a simpler stack structure to represent the
  chunks, the stack offers faster access to the back of the
  container and faster indexing operations than deque.
- Unlike deque, the stack cannot guarantee fast updates to
  the front of the container: each update operation performed on
  the front position can require at most `Chunk_capacity`
  items to be shifted toward to back.

The template interface of the stack constructor is the
same as that of the deque constructor, except that the
chunk structure is not needed.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace pasl {
namespace data {
namespace chunkedseq {
namespace bootstrapped {

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  class Item_alloc = std::allocator<Item>
>
using stack;

}}}}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
=======

\snippet examples/chunkedseq_3.cpp stack_example

Output
------

    mystack contains: 4 3 2 1 0

@}

\defgroup bag Bag
@{

Our [bag](@ref pasl::data::chunkedseq::chunkedbagbase) is a generic
container that trades the guarantee of order between its items for
stronger guarantees on space usage and faster push and pop operations
than the corresponding properties of the stack structure.  In
particular, the bag guarantees that there are no empty spaces in
between consecutive items of the sequence, whereas stack and deque can
guarantee only that no more than half of the cells of the chunks are
empty.

Runtime interface
=================

Although our bag is unordered in general, in particular use cases,
order among items is guaranteed.  Order of insertion and removal of
the items is guaranteed by the bag under any sequence of push or pop
operations that affect the back of the container.  The split and
concatenation operations typically reorder items.

The container supports `front`, `push_front` and `pop_front`
operations for the sole purpose of interface compatibility.
These operations simply perform the corresponding actions
on the back of the container.

Template interface
==================

The template interface of the bag is similar to that of stack.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace pasl {
namespace data {
namespace chunkedseq {
namespace bootstrapped {

template <
  class Item,
  int Chunk_capacity = 512,
  class Cache = cachedmeasure::trivial<Item, size_t>,
  class Item_alloc = std::allocator<Item>
>
using bagopt;

}}}}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
=======

\snippet examples/chunkedseq_4.cpp bag_example

Output
------

    mybag contains: 4 3 2 1 0

@}

\defgroup map Associative map
@{

Using the [cached-measurement feature](@ref cached_measurement) of our
chunked sequence structure, we have implemented asymptotically
efficient associative maps in the style of [STL
map](http://www.cplusplus.com/reference/map/map/). Our implementation
is, however, not designed to compete with highly optimized
implementations, such as that of STL. Rather, the main purpose of our
implementation is to provide an example of advanced use of cached
measurement so that others can apply similar techniques to build their
own custom data structures.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace pasl {
namespace data {
namespace map {

template <class Key,
          class Item,
          class Compare = std::less<Key>,
          class Key_swap = std_swap<Key>,
          class Alloc = std::allocator<std::pair<const Key, Item> >,
          int chunk_capacity = 8
>
class map;

}}}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[Our map interface](@ref pasl::data::map::map) implements only a
subset of the [STL
interface](http://www.cplusplus.com/reference/map/map/). The
operations that we do implement have the same time and space
complexity as do the operations implemented by the STL
container. However, the constant factors imposed by our container may
be significantly larger than those of the STL container because our
structure is not specifically optimized for this use case.

Example: insert
===============

\snippet examples/map_1.cpp map_example

Output
------

    mymap['a'] is an element
    mymap['b'] is another element
    mymap['c'] is another element
    mymap['d'] is 
    mymap now contains 4 elements.

Example: erase
==============

\snippet examples/map_2.cpp map_example2

Output
------

    f => 60
    e => 50
    d => 40
    a => 10

@}

\defgroup parallel_processing Parallel processing
@{

The containers of chunkedseq are well suited to applications which
use fork-join parallelism: thanks to the logarithmic-time split
operations, chunkedseq containers can be divided efficiently, and
thanks to the logarithmic-time concatenate operations, chunkedseq
containers can be merged efficiently.  Moreover, chunkedseq containers
can be processed efficiently in a sequential fashion, thereby enabling
a liberal programming style in which sequential and parallel
processing styles are combined synergistically.  The following example
programs deomonstrate this style.

\remark The data structures of the chunkedseq package are *not*
concurrent data structures, or, put differently, chunkedseq data
structures admit only single-threaded update operations.

\remark The following examples are evidence that this single-threading
restriction does *not* necessarily limit parallelism.

Example: `pkeep_if`
==========================

To see how our deque can be used for parallel processing, let us
consider the following program, which constructs the subsequence of a
given sequence, based on selections taken by a client-supplied
predicate function.  Assuming fork-join parallel constructs, such as
Cilk's `spawn` and `sync`, the selection and build process of the
`pkeep_if` function can achieve a large (in fact, unbounded) amount of
parallelism thanks to the fact that the span of the computation is
logarithmic in the size of the input sequence.  Moreover, `pkeep_if`
is *work efficient* thanks to the fact that the algorithm takes linear
time in the size of the input sequence (assuming, of course, that
the client-supplied predicate takes constant time).

\snippet examples/chunkedseq_1.cpp chunkedseq_example1

Output
------

    sum = 1000000000000

Example: `pcopy`
================

This algorithm implements a parallel version of
[std::copy](http://en.cppreference.com/w/cpp/algorithm/copy).  Note,
however, that the two versions differ slightly: in our version, the
type of the destination parameter is a reference to the destination,
whereas the corresponding type in std::copy is instead an iterator
that points to the beginning of the destination container.

\snippet examples/chunkedseq_6.cpp pcopy_example

Output
------

    mydeque2 contains: 0 1 2 3 4 5

Example: `pcopy_if`
===================

This algorithm implements a parallel version of
[std::copy_if](http://en.cppreference.com/w/cpp/algorithm/copy).  Just
as before, our implementation uses a type for the third parameter that
is different from the corresponding third parameter of the STL
version.

\snippet examples/chunkedseq_7.cpp pcopy_if_example

Output
------

    mydeque2 contains: 0 2 4

@}

\defgroup segments Segments
@{

In this package, we use the term segment to refer to pointer values
which reference a range in memory.
We define two particular forms of segments:
- A *basic segment* is a value which consists of two pointers, namely
  `begin` and `end`, that define the right-open interval, `(begin,
  end]`.
- An *enriched segment* is a value which consists of a basic segment,
  along with a pointer, namely `middle`, which points at some location
  in between `begin` and `end`, such that `begin <= middle < end`.

The following class defines a representation for enriched segments.

\snippet include/segment.hpp segment

Example
=======

\snippet examples/segment_1.cpp segment_example

Output
------

    mydeque contains: 0 1 2 3 4 5
    the segment which contains mydeque[3] contains: 2 3
    mydeque[3]=3

@}

\defgroup weighted_container Weighted container
@{

The `chunkedseq` containers can easily generalize to
*weighted containers*.
A weighted container is a container that assigns to each
item in the container an integral weight value.
The weight value is typically expressed as a weight function
that is defined by the client and passed to the container
via template argument.

The purpose of the weight is to enable the client to use the
weighted-split operation, which divides the container into two
pieces by a specified weight.
The split operation takes only logarithmic time.

Example: split sequence of strings by length
============================================

The following example program demonstrates how one can use
weighted split to split a sequence of string values based
on the number of even-length strings.
In this case, our split divides the sequence into two
pieces so that the first piece goes into `d` and the
second to `f`.
The split function specifies that `d` is to receive the first half
of the original sequence of strings that together contain
half of the total number of even-length strings in the original
sequence; `f` is to receive the remaining strings.
Because the lengths of the strings are cached internally by
the weighted container, the split
operation takes logarithmic time in the number of strings.

\snippet examples/weighted_split.cpp weighted_split_example

The program prints the following:

    nb even strings: 6
    d =
    Let's divide this

    f =
    sequence of strings into two pieces

@}

\defgroup stl_iterator STL-style iterator
@{

Our deque, stack and bag containers implement the
[random-access iterators](@ref pasl::data::chunkedseq::iterator::random_access)
in the style of 
[STL's random-access iterators](http://www.cplusplus.com/reference/iterator/RandomAccessIterator/).

Example
=======

\snippet examples/iterator_1.cpp iterator_example

Output
------

    mydeque contains: 0 1 2 3 4

Segment access
==============

As a bonus, the iterator classes support access to segments via the
method 
[get_segment](@ref pasl::data::chunkedseq::iterator::random_access::get_segment). 
For more information on this operation, see the documentation on
[segments](@ref segments).

@}

\defgroup cached_measurement Customized data structures by cached measurement
@{

This documentation covers essential concepts that are needed to
implement custom data structures out of various instantiations of the
chunkedseq structure.
Just like the Finger Tree of Hinze and Patterson, the chunkedseq
can be instantiated in certain ways to yield asymptotically efficient
data structures, such as associative maps, priority queues, weighted
sequences, interval trees, etc.
A summary of these ideas that is presented in greater detail can be
find in the [original publication on finger
trees](http://www.soi.city.ac.uk/~ross/papers/FingerTree.html) and in
a [blog
post](http://apfelmus.nfshost.com/articles/monoid-fingertree.html).

In this tutorial, we present the key mechanism for building
customized data structures: *monoid-cached measurement*.
We show how to use monoid-cached measurements to implement a powerful
form of split operation that affects chunkedseq containers.
Using this split operation, we then show how to apply our
cached measurement scheme to build two new data structures:
- weighted containers with weighted splits
- asymptotically efficient associative map containers in the style of
  [std::map](http://www.cplusplus.com/reference/map/map/)


Taking measurements
===================

Let \f$S\f$ denote the type of the items contained by the chunkedseq
container and \f$T\f$ the type of the cached measurements.
For example, suppose that we want to define a weighted chunkedseq
container of `std::string`s for which the weights have type
`weight_type`. Then we have: \f$S = \mathtt{std::string}\f$ and \f$T =
\mathtt{weight\_type}\f$.
How exactly are cached measurements obtained?
The following two methods are the ones that are used by our C++
package.

Measuring items individually
----------------------------

A *measure function* is a function \f$m\f$ that is provided by the client;
the function takes a single item and returns a 
single measure value:
\f[
\begin{array}{rcl}
  m(s) & : & S \rightarrow T
\end{array}
\f]

### Example: the "size" measure
Suppose we want to use our measurement to represent the number of items
that are stored in the container. We call this measure the *size measure*.
The measure of any individual item always equals one:
\f[
\begin{array}{rclll}
  \mathtt{size}(s) & : & S \rightarrow \mathtt{long} & = & 1
\end{array}
\f]

### Example: the "string-size" measure
The string-size measurement assigns to each item the weight equal to
the number of characters in the given string.
\f[
\begin{array}{rclll}
  \mathtt{string\_size}(str) & : & \mathtt{string} \rightarrow \mathtt{long} & = & str.\mathtt{size}()
\end{array}
\f]

Measuring items in contiguous regions of memory
-----------------------------------------------

Sometimes it is convenient to have the ability to compute, all at once,
the combined measure of a group of items that is referenced by a given
"basic" [segment](@ref segments).
For this reason, we require that, in addition to \f$m\f$, each measurement
scheme provides a segment-wise measure operation, namely
\f$\vec{m}\f$, which takes the pair of pointer arguments \f$begin\f$
and \f$end\f$ which correspond to a basic segment, and returns
a single measured value.
\f[ \begin{array}{rcl}
  \vec{m}(begin, end) & : & (S^\mathtt{*}, S^\mathtt{*}) \rightarrow T 
\end{array} 
\f]
The first and second arguments correspond to the range in memory
defined by the segment \f$(begin, end]\f$.
The value returned by \f$\vec{m}(begin, end)\f$ should equal the sum of the
values \f$m(\mathtt{*}p)\f$ for each pointer \f$p\f$ in the range
\f$(begin, end]\f$.

### Example: segmented version of our size measurement

This operation is simply \f$\vec{m}(begin, end) = |end-begin|\f$, 
where our segment is defined by the sequence of items represented
by the range of pointers \f$(begin, end]\f$.

The measure descriptor
----------------------

The *measure descriptor* is the name that we give to the C++ class
that describes a given measurement scheme.
This interface exports deinitions of the following
types:

Type                   | Description
-----------------------|----------------------------------------------------
`value_type`           | type \f$S\f$ of items stored in the container
`measured_type`        | type \f$T\f$ of item-measure values

And this interface exports definitions of the following methods:

Members                                                                    | Description
---------------------------------------------------------------------------|------------------------------------------------
`measured_type operator()(const value_type& v)`                            | returns \f$m(\mathtt{v})\f$
`measured_type operator()(const value_type* begin, const value_type* end)` | returns \f$\vec{m}(\mathtt{begin}, \mathtt{end})\f$

### Example: trivial measurement

Our first kind of measurement is one that does nothing except make
fresh values whose type is the same as the type of the second template
argument of the class.

\snippet include/measure.hpp trivial

The trivial measurement is useful in situations where cached
measurements are not needed by the client of the chunkedseq. Trivial
measurements have the advantage of being (almost) zero overhead
annotations.

### Example: weight-one (uniformly sized) items

This kind of measurement is useful for maintaining fast access to the
count of the number of items stored in the container.

\snippet include/measure.hpp uniform

### Example: dynamically weighted items

This technique allows the client to supply to the internals of the
chunkedseq container an arbitrary weight function. This
client-supplied weight function is passed to the following class by
the third template argument.

\snippet include/measure.hpp weight

### Example: combining cached measurements

Often it is useful to combine meaurements in various
configurations. For this purpose, we define the measured pair, which
is just a structure that has space for two values of two given
measured types, namely `Measured1` and `Measured2`.

\snippet include/measure.hpp measured_pair

The combiner measurement just combines the measurement strategies of
two given measures by pairing measured values.

\snippet include/measure.hpp combiner

Using algebras to combine measurements
======================================

Recall that a *monoid* is an algebraic structure that consists
of a set \f$T\f$, an associative binary operation \f$\oplus\f$ and an identity
element \f$\mathbf{I}\f$. That is, \f$(T, \oplus, \mathbf{I})\f$ is a monoid
if:

- \f$\oplus\f$ is associative: for every \f$x\f$, \f$y\f$ and \f$z\f$ in \f$T\f$, 
  \f$x \oplus (y \oplus z) = (x \oplus y) \oplus z\f$.
- \f$\mathbf{I}\f$ is the identity for \f$\oplus\f$: for every \f$x\f$ in \f$T\f$,
  \f$x \oplus \mathbf{I} = \mathbf{I} \oplus x\f$.

Examples of monoids include the following:

- \f$T\f$ = the set of all integers; \f$\oplus\f$ = addition; \f$\mathbf{I}\f$
  = 0
- \f$T\f$ = the set of 32-bit unsigned integers; \f$\oplus\f$ = addition
  modulo \f$2^{32}\f$; \f$\mathbf{I}\f$ = 0
- \f$T\f$ = the set of all strings; \f$\oplus\f$ = concatenation;
  \f$\mathbf{I}\f$ = the empty string

A *group* is a closely related algebraic structure. Any monoid
is also a group if the monoid has an inverse operation \f$\ominus\f$:

- \f$\ominus\f$ is inverse for \f$\oplus\f$: for every \f$x\f$ in \f$T\f$,
  there is an item \f$y = \ominus x\f$ in \f$T\f$, such that \f$x \oplus y
  = \mathbf{I}\f$.

The algebra descriptor
----------------------

We require that the descriptor export a binding to the type of the
measured values that are related by the algebra.

Type                   | Description
-----------------------|-------------------------------------------------------------
`value_type`           | type of measured values \f$T\f$ to be related by the algebra

We require that the descriptor export the following members. If
`has_inverse` is false, then it should be safe to assume that the
`inverse(x)` operation is never called.

Static members                                 | Description
-----------------------------------------------|-----------------------------------
const bool has_inverse                         | `true`, iff the algebra is a group
value_type identity()                          | returns \f$\mathbf{I}\f$
value_type combine(value_type x, value_type y) | returns `x` \f$\oplus\f$ `y`
value_type inverse(value_type x)               | returns \f$\ominus\f$ `x`

### Example: trivial algebra

The trivial algebra does nothing except construct new identity
elements.

\snippet include/algebra.hpp trivial

### Example: algebra for integers

The algebra that we use for integers is a group in which the identity
element is zero, the plus operator is integer addition, and the minus
operator is integer negation.

\snippet include/algebra.hpp int_group_under_addition_and_negation

### Example: combining algebras

Just like with the measurement descriptor, an algebra descriptor can
be created by combining two given algebra descriptors pairwise.

\snippet include/algebra.hpp combiner

Scans
-----

A *scan* is an iterated reduction that maps to each item \f$v_i\f$ in a
given sequences of items \f$S = [v_1, v_2, \ldots, v_n]\f$ a single
measured value \f$c_i = \mathbf{I} \oplus m(v_1) \oplus m(v_2) \oplus
\ldots \oplus m(v_i)\f$, where \f$m(v)\f$ is a given measure function.
For example, consider the "size" (i.e., weight-one) scan, which is
specified by the use of a particular measure function:
\f$m(v) = 1\f$.
Observe that the size scan gives the positions of the items in the
sequence, thereby enabling us later on to index and to split the
chunkedseq at a given position.

For convenience, we define scan formally as follows. The operator
returns the combined measured values of the items in the range
of positions \f$[i, j)\f$ in the given sequence \f$s\f$.
\f[
\begin{array}{rclr}
  M_{i,j}    & : & \mathtt{Sequence}(S) \rightarrow T \\
  M_{i,i}(s) & = & \mathbf{I} & \\
  M_{i,j}(s) & = & \oplus_{k = i}^j m(s_k) & \mathrm{if} \, i < j 
\end{array}
\f]

Why associativity is necessary
------------------------------

The cached value of an internal tree node \f$k\f$ in the chunkedseq
structure is computed by \f$M_{i,j}(s)\f$, where \f$s = [v_i, \ldots,
v_j]\f$ represents a subsequence of values contained in the chunks of
the subtree below node \f$k\f$. When this reduction is performed by
the internal operations of the chunkedseq, this expression is broken
up into a set of subexpressions, for example: \f$((m(v_i) \oplus
m(v_{i+1})) \oplus (m(v_{i+2}) \oplus m(v_{i+3}) \oplus (m(v_{i+4})
\oplus m(v_{i+5}))) ... \oplus m(v_j))\f$. The partitioning into
subexpressions and the order in which the subexpressions are combined
depends on the particular shape of the underlying
chunkedseq. Moreover, the particular shape is determined uniquely by
the history of update operations that created the finger tree. As
such, we could build two chunkedseqs by, for example, using different
sequences of push and pop operations and end up with two different
chunkedseq structures that represent the same sequence of items. Even
though the two chunkedseqs represent the same sequence, the cached
measurements of the two chunkedseqs are combined up to the root of the
chunkedseq by two different partitionings of combining
operations. However, if \f$\oplus\f$ is associative, it does not
matter: regardless of how the expression are broken up, the cached
measurement at the root of the chunkedseq is guaranteed to be the same
for any two chunkedseqs that represent the same sequence of items.
Commutativity is not necessary, however, because the ordering of the
items of the sequence is respected by the combining operations
performed by the chunkedseq.

Why the inverse operation can improve performance
-------------------------------------------------

Suppose we have a cached measurement \f$C = M_{i,j}(s)\f$ , where \f$s
= [v_i, \ldots, v_j]\f$ represents a subsequence of values contained
in the same chunk somewhere inside our chunkedseq structure. Now,
suppose that we wish to remove the first item from our sequence of
measurements, namely \f$v_i\f$. On the one hand, without an inverse
operation, and assuming that we have not cached partial sums of
\f$C\f$, the only way to compute the new cached value is to recompute
\f$(m(v_{i+1}) \oplus ... \oplus m(v_j))\f$. On the other hand, if the
inverse operation is cheap, it may be much more efficient to instead
compute \f$\ominus m(v_i) \oplus C\f$.

Therefore, it should be clear that using the inverse operation can
greatly improve efficiency in situations where the combined cached
measurement of a group of items needs to be recomputed on a regular
basis. For example, the same situation is triggered by the pop
operations of the chunks stored inside the chunkedseq structure. On
the one hand, by using inverse, each pop operation requires only a few
additional operations to reset the cached measured value of the
chunk. On the other, if inverse is not available, each pop operation
requires recomputing the combined measure of the chunk, which although
constant time, takes time proportion with the chunk size, which can be
a fairly large fixed constant, such as 512 items.  As such,
internally, the chunkedseq operations use inverse operations whenever
permitted by the algebra (i.e., when the algebra is identified as a
group) but otherwise fall back to the most general strategy when the
algebra is just a monoid.

Defining custom cached-measurement policies
===========================================

The cached-measurement policy binds both the measurement scheme and
the algebra for a given instantiation of chunkedseq.  For example, the
following are cached-measurement policies:

- nullary cached measurement: \f$m(s) = \emptyset\f$; \f$\vec{m}(v) =
  \emptyset\f$; \f$A_T = (\mathcal{P}(\emptyset), \cup, \emptyset,
  \ominus )\f$, where \f$\ominus \emptyset = \emptyset\f$
- size cached measurement: \f$m(s) = 1\f$; \f$\vec{m}(v) = |v|\f$; \f$A_T =
  (\mathtt{long}, +, 0, \ominus )\f$
- pairing policies (monoid): for any two cached-measurement
  policies \f$m_1\f$; \f$\vec{m_1}\f$; \f$A_{T_1} = (T_1, \oplus_1,
  \mathtt{I}_1)\f$ and \f$m_2\f$; \f$\vec{m_2}\f$; \f$A_{T_2} = (T_2, \oplus_2,
  \mathtt{I}_2)\f$, \f$m(s_1, s_2) = (m_1(s_1), m_2(s_2))\f$; \f$\vec{m}(v_1,
  v_2) = (\vec{m_1}(v_1), \vec{m_2}(v_2))\f$; \f$A = (T_1 \times T_2,
  \oplus, (\mathtt{I}_1, \mathtt{I}_2))\f$ is also a cached-measurement
  policy, where \f$(x_1, x_2) \oplus (y_1, y_2) = (x_1 \oplus y_1, x_2
  \oplus y_2)\f$
- pairing policies (group): for any two cached-measurement
  policies \f$m_1\f$; \f$\vec{m_1}\f$; \f$A_{T_1} = (T_1, \oplus_1,
  \mathtt{I}_1, \ominus_1)\f$ and \f$m_2\f$; \f$\vec{m_2}\f$; \f$A_{T_2} =
  (T_2, \oplus_2, \mathtt{I}_2, \ominus_2)\f$, \f$m(s_1, s_2) =
  (m_1(s_1), m_2(s_2))\f$; \f$\vec{m}(v_1, v_2) = (\vec{m_1}(v_1),
  \vec{m_2}(v_2))\f$; \f$A = (T_1 \times T_2, \oplus, (\mathtt{I}_1,
  \mathtt{I}_2), \ominus)\f$ is also a cached-measurement policy,
  where \f$(x_1, x_2) \oplus (y_1, y_2) = (x_1 \oplus y_1, x_2 \oplus
  y_2)\f$ and \f$\ominus(x_1, x_2) = (\ominus_1 x_1,
  \ominus_2 x_2)\f$
- pairing policies (mixed): if only one of two given
  cached-measurement policies is a group, we demote the group to a
  monoid and apply the pairing policy for two monoids

\remark To save space, the chunkedseq structure can be instantiated
with the nullary cached measurement alone. No space is taken by the
cached measurements in this configuration because the nullary
measurement takes zero bytes. However, the only operations supported
in this configuration are push, pop, and concatenate. The size cached
measurement is required by the indexing and split operations. The
various instantiations of chunkedseq, namely deque, stack and bag all
use the size measure for exactly this reason.

The cached-measurement descriptor
---------------------------------

The interface exports four key components: the type of the items in
the container, the type of the measured values, the measure function
to gather the measurements, and the algebra to combine measured
values.

Type                   | Description
-----------------------|--------------------------------------------
`measure_type`         | type of the measure descriptor
`algebra_type`         | type of the algebra descriptor
`value_type`           | type \f$S\f$ of items to be stored in the container
`measured_type`        | type \f$T\f$ of measured values
`size_type`            | `size_t`

The only additional function that is required by the policy is a swap
operation.

Static members                                   | Description
-------------------------------------------------|------------------------------------
`void swap(measured_type& x, measured_type& y)`  | exchanges the values of `x` and `y`

Example: trivial cached measurement
-----------------------------------

This trivial cached measurement is, by itself, completely inert: no
computation is required to maintain cached values and only a minimum
of space is required to store cached measurements on internal tree
nodes of the chunkedseq.

\snippet include/cachedmeasure.hpp trivial

Example: weight-one (uniformly sized) items
-------------------------------------------

In our implementation, we use this cached measurement policy to
maintain the size information of the container. The `size()` methods
of the different chunkedseq containers obtain the size information by
referencing values cached inside the tree by this policy.

\snippet include/cachedmeasure.hpp size

Example: weighted items
-----------------------

Arbitrary weights can be maintained using a slight generalization of
the `size` measurement above.

\snippet include/cachedmeasure.hpp weight

Example: combining cached measurements
--------------------------------------

Using the same combiner pattern we alredy presented for measures and
algebras, we can use the following template class to build
combinations of any two given cached-measurement policies.

\snippet include/cachedmeasure.hpp combiner

Splitting by predicate functions
================================

Logically, the split operation on a chunkedseq container divides the
underlying sequence into two pieces, leaving the first piece in the
container targeted by the split and moving the other piece to another
given container.
The position at which the split occurs is determined by a search
process that is guided by a *predicate function*.
What carries out the search process?
That job is the job of the internals of the chunkedseq class; the
client is responsible only to provide the predicate function that is
used by the search process.
Formally, a predicate function is simply a function \f$p\f$ which 
takes a measured value and returns either `true` or `false`.
\f[
\begin{array}{rcl}
  p(m) & : & T \rightarrow \mathtt{bool}
\end{array}
\f]
The search process guarantees that the position at which the split
occurs is the position \f$i\f$ in the target sequence, \f$s = [v_1,
\ldots, v_i, \ldots v_n]\f$, at which the value returned by
\f$p(M_{0,i}(s))\f$ first switches from false to true.
The first part of the split equals \f$[v_1, \ldots, v_{i-1}]\f$ and
the second \f$[v_i, \ldots, v_n]\f$.

The predicate function descriptor
---------------------------------

In our C++ package, we represent predicate functions as classes which
export the following public method.

Members                             | Description
------------------------------------|------------------------------------
`bool operator()(measured_type m)`  | returns \f$p(\mathtt{m})\f$

Example: weighted splits
------------------------

Let us first consider the small example which is given already for the
[weighted container](@ref weighted_container).
The action performed by the example program is to divide a given
sequence of strings so that the first piece of the split contains
approximately half of the even-length strings and the second piece the
second half.
In our example code (see the page linked above), we assign to each
item a certain weight, which is defined formally as follows: if the
length of the given string is an even number, return a 1; else, return
a 0.
\f[
\begin{array}{rcccl}
 m(str) & : & \mathtt{string} \rightarrow \mathtt{int} & = & \left\{ 
  \begin{array}{l l}
    1 & \quad \mathrm{if}\, str.\mathtt{size()}\, \mathrm{is\, an\, even\, number}\\
    0 & \quad \mathrm{otherwise}
  \end{array} \right.
\end{array}
\f]
Let \f$n\f$ denote the number of even-length strings in our source
sequence.
Then, the following predicate function delivers the exact split that
we want.
\f[
\begin{array}{rcccl}
  p(m) & : & int \rightarrow \mathtt{bool} & = & m \geq n/2
\end{array}
\f]
Let \f$s\f$ denote the sequence of strings (i.e., `["Let's", "divide",
"this", "string", "into", "two", "pieces"]` that we want to split.
The following table shows the logical states of the split process.

         \f$i\f$           | 0       | 1        | 2       | 3        | 4      | 5      | 6        |
---------------------------|---------|----------|---------|----------|--------|--------|----------|
\f$v_i\f$                  | `Let's` | `divide` | `this`  | `string` | `into` | `two`  | `pieces` |
\f$m(v_i)\f$               | 0       | 1        | 1       | 1        | 1      | 0      | 1        |
\f$M_{0,i}(s)\f$              | 0       | 1        | 2       | 3        | 4      | 4      | 5        |
\f$p(M_{0,i}(s))\f$         | `false` | `false`  | `false` | `true`   | `true` | `true` | `true`   |

\remark Even though the search process might look like a linear
search, the process in fact takes just logarithmic time in the number
of items in the sequence.  The logarithmic time bound is possible
thanks to the fact that internal nodes of the chunkedseq tree (which
is itself a tree whose height is logarithmic in the number of items)
are annotated by partial sums of weights.

Example: using cached measurement to implement associative maps
===============================================================

Our final example combines all of the elements of cached measurement
to yield an asymptotically efficient implementation of associative
maps.
The idea behind the implementation is to represent the map internally
by a chunkedseq container of key-value pairs.
The key to efficiency is that the items in the chunkedseq are stored
in descending order.
When key-value pairs are logically added to the map, the key-value
pair is physically added to a position in the underlying sequence so
that the descending order is maintained.
The insertion and removal of key-value pairs is achieved by splitting
by a certain predicate function which we will see later.
At a high level, what is happening is a kind of binary search that
navigates the underlying chunkedseq structure, guided by
carefully chosen cached key values that annotate the interior nodes of
the chunkedseq.

\remark We could have just as well maintain keys in ascending order.

Optional values
---------------

Our implementation uses *optional values*, which are values that
logically either contain a value of a given type or contain nothing at
all.
The concept is similar to that of the null pointer, except that the
optional value applies to any given type, not just pointers.

\snippet examples/map.hpp option

Observe that our class implements the "less-than" operator: `<`.
Our implementation of this operator lifts any implementation of the
same operator at the type `Item` to the space of our `option<Item>`:
that is, our operator treats the empty (i.e., nullary) optional value
as the smallest optional value.
Otherwise, our the comparison used by our operator is the implementation
already defined for the given type, `Item`, if such an implementation
is available.

The measure descriptor
----------------------

The type of value returned by the measure function (i.e.,
`measured_type`) is the optional key value, that is, a value of type
`option<key_type>`.
The measure function simply extracts the smallest key value from the
key-value pairs that it has at hand and packages them as an
optional key value.

\snippet examples/map.hpp get_key_of_last_item

The monoid descriptor
---------------------

The monoid uses for its identity element the nullary optional key.
The combining operator takes two optional key values and of the two
returns either the smallest one or the nullary optional key value.

\snippet examples/map.hpp take_right_if_nonempty

The descriptor of the cached measurement policy
-----------------------------------------------

The cache measurement policy combines the measurement and monoid
descriptors in a straightforward fashion.

\snippet examples/map.hpp map_cache

The associative map
-------------------

The associative map class maintains the underlying sorted sequence of
key-value pairs in the field called `seq`.
The method called `upper` is the method that is employed by the class
to maintain the invariant on the descending order of the keys.
This method returns either the position of the first key that is
greater than the given key, or the position of one past the end of the
sequence if the given key is the greatest key.

As is typical of STL style, the indexing operator is used by the
structure to handle both insertions and lookups.
The operator works by first searching in its underlying sequence for
the key referenced by its parameter; if found, the operator updates
the value component of the corresponding key-value pair.
Otherwise, the operator creates a new position in the sequence to put
the given key by calling the `insert` method of `seq` at the
appropriate position.

\snippet examples/map.hpp swap
\snippet examples/map.hpp map

@}

\defgroup fixedcapacity Fixed-capacity vectors
@{

The fixed-capacity buffer {#fixedcapacitybuffer}
=========================

Type                   | Description
-----------------------|----------
`allocator_type`       | STL-style allocator class
`size_type`            | `size_t`
`value_type`           | type of items to be stored in the container
`reference`            | `allocator_type::reference`
`const_reference`      | `allocator_type::const_reference`
`pointer`              | `allocator_type::pointer`
`const_pointer`        | `allocator_type::const_pointer`

Member                                                                                    | Description
------------------------------------------------------------------------------------------|-------------
`capacity`                                                                                | integer value which denotes the maximum number items that can be contained in the chunk
`full() const`                                                                            | returns true iff the container is full (i.e., reached capacity)
`empty() const`                                                                           | returns true iff the container is empty
`size() const`                                                                            | returns the number of items in the container
`back() const`                                                                            | returns a reference to the last item in the container
`front() const`                                                                           | returns a reference to the first item in the container
`push_back(const value_type& x)`                                                          | adds value `x` to the last position of the container
`push_front(const value_type& x)`                                                         | adds value `x` to the first position of the container
`pop_back()`                                                                              | removes the value from the last position of the container
`pop_front()`                                                                             | removes the value from the first position of the container
`backn(value_type* xs, size_type nb)`                                                     | copies the last `nb` items from the container to the memory pointed to by `xs`
`frontn(value_type* xs, size_type nb)`                                                    | copies the first `nb` items from the container to the memory pointed to by `xs`
`pushn_back(const value_type* xs, size_type nb)`                                          | pushes on the back of the container the `nb` items from the memory pointed to by `xs`
`pushn_front(const value_type* xs, size_type nb)`                                         | pushes on the front of the container the `nb` items from the memory pointed to by `xs`
`pushn_back(const Body& body, size_type nb)`                                              | pushes on the back of the container `nb` uninitialized items (i.e., default-constructed items), then initializes each item `i` in the underlying vector `vec` by applying `body(i, vec[i])` (behavior is undefined if `size() + nb > capacity`)
`popn_front(size_type nb)`                                                                | removes the first `nb` items from the container
`popn_back(size_type nb)`                                                                 | removes the last `nb` items from the container
`popn_front(value_type* xs, size_type nb)`                                                | removes the first `nb` items from the container and leaves the contents in the memory pointed to by `xs`
`popn_back(value_type* xs, size_type nb)`                                                 | removes the last `nb` items from the container and leaves the contents in the memory pointed to by `xs`
`swap(Other_chunk& other)`                                                                | swaps the contents of the chunk with the contents of another chunk of type `Other_chunk`
`transfer_back_to_front(chunk& target, size_type nb)`                                     | moves the last `nb` items from this chunk to the front of the target chunk
`transfer_front_to_back(chunk& target, size_type nb)`                                     | moves the first `nb` items from this chunk to the back of the target chunk
`get_vec()`                                                                               | returns a reference to the underlying vector container
`clear()`                                                                                 | erases and deallocates the contents of the container
`operator[](size_type ix) const`                                                          | returns a reference to the item at position `ix` in the container
`segment_by_index(size_type ix) const`                                                    | returns the segment relative to the given index `ix`. in particular, the middle of the result segment points to the cell at index `ix` in the container and beginning and end point to the first and one past the last cell respectively that are both in the same contiguous chunk as the middle cell
`index_of_pointer(const value_type* p) const`                                             | returns the index in the sequence corresponding to pointer `p`

For-each loops {#foreach_loop_body}
==============

The for-each loop operation offers sequential access to the items of a
vector. To use the for-each loop, the client must apply the `for_each`
method of the vector, providing the body of the loop as the argument.
If it accesses items read only, the body of the loop must provide the
following method:

Member function                                     | Description
----------------------------------------------------|---------------
`operator()(reference v)` | represents the action of the loop body to perform on the container item referenced by `v`

Example: using for-each loop to compute a sum of the items of a container `c`
-------------------------------------

    int sum = 0;
    c.for_each([&sum] (reference v) {
        sum += v;
    });

Implementations of fixed-capacity vectors provided by our library {#basic-fixed-capacity}
=================================================================

This library provides two implementations of fixed-capacity vectors:
the \ref pasl::data::fixedcapacity::stack and the \ref
pasl::data::fixedcapacity::ringbuffer. Both export the same interface,
namely the interface of the fixed-capacity vector, but each has
different performance characteristics. The `ringbuffer` offers
slightly slower but still fast constant-time access to both ends of
the container. The stack is biased to offer fast access to the end
of the container. The disadvantage is that the stack imposes a
linear-time cost to push an item on the front of the container. That
is, the contents of the container must be shifted right by one
position each time an individual item is pushed on the front of the
container. However, the contents of the stack need to be shifted
right by _n_ positions in order to push _n_ items in bulk at once.

Example: fixed-capacity stack
-----------------------------

\snippet include/fixedcapacitybase.hpp fixedcapacitystack

@}
