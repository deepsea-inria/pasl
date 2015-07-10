% The Parallel-Container Template Library User's Guide
% Deepsea Project
% 10 February 2015

Introduction
============

The parallel-container template library (pctl) is a C++ library that
is currently under development by the [Deepsea
Project](http://deepsea.inria.fr). The goal of the pctl is to provide
a rich set of parallel data structures, along with efficient
data-parallel algorithms, such as merging, sorting, reduction, etc. We
are designing the pctl implementation to target shared-memory,
parallel (i.e., multicore) platforms.

Our design borrows to a large extent the conventions used by the
[Standard Template
Library](http://www.cplusplus.com/reference/stl/). However, when these
conventions come into conflict with parallelism, we break with the STL
convention.

The pctl provides a rigorous framework for reasoning about the cost of
pctl operations. The cost model employed by pctl is the *work*/*span*
model, which is provided by systems, such as [Cilk
Plus](https://software.intel.com/en-us/intel-cilk-plus) and
[pasl](http://deepsea.inria.fr). Moreover, pctl implements a
[granularity-control technique](http://deepsea.inria.fr/oracular/)
that is helpful for taming overheads that relate to parallel
execution.

Preliminaries
=============

Platform
--------

We have implemented pctl to be compatible with the C++11
standard. Earlier version of the C++ standard may be incompatible with
pctl.

Cost model
----------

Define *work* and *span* here.

Granularity control
-------------------

Define *cost function* here.

Introduce `fork2` and `cstmt` here.

Containers
==========

Our definition of a conainer is the same as that of STL: a *container*
is a holder object that stores a collection of other objects (its
elements). Elements are implemented as class templates, which allows
flexibility to specify at compile time the type of the elements to be
stored in any given container.

The container manages the storage space for its elements and provides
member functions to access them, either directly or through iterators
(reference objects with similar properties to pointers).

The particular containers defined by pctl fill what we believe are
important gaps in the current state-of-the-art C++ container
libraries. In specific, the gaps relate to data structures that can
always be resized and accessed efficiently in parallel. Note that,
although we designed the pctl containers to useful for *parallel
programming*, we have not designed them to be useful for *concurrent
programming*.  A discussion of the difference between parallel and
concurrent programming is out of the scope of this manual, but can be
found
[here](https://wiki.haskell.org/Parallelism_vs._Concurrency). What
this property means for clients of pctl is that the container
operations that modify the container (e.g., `push` or `pop`) generally
are not thread safe. Parallelism is achieved instead by operating in
parallel on disjoint range of, say, the same sequence, or by splitting
and merging sequences in a fork-join fashion.

Sequence containers
-------------------

Class name                           | Description
-------------------------------------|---------------------------------
[`parray`](#parray)                  | Array class
[`pchunkedseq`](#pchunkedseq)        | Chunked-sequence class

Table: Sequence containers that are provided by pctl.

Associative containers
----------------------

Class name          | Description
--------------------|-------------------------
[`pset`](#pset)     | Set class
[`pmap`](#pmap)     | Associative-map class

Table: Associative containers that are provided by pctl.

String containers
-----------------

Class name                           | Description
-------------------------------------|-------------------------------------
[`pstring`](#pstring)                | Array-based string class

Table: String containers that are provided by pctl.

Parallel array {#parray}
==============

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Item, class Alloc = std::allocator<Item>>
class parray;

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Parallel arrays are containers representing arrays that are populated
with their elements in parallel. Although they can be resized, the
work cost of resizing a parallel array is linear. In other words,
parallel arrays are good for bulk-parallel processing, but not so good
for incremental updates.

Just like traditional C++ arrays (i.e., raw memory allocated by, say,
`malloc`), parallel arrays use contiguous storage to hold their
elements. As such, elements can be accessed as efficiently as C arrays
by using offsets or by using pointer values. Unlike C arrays, storage
is managed automatically by the container. Moreover, unlike C arrays,
parallel arrays initialize and de-initialize their cells in parallel.

## Template parameters

+-----------------------------------+-----------------------------------+
| Template parameter                | Description                       |
+===================================+===================================+
| [`Item`](#pa-item)                | Type of the objects to be stored  |
|                                   |in the container                   |
+-----------------------------------+-----------------------------------+
| [`Alloc`](#pa-alloc)              | Allocator to be used by the       |
|                                   |container to construct and destruct|
|                                   |objects of type `Item`             |
+-----------------------------------+-----------------------------------+

Table: Template parameters for the `parray` class.

### Item type {#pa-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the elements.  Only if `Item` is guaranteed to not throw while
moving, implementations can optimize to move elements instead of
copying them during reallocations.  Aliased as member type
`parray::value_type`.

### Allocator {#pa-alloc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Alloc;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the allocator object used to define the storage allocation
model. By default, the allocator class template is used, which defines
the simplest memory allocation model and is value-independent.
Aliased as member type `parray::allocator_type`.

## Member types

+-----------------------------------+-----------------------------------+
| Type                              | Description                       |
+===================================+===================================+
| `value_type`                      | Alias for template parameter      |
|                                   |`Item`                             |
+-----------------------------------+-----------------------------------+
| `reference`                       | Alias for `value_type&`           |
+-----------------------------------+-----------------------------------+
| `const_reference`                 | Alias for `const value_type&`     |
+-----------------------------------+-----------------------------------+
| `pointer`                         | Alias for `value_type*`           |
+-----------------------------------+-----------------------------------+
| `const_pointer`                   | Alias for `const value_type*`     |
+-----------------------------------+-----------------------------------+
| [`iterator`](#pa-iter)            | Iterator                          |
+-----------------------------------+-----------------------------------+
| [`const_iterator`](#pa-iter)      | Const iterator                    |
+-----------------------------------+-----------------------------------+

Table: Parallel-array member types.

### Iterator {#pa-iter}

The type `iterator` and `const_iterator` are instances of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator)
concept.

## Constructors and destructors

+-----------------------------------+-----------------------------------+
| Constructor                       | Description                       |
+===================================+===================================+
| [empty container                  | constructs an empty container with|
|constructor](#pa-e-c-c) (default   |no items                           |
|constructor)                       |                                   |
+-----------------------------------+-----------------------------------+
| [fill constructor](#pa-e-f-c)     | constructs a container with a     |
|                                   |specified number of copies of a    |
|                                   |given item                         |
+-----------------------------------+-----------------------------------+
| [populate constructor](#pa-e-p-c) | constructs a container with a     |
|                                   |specified number of values that are|
|                                   |computed by a specified function   |
+-----------------------------------+-----------------------------------+
| [copy constructor](#pa-e-cp-c)    | constructs a container with a copy|
|                                   |of each of the items in the given  |
|                                   |container, in the same order       |
+-----------------------------------+-----------------------------------+
| [initializer list](#pa-i-l-c)     | constructs a container with the   |
|                                   |items specified in a given         |
|                                   |initializer list                   |
+-----------------------------------+-----------------------------------+
| [move constructor](#pa-m-c)       | constructs a container that       |
|                                   |acquires the items of a given      |
|                                   |parallel array                     |
+-----------------------------------+-----------------------------------+
| [destructor](#pa-destr)           | destructs a container             |
+-----------------------------------+-----------------------------------+

Table: Parallel-array constructors and destructors.

### Empty container constructor {#pa-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

### Fill container {#pa-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(long n, const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Populate constructor {#pa-e-p-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
// (1) Constant-time body
parray(long n, std::function<Item(long)> body);
// (2) Non-constant-time body
parray(long n,
       std::function<long(long)> body_comp,
       std::function<Item(long)> body);
// (3) Non-constant-time body along with range-based complexity function
parray(long n,
       std::function<long(long,long)> body_comp_rng,
       std::function<Item(long)> body);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` cells, populating those cells with
values returned by the `n` calls, `body(0)`, `body(1)`, ...,
`body(n-1)`, in that order.

In the second version, the value returned by `body_comp(i)` is used by
the constructor as the complexity estimate for the call `body(i)`.

In the third version, the value returned by `body_comp(lo, hi)` is
used by the constructor as the complexity estimate for the calls
`body(lo)`, `body(lo+1)`, ... `body(hi-1)`.

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

### Copy constructor {#pa-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(const parray& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Initializer-list constructor {#pa-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

### Move constructor {#pa-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(parray&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

### Destructor {#pa-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~parray();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

## Member functions

+------------------------+--------------------------------------+
| Operation              | Description                          |
+========================+======================================+
| [`operator[]`](#pa-i-o)| Access member item                   |
|                        |                                      |
+------------------------+--------------------------------------+
| [`size`](#pa-si)       | Return size                          |
+------------------------+--------------------------------------+
| [`clear`](#pa-cl)      | Empty container contents             |
+------------------------+--------------------------------------+
| [`resize`](#pa-rsz)    | Change size                          |
+------------------------+--------------------------------------+
| [`swap`](#pa-sw)       | Exchange contents                    |
+------------------------+--------------------------------------+
| [`begin`](#pa-beg)     | Returns an iterator to the beginning |
| [`cbegin`](#pa-beg)    |                                      |
+------------------------+--------------------------------------+
| [`end`](#pa-end)       | Returns an iterator to the end       |
| [`cend`](#pa-end)      |                                      |
+------------------------+--------------------------------------+

Table: Parallel-array member functions.

### Indexing operator {#pa-i-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
reference operator[](long i);
const_reference operator[](long i) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a reference at the specified location `i`. No bounds check is
performed.

***Complexity.*** Constant time.

### Size operator {#pa-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

### Clear {#pa-cl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void clear();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Erases the contents of the container, which becomes an empty
container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Resize {#pa-rsz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void resize(long n, const value_type& val); // (1)
void resize(long n) {                       // (2)
  value_type val;
  resize(n, val);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container to contain `n` items.

If the current size is greater than `n`, the container is reduced to
its first `n` elements.

If the current size is less than `n`,

1. additional copies of `val` are appended
2. additional default-inserted elements are appended

***Complexity.*** Let $m$ be the size of the container just before and
   $n$ just after the resize operation. Then, the work and span are
   linear and logarithmic in $\max(m, n)$, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Exchange operation {#pa-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(parray& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

### Iterator begin {#pa-beg}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator begin() const;
const_iterator cbegin() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the first item of the container.

If the container is empty, the returned iterator will be equal to
end().

***Complexity.*** Constant time.

### Iterator end {#pa-end}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator end() const;
const_iterator cend() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the element following the last item of the
container.

This element acts as a placeholder; attempting to access it results in
undefined behavior.

***Complexity.*** Constant time.

Parallel chunked sequence {#pchunkedseq}
=========================

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Item, class Alloc = std::allocator<Item>>
class pchunkedseq;

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Parallel chunked sequences are containers representing
sequences. Elements of the container are populated with their elements
in parallel. The container itself can be resized in an incremental
fashion in amortized constant time and can be split at a specified
position and concatenated in logarithmic time.

Unlike parallel arrays, parallel chunked sequences use small chunks,
usually sized to contain between 512 and 1024 items. Internally, the
chunks are stored at the leaves of a self-balancing tree. The tree is,
for all practical purposes, no more than eight levels deep. The space
usage of the sequence is, in practice, never more than 20% greater
than the space of a corresponding array.

The parallel chunked sequence class is a wrapper class whose sole
member object is a sequential chunked sequence. The wrapper function
ensures that, when accessed in bulk fashion, the sequential chunked
sequence is processed in parallel. The parallel chunked sequence class
defines wrapper methods for many of the methods of the sequential
chunked sequence class. However, additional methods can be accessed by
directly calling methods of the sequential chunked sequence
object. The complete interface of the sequential chunked sequence
class is documented
[here](http://deepsea.inria.fr/chunkedseq/doc/html/group__deque.html).

## Template parameters

+-----------------------------------+-----------------------------------+
| Template parameter                | Description                       |
+===================================+===================================+
| [`Item`](#cs-item)                | Type of the objects to be stored  |
|                                   |in the container                   |
+-----------------------------------+-----------------------------------+
| [`Alloc`](#cs-alloc)              | Allocator to be used by the       |
|                                   |container to construct and destruct|
|                                   |objects of type `Item`             |
+-----------------------------------+-----------------------------------+

Table: Template parameters for the `pchunkedseq` class.

### Item type {#cs-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the elements.  Only if `Item` is guaranteed to not throw while
moving, implementations can optimize to move elements instead of
copying them during reallocations.  Aliased as member type
`parray::value_type`.

### Allocator {#cs-alloc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Alloc;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the allocator object used to define the storage allocation
model. By default, the allocator class template is used, which defines
the simplest memory allocation model and is value-independent.
Aliased as member type `parray::allocator_type`.

## Member types

+--------------------------------------+---------------------------------------------+
| Type                                 | Description                                 |
+======================================+=============================================+
| `value_type`                         | Alias for template parameter                |
|                                      |`Item`                                       |
+--------------------------------------+---------------------------------------------+
| `reference`                          | Alias for `value_type&`                     |
+--------------------------------------+---------------------------------------------+
| `const_reference`                    | Alias for `const value_type&`               |
+--------------------------------------+---------------------------------------------+
| `pointer`                            | Alias for `value_type*`                     |
+--------------------------------------+---------------------------------------------+
| `const_pointer`                      | Alias for `const value_type*`               |
+--------------------------------------+---------------------------------------------+
| [`iterator`](#pc-iter)               | Iterator                                    |
+--------------------------------------+---------------------------------------------+
| [`const_iterator`](#pc-iter)         | Const iterator                              |
+--------------------------------------+---------------------------------------------+
| [`seq_type`](#pc-seq-type)           | Alias for                                   |
|                                      |`data::chunkedseq::bootstrapped::deque<Item>`|
+--------------------------------------+---------------------------------------------+
| [`segment_type`](#pc-seg-type)       | Alias for `typename seq_type::segment_type` |
|                                      |                                             |
+--------------------------------------+---------------------------------------------+
| [`const_segment_type`](#pc-cseg-type)| Alias for `typename                         |
|                                      |seq_type::const_segment_type`                |
+--------------------------------------+---------------------------------------------+

Table: Parallel chunked sequence member types.

### Iterator {#pc-iter}

The type `iterator` and `const_iterator` are instances of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator)
concept.

Additionally, the iterator supports *segment-wise access*, which
allows the client of the iterator to query an iterator value to
discover the contiguous region of memory in the same chunk that stores
nearby items.  The
[documentation](http://deepsea.inria.fr/chunkedseq/doc/html/group__stl__iterator.html)
on chunked sequences describes this interface in detail.

### Sequential chunked sequence type {#pc-seq-type}

The class which implements this type is described in detail
[here](http://deepsea.inria.fr/chunkedseq/doc/html/group__deque.html).

### Segment type {#pc-seg-type}

The segment type, when instantiated by the paralle chunked sequence
class, looks like the following class.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class segment {
public:
  
  pointer begin;
  pointer middle;
  pointer end;
  
  segment()
  : begin(nullptr), middle(nullptr), end(nullptr) { }
  
  segment(pointer begin, pointer middle, pointer end)
  : begin(begin), middle(middle), end(end) { }
  
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The value `begin` points to the first item in the segment, the value
`middle` to the item of interest in the segment, and `end` to the
address one past the end of the sequence.

Segments are described in detail
[here](http://deepsea.inria.fr/chunkedseq/doc/html/group__segments.html).

### Const segment type {#pc-cseg-type}

The const segment type looks the same as the segment class above,
except that `pointer` types are replaced by `const_pointer` types.

## Member objects

+-----------------------------------+-----------------------------------+
| Member objects                    | Description                       |
+===================================+===================================+
| [`seq`](#cs-seq)                  | Sequential chunked sequence       |
|                                   |                                   |
+-----------------------------------+-----------------------------------+

Table: Parallel chunked sequence member objects.

### Sequential chunked sequence {#cs-seq}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
seq_type seq;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This object provides the storage for all the elements in the
container.

## Constructors and destructors

+-----------------------------------+-----------------------------------+
| Constructor                       | Description                       |
+===================================+===================================+
| [empty container                  | constructs an empty container with|
|constructor](#cs-e-c-c) (default   |no items                           |
|constructor)                       |                                   |
+-----------------------------------+-----------------------------------+
| [fill constructor](#cs-e-f-c)     | constructs a container with a     |
|                                   |specified number of copies of a    |
|                                   |given item                         |
+-----------------------------------+-----------------------------------+
| [populate constructor](#cs-e-p-c) | constructs a container with a     |
|                                   |specified number of values that are|
|                                   |computed by a specified function   |
+-----------------------------------+-----------------------------------+
| [copy constructor](#cs-e-cp-c)    | constructs a container with a copy|
|                                   |of each of the items in the given  |
|                                   |container, in the same order       |
+-----------------------------------+-----------------------------------+
| [initializer list](#cs-i-l-c)     | constructs a container with the   |
|                                   |items specified in a given         |
|                                   |initializer list                   |
+-----------------------------------+-----------------------------------+
| [move constructor](#cs-m-c)       | constructs a container that       |
|                                   |acquires the items of a given      |
|                                   |parallel array                     |
+-----------------------------------+-----------------------------------+
| [destructor](#cs-destr)           | destructs a container             |
+-----------------------------------+-----------------------------------+

Table: Parallel chunked sequence constructors and destructors.

### Empty container constructor {#cs-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

### Fill container {#cs-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(long n, const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Populate constructor {#cs-e-p-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
// (1) Constant-time body
pchunkedseq(long n, std::function<Item(long)> body);
// (2) Non-constant-time body
pchunkedseq(long n,
            std::function<long(long)> body_comp,
            std::function<Item(long)> body);
// (3) Non-constant-time body along with range-based complexity function
pchunkedseq(long n,
            std::function<long(long,long)> body_comp_rng,
            std::function<Item(long)> body);            
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` cells, populating those cells with
values returned by the `n` calls, `body(0)`, `body(1)`, ...,
`body(n-1)`, in that order.

In the second version, the value returned by `body_comp(i)` is used by
the constructor as the complexity estimate for the call `body(i)`.

In the third version, the value returned by `body_comp(lo, hi)` is
used by the constructor as the complexity estimate for the calls
`body(lo)`, `body(lo+1)`, ... `body(hi-1)`.

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

### Copy constructor {#cs-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(const pchunkedseq& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Initializer-list constructor {#cs-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

### Move constructor {#cs-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(pchunkedseq&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

### Destructor {#cs-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~pchunkedseq();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

## Member functions

+-------------------------------------+--------------------------------------+
| Operation                           | Description                          |
+=====================================+======================================+
|   [`operator[]`](#cs-i-o)           | Access member item                   |
+-------------------------------------+--------------------------------------+
| [`size`](#cs-si)                    | Return size                          |
+-------------------------------------+--------------------------------------+
| [`swap`](#cs-sw)                    | Exchange contents                    |
+-------------------------------------+--------------------------------------+
| [`begin`](#cs-beg)                  | Returns an iterator to the beginning |
| [`cbegin`](#cs-beg)                 |                                      |
+-------------------------------------+--------------------------------------+
| [`end`](#cs-end)                    | Returns an iterator the end          |
| [`cend`](#cs-end)                   |                                      |
+-------------------------------------+--------------------------------------+
| [`clear`](#cs-clear)                | Erases contents of the container     |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`tabulate`](#cs-rbld)              | Repopulate container changing size   |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`resize`](#cs-rsz)                 | Change size                          |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
                 
Table: Operations of the parallel chunked sequence.

### Indexing operator {#cs-i-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
reference operator[](long i);
const_reference operator[](long i) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a reference at the specified location `i`. No bounds check is
performed.

***Complexity.*** Logarithmic time. The complexity is, however, for
practical purposes, constant time, because thanks to the high
branching factor the height of the tree is, in practice, never more
than eight.

### Size operator {#cs-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

### Exchange operation {#cs-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(pchunkedseq& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

### Iterator begin {#cs-beg}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator begin() const;
const_iterator cbegin() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the first item of the container.

If the container is empty, the returned iterator will be equal to
end().

***Complexity.*** Constant time.

### Iterator end {#cs-end}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator end() const;
const_iterator cend() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the element following the last item of the
container.

This element acts as a placeholder; attempting to access it results in
undefined behavior.

***Complexity.*** Constant time.

### Clear {#cs-clear}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void clear();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Erases the contents of the container, which becomes an empty
container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Tabulate {#cs-rbld}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void tabulate(long n, std::function<value_type(long)> body);
void tabulate(long n,
              std::function<long(long)> body_comp_rng,
              std::function<value_type(long)> body);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container so that it contains `n` items.

The contents of the current container are removed and replaced by the
`n` items returned by the `n` calls, `body(0)`, `body(1)`, ...,
`body(n-1)`, in that order.

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Resize {#cs-rsz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void resize(long n, const value_type& val);  // (1)
void resize(long n) {                        // (2)
  value_type val;
  resize(n, val);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container to contain `n` items.

If the current size is greater than `n`, the container is reduced to
its first `n` elements.

If the current size is less than `n`,

1. additional copies of `val` are appended
2. additional default-inserted elements are appended

***Complexity.*** Let $m$ be the size of the container just before and
   $n$ just after the resize operation. Then, the work and span are
   linear and logarithmic in $\max(m, n)$, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

## Non-member functions

+-------------------------------------+--------------------------------------+
| Function                            | Description                          |
+=====================================+======================================+
| [`for_each`](#cs-foreach)           | Applies a given function to each item|
|                                     |in the container                      |
+-------------------------------------+--------------------------------------+
| [`for_each_segment`](#cs-foreachseg)| Applies a given function to each     |
|                                     |segment in the container              |
+-------------------------------------+--------------------------------------+

Table: Functions relating to the parallel chunked sequence.

### For each {#cs-foreach}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace segmented {

template <class Body>
void for_each(Body body);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Performs the calls `body(x1)`, ..., `body(xn)` for each item `xi` in
the container.

The class `Body` should provide a call operator of the following
type. The reference `xi` points to the $i^{th}$ item in the container.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Item& xi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

### For each segment {#cs-foreachseg}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace segmented {

template <class Body_seg>
void for_each_segment(Body_seg body_seg);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Performs the calls `body_seg(lo1, hi1)`, ..., `body(lon, hin)` for
each segment of items (`loi`, `hii`) in the container.

The class `Body_seg` should provide a call operator of the following
type. The `lo` value points to the first item in the segment and `hi`
one past the end of the segment.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(pointer lo, pointer hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing
$\mathtt{body\_seg}(i,i+1)$ and $s(i)$ the corresponding span cost.

Parallel set {#pset}
============

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Item,
  class Compare = std::less<Item>,
  class Alloc = std::allocator<Item>,
  int chunk_capacity = 8
>
class pset;

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The set container stores unique elements in ascending order. Order
among the container elements is maintained by a comparison operator,
which is provided by the template parameter `Compare`. The value of
the elements in the set container cannot be modified once in the
container, but they can be inserted or removed from the container.

Just like the STL `set` container, our `pset` container provides
`insert` and `erase` methods. Both methods take logarithmic time in
the size of the container.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset<int> s;
s.insert(5);
s.insert(432);
s.insert(5);
s.insert(89);
s.erase(5);
std::cout << "s = " << s << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This program prints the following.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
s = { 89, 432 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unlike an STL `set`, our `pset` provides bulk set operations,
including union (i.e., merge), intersection, and difference. Moreover,
these methods are highly parallel: they take linear work and
logarithmic span in the total size of the two containers being
combined. The `merge` method computes the set union with a given
container, leaving the result in the targeted container and leaving
the given container empty.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset<int> s1 = { 3, 1, 5, 8, 12 };
pset<int> s2 = { 3, 54, 8, 9 };
s1.merge(s2);
std::cout << "s1 = " << s1 << std::endl;
std::cout << "s2 = " << s2 << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
s1 = { 1, 3, 5, 8, 9, 12, 54 }
s2 = {  }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set intersection is handled in a similar fashion, by the
`intersection` method.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset<int> s1 = { 3, 1, 5, 8, 12 };
pset<int> s2 = { 3, 54, 8, 9 };
s1.intersect(s2);
std::cout << "s1 = " << s1 << std::endl;
std::cout << "s2 = " << s2 << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
s1 = { 3, 8 }
s2 = {  }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set difference is handled similarly by the `diff` method.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset<int> s1 = { 3, 1, 5, 8, 12 };
pset<int> s2 = { 3, 54, 8, 9 };
s1.diff(s2);
std::cout << "s1 = " << s1 << std::endl;
std::cout << "s2 = " << s2 << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
s1 = { 1, 5, 12 }
s2 = {  }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


## Template parameters

+-----------------------------------+-----------------------------------+
| Template parameter                | Description                       |
+===================================+===================================+
| [`Item`](#pset-item)              | Type of the objects to be stored  |
|                                   |in the container                   |
+-----------------------------------+-----------------------------------+
| [`Compare`](#pset-compare)        | Type of the comparison function   |
|                                   |                                   |
|                                   |                                   |
+-----------------------------------+-----------------------------------+
| [`Alloc`](#pset-alloc)            | Allocator to be used by the       |
|                                   |container to construct and destruct|
|                                   |objects of type `Item`             |
+-----------------------------------+-----------------------------------+
| [`chunk_capacity`](#pset-ccap)    | Capacity of the contiguous chunks |
|                                   |of memory that are used by the     |
|                                   |container to store items           |
+-----------------------------------+-----------------------------------+

Table: Template parameters for the `pset` class.

### Item type {#pset-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the elements.  Only if `Item` is guaranteed to not throw while
moving, implementations can optimize to move elements instead of
copying them during reallocations.  Aliased as member type
`pset::value_type`.

### Compare function {#pset-compare}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Compare;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool operator()(const Item& lhs, const Item& rhs);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Allocator {#pset-alloc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Alloc;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the allocator object used to define the storage allocation
model. By default, the allocator class template is used, which defines
the simplest memory allocation model and is value-independent.
Aliased as member type `pset::allocator_type`.

### Chunk capacity {#pset-ccap}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int chunk_capacity;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An integer which determines the maximum size of a contiguous chunk of
memory in the underlying container. The chunks are used by the
container to store the items. The minimum size of a chunk is
guaranteed by the container to be at least `chunk_size/2`.

## Member types

+-----------------------------------+-----------------------------------+
| Type                              | Description                       |
+===================================+===================================+
| `value_type`                      | Alias for template parameter      |
|                                   |`Item`                             |
+-----------------------------------+-----------------------------------+
| `key_type`                        | Alias for template parameter      |
|                                   |`Item`                             |
+-----------------------------------+-----------------------------------+
| `key_compare`                     | Alias for template parameter      |
|                                   |`Compare`                          |
+-----------------------------------+-----------------------------------+
| `reference`                       | Alias for `value_type&`           |
+-----------------------------------+-----------------------------------+
| `const_reference`                 | Alias for `const value_type&`     |
+-----------------------------------+-----------------------------------+
| `pointer`                         | Alias for `value_type*`           |
+-----------------------------------+-----------------------------------+
| `const_pointer`                   | Alias for `const value_type*`     |
+-----------------------------------+-----------------------------------+
| [`iterator`](#pset-iter)          | Iterator                          |
+-----------------------------------+-----------------------------------+
| [`const_iterator`](#pset-iter)    | Const iterator                    |
+-----------------------------------+-----------------------------------+

Table: Parallel-set member types.

### Iterator {#pset-iter}

The type `iterator` and `const_iterator` are instances of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator)
concept.

## Constructors and destructors

+-----------------------------------+-----------------------------------+
| Constructor                       | Description                       |
+===================================+===================================+
| [empty container                  | constructs an empty container with|
|constructor](#pset-e-c-c) (default |no items                           |
|constructor)                       |                                   |
+-----------------------------------+-----------------------------------+
| [fill constructor](#pset-e-f-c)   | constructs a container with a     |
|                                   |specified number of copies of a    |
|                                   |given item                         |
+-----------------------------------+-----------------------------------+
|[populate constructor](#pset-e-p-c)| constructs a container with a     |
|                                   |specified number of values that are|
|                                   |computed by a specified function   |
+-----------------------------------+-----------------------------------+
| [copy constructor](#pset-e-cp-c)  | constructs a container with a copy|
|                                   |of each of the items in the given  |
|                                   |container, in the same order       |
+-----------------------------------+-----------------------------------+
| [initializer list](#pset-i-l-c)   | constructs a container with the   |
|                                   |items specified in a given         |
|                                   |initializer list                   |
+-----------------------------------+-----------------------------------+
| [move constructor](#pset-m-c)     | constructs a container that       |
|                                   |acquires the items of a given      |
|                                   |parallel array                     |
+-----------------------------------+-----------------------------------+
| [destructor](#pset-destr)         | destructs a container             |
+-----------------------------------+-----------------------------------+

Table: Parallel set constructors and destructors.

### Empty container constructor {#pset-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

### Fill container {#pset-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset(long n, const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Populate constructor {#pset-e-p-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
// (1) Constant-time body
pset(long n, std::function<Item(long)> body);
// (2) Non-constant-time body
pset(long n,
     std::function<long(long)> body_comp,
     std::function<Item(long)> body);
// (3) Non-constant-time body along with range-based complexity function
pset(long n,
     std::function<long(long,long)> body_comp_rng,
     std::function<Item(long)> body);            
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` cells, populating those cells with
values returned by the `n` calls, `body(0)`, `body(1)`, ...,
`body(n-1)`, in that order.

In the second version, the value returned by `body_comp(i)` is used by
the constructor as the complexity estimate for the call `body(i)`.

In the third version, the value returned by `body_comp(lo, hi)` is
used by the constructor as the complexity estimate for the calls
`body(lo)`, `body(lo+1)`, ... `body(hi-1)`.

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

### Copy constructor {#pset-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset(const pset& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Initializer-list constructor {#pset-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

### Move constructor {#pset-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pset(pset&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

### Destructor {#pset-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~pset();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

## Member functions

+-------------------------------------+--------------------------------------+
| Operation                           | Description                          |
+-------------------------------------+--------------------------------------+
| [`size`](#pset-si)                  | Return size                          |
+-------------------------------------+--------------------------------------+
| [`swap`](#pset-sw)                  | Exchange contents                    |
+-------------------------------------+--------------------------------------+
| [`begin`](#pset-beg)                | Returns an iterator to the beginning |
| [`cbegin`](#pset-beg)               |                                      |
+-------------------------------------+--------------------------------------+
| [`end`](#pset-end)                  | Returns an iterator the end          |
| [`cend`](#pset-end)                 |                                      |
+-------------------------------------+--------------------------------------+
| [`clear`](#pset-clear)              | Erases contents of the container     |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`find`](#pset-find)                | Get iterator to element              |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`insert`](#pset-insert)            | Insert element                       |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`erase`](#pset-erase)              | Remove element                       |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`merge`](#pset-merge)              | Take set union with given pset       |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`intersect`](#pset-intersect)      | Take set intersection with given pset|
|                                     |                                      |
+-------------------------------------+--------------------------------------+
| [`diff`](#pset-diff)                | Take set difference with give pset   |
|                                     |                                      |
+-------------------------------------+--------------------------------------+
          
Table: Operations of the parallel set.

### Size operator {#pset-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

### Exchange oparation

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(pset& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of other. Does not
invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

### Iterator begin {#pset-beg}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator begin() const;
const_iterator cbegin() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the first item of the container.

If the container is empty, the returned iterator will be equal to
end().

***Complexity.*** Constant time.

### Iterator end {#pset-end}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator end() const;
const_iterator cend() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the element following the last item of the
container.

This element acts as a placeholder; attempting to access it results in
undefined behavior.

***Complexity.*** Constant time.

### Clear {#pset-clear}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void clear();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Erases the contents of the container, which becomes an empty
container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Find {#pset-find}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
itereator find(const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Searches the container for an element equivalent to `val` and returns
an iterator to it if found, otherwise it returns an iterator to
`pset::end`.

Two elements of a set are considered equivalent if the container's
comparison object returns false reflexively (i.e., no matter the order
in which the elements are passed as arguments).

***Complexity.*** Logarithmic time.

### Insert {#pset-insert}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pair<iterator,bool> insert (const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Extends the container by inserting new elements, effectively
increasing the container size by the number of elements inserted.

Because elements in a set are unique, the insertion operation checks
whether each inserted element is equivalent to an element already in
the container, and if so, the element is not inserted, returning an
iterator to this existing element (if the function returns a value).

***Complexity.*** Logarithmic time.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Erase {#pset-erase}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
size_type erase (const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Removes from the set the value `val`, if it is present.

This effectively reduces the container size by the number of elements
removed, which are destroyed.

***Complexity.*** Logarithmic time.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Set union {#pset-merge}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void merge(pset& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Leaves in the current container the result of taking the set union of
the original container and the `other` container and leaves the
`other` container empty.

***Complexity.*** Work and span are linear and logarithmic in the
   combined sizes of the two containers, respectively.

***Iterator validity.*** Invalidates all iterators.
   
### Set intersection {#pset-intersect}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void intersect(pset& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Leaves in the current container the result of taking the set
intersection of the original container and the `other` container and
leaves the `other` container empty.

***Complexity.*** Work and span are linear and logarithmic in the
   combined sizes of the two containers, respectively.

***Iterator validity.*** Invalidates all iterators.

### Set difference {#pset-diff}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void diff(pset& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Leaves in the current container the result of taking the difference
intersection between the original container and the `other` container
and leaves the `other` container empty.

***Complexity.*** Work and span are linear and logarithmic in the
   combined sizes of the two containers, respectively.

***Iterator validity.*** Invalidates all iterators.

Parallel map {#pmap}
============

Parallel string {#pstring}
===============

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Alloc = std::allocator<Item>>
class pstring;

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

## Member types

+-----------------------------------+-----------------------------------+
| Type                              | Description                       |
+===================================+===================================+
| `value_type`                      | Alias for type `char`             |
|                                   |                                   |
+-----------------------------------+-----------------------------------+
| `reference`                       | Alias for `value_type&`           |
+-----------------------------------+-----------------------------------+
| `const_reference`                 | Alias for `const value_type&`     |
+-----------------------------------+-----------------------------------+
| `pointer`                         | Alias for `value_type*`           |
+-----------------------------------+-----------------------------------+
| `const_pointer`                   | Alias for `const value_type*`     |
+-----------------------------------+-----------------------------------+
| [`iterator`](#ps-iter)            | Iterator                          |
+-----------------------------------+-----------------------------------+
| [`const_iterator`](#ps-iter)      | Const iterator                    |
+-----------------------------------+-----------------------------------+

Table: Parallel string member types.

### Iterator {#ps-iter}

The type `iterator` and `const_iterator` are instances of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator)
concept.

## Constructors and destructors

+-----------------------------------+-----------------------------------+
| Constructor                       | Description                       |
+===================================+===================================+
| [empty container                  | constructs an empty container with|
|constructor](#ps-e-c-c) (default   |no items                           |
|constructor)                       |                                   |
+-----------------------------------+-----------------------------------+
| [fill constructor](#ps-e-f-c)     | constructs a container with a     |
|                                   |specified number of copies of a    |
|                                   |given item                         |
+-----------------------------------+-----------------------------------+
| [populate constructor](#ps-e-p-c) | constructs a container with a     |
|                                   |specified number of values that are|
|                                   |computed by a specified function   |
+-----------------------------------+-----------------------------------+
| [copy constructor](#ps-e-cp-c)    | constructs a container with a copy|
|                                   |of each of the items in the given  |
|                                   |container, in the same order       |
+-----------------------------------+-----------------------------------+
| [initializer list](#ps-i-l-c)     | constructs a container with the   |
|                                   |items specified in a given         |
|                                   |initializer list                   |
+-----------------------------------+-----------------------------------+
| [move constructor](#ps-m-c)       | constructs a container that       |
|                                   |acquires the items of a given      |
|                                   |parallelstring                     |
+-----------------------------------+-----------------------------------+
| [destructor](#ps-destr)           | destructs a container             |
+-----------------------------------+-----------------------------------+

Table: Parallel-string constructors and destructors.

### Empty container constructor {#ps-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

### Fill container {#ps-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring(long n, char val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Populate constructor {#ps-e-p-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
// (1) Constant-time body
pstring(long n, std::function<char(long)> body);
// (2) Non-constant-time body
pstring(long n,
        std::function<long(long)> body_comp,
        std::function<char(long)> body);
// (3) Non-constant-time body along with range-based complexity function
pstring(long n,
        std::function<long(long,long)> body_comp_rng,
        std::function<char(long)> body);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` cells, populating those cells with
values returned by the `n` calls, `body(0)`, `body(1)`, ...,
`body(n-1)`, in that order.

In the second version, the value returned by `body_comp(i)` is used by
the constructor as the complexity estimate for the call `body(i)`.

In the third version, the value returned by `body_comp(lo, hi)` is
used by the constructor as the complexity estimate for the calls
`body(lo)`, `body(lo+1)`, ... `body(hi-1)`.

***Complexity.*** The work and span cost are $(\sum_{0 \leq i < n}
w(i))$ and $(\log n + \max_{0 \leq i < n} s(i))$, respectively, where
$w(i)$ represents the work cost of computing $\mathtt{body}(i)$ and
$s(i)$ the corresponding span cost.

### Copy constructor {#ps-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring(const pstring& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

### Initializer-list constructor {#ps-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

### Move constructor {#ps-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring(pstring&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

### Destructor {#ps-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~pstring();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

## Operations

+---------------------------+--------------------------------------+
| Operation                 | Description                          |
+===========================+======================================+
| [`operator[]`](#ps-i-o)   | Access member item                   |
|                           |                                      |
+---------------------------+--------------------------------------+
| [`size`](#ps-si)          | Return size                          |
+---------------------------+--------------------------------------+
| [`clear`](#ps-clear)      | Erase contents                       |
+---------------------------+--------------------------------------+
| [`resize`](#ps-rsz)       | Change size                          |
+---------------------------+--------------------------------------+
| [`swap`](#ps-sw)          | Exchange contents                    |
+---------------------------+--------------------------------------+
| [`begin`](#ps-beg)        | Returns an iterator to the beginning |
| [`cbegin`](#ps-beg)       |                                      |
+---------------------------+--------------------------------------+
| [`end`](#ps-end)          | Returns an iterator to the end       |
| [`cend`](#ps-end)         |                                      |
+---------------------------+--------------------------------------+
| [`operator+=`](#ps-append)| Append to string                     |
|                           |                                      |
+---------------------------+--------------------------------------+
| [`operator+`](#ps-concat) | Concatenates strings                 |
|                           |                                      |
+---------------------------+--------------------------------------+
| [`c_str`](#ps-cstr)       | Get C string equivalent              |
|                           |                                      |
+---------------------------+--------------------------------------+

Table: Parallel-array member functions.

### Indexing operator {#ps-i-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
reference operator[](long i);
const_reference operator[](long i) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a reference at the specified location `i`. No bounds check is
performed.

***Complexity.*** Constant time.

### Size operator {#ps-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

### Clear {#ps-clear}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void clear();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Erases the contents of the container, which becomes an empty
container.

***Complexity.*** Linear and logarithmic work and span, respectively.

### Resize {#ps-rsz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void resize(long n, char val); // (1)
void resize(long n) {                       // (2)
  char val;
  resize(n, val);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container to contain `n` items.

If the current size is greater than `n`, the container is reduced to
its first `n` elements.

If the current size is less than `n`,

1. additional copies of `val` are appended
2. additional default-inserted elements are appended

***Complexity.*** Let $m$ be the size of the container just before and
   $n$ just after the resize operation. Then, the work and span are
   linear and logarithmic in $\max(m, n)$, respectively.

### Exchange operation {#ps-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(pstring& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

### Iterator begin {#ps-beg}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator begin() const;
const_iterator cbegin() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the first item of the container.

If the container is empty, the returned iterator will be equal to
end().

***Complexity.*** Constant time.

### Iterator end {#ps-end}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
iterator end() const;
const_iterator cend() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns an iterator to the element following the last item of the
container.

This element acts as a placeholder; attempting to access it results in
undefined behavior.

***Complexity.*** Constant time.

### Append to string {#ps-append}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring& operator+=(const pstring& str);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Extends the string by appending additional characters at the end of
its current value.

***Complexity.*** Linear and logarithmic in the size of the resulting
   string, respectively.

***Iterator validity.*** Invalidates all iterators, if the size before
   the operation differs from the size after.

### Concatenate strings {#ps-concat}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pstring operator+(const pstring& rhs);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a newly constructed string object with its value being the
concatenation of the characters in `this` followed by those of `rhs`.

***Complexity.*** Linear and logarithmic in the size of the resulting
   string, respectively.

### Get C string equivalent {#ps-cstring}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
const char* c_str();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a pointer to an array that contains a null-terminated sequence
of characters (i.e., a C-string) representing the current value of the
string object.

This array includes the same sequence of characters that make up the
value of the string object plus an additional terminating
null-character (`'\0'`) at the end.

***Complexity.*** Constant time.

Data-parallel operations
========================

This section introduces a collection of data-parallel operations that
are useful for processing over pctl containers.

Parallel-for loop
-----------------

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter, class Body>
void parallel_for(Iter lo, Iter hi, Body body);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first of data-parallel operations we are going to consider is a
looping construct that is useful for making changes to memory. For
now, we are going to assume that each iterate of our loops take
constant time. In the next section, we show how we can handle in an
effective manner loop bodies that are not constant time.

Let us begin by considering example code. The following program uses a
parallel-for loop to assign each position `i` in `xs` to the value
`i+1`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<long> xs = { 0, 0, 0, 0 };
parallel_for((long)0, xs.size(), [&] (long i) {
  xs[i] = i+1;
});

std::cout << "xs = " << xs << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's output is the following.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
xs = { 1, 2, 3, 4 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We can just as easily configure our parallel-for loop to iterate with
respect to a specified range of iterator values. In this case, we are
instead just incrementing the value in each cell of our parallel
array.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<long> xs = { 0, 1, 2, 3 };
parallel_for(xs.begin(), xs.end(), [&] (long* p) {
  long& xs_i = *p;
  xs_i = xs_i + 1;
});

std::cout << "xs = " << xs << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output of this program is the same as that of the one just above.

By exchanging the type `parray` for `pchunkedseq` and the iterator
type `long*` for `typename pchunkedseq<long>::iterator`, we can
iterate over a parallel chunked sequence in a similar manner.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq<long> xs = { 0, 1, 2, 3 };
parallel_for(xs.begin(), xs.end(), [&] (typename pchunkedseq<long>::iterator p) {
  long& xs_i = *p;
  xs_i = xs_i + 1;
});

std::cout << "xs = " << xs << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[parallelfor.cpp]: ../example/parallelfor.cpp

The output of this program is the same as that of the one just
above. All of the examples presented here can be found in the source
file [parallelfor.cpp].

### Non-constant-time loop bodies {#weighted-parallel-for}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter, class Body, class Comp>
void parallel_for(Iter lo, Iter hi, Comp comp, Body body);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the loop body does not take constant time, that is, the loop body
takes time in proportion to a known quantity, our code needs to
provide a runction of that quantity to the looping construct in order
for the loop to find an efficient schedule for the loop iterations. To
see how this reporting is done, let us consider a program that
computes the product of a dense matrix by a dense vector. Our matrix
is $\mathtt{n} \times \mathtt{n}$ and is laid out in memory in row
major format. In the following example, we call `dmdmult1` to compute
the vector resulting from the multiplication of matrix `mtx` and
vector `vec`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<double> mtx = { 1.1, 2.1, 0.3, 5.8,
                       8.1, 9.3, 3.1, 3.2,
                       5.3, 3.5, 7.9, 2.3,
                       4.5, 5.5, 3.4, 4.5 };

parray<double> vec = { 4.3, 0.3, 2.1, 3.3 };

parray<double> result = dmdvmult1(mtx, vec);
std::cout << "result = " << result << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output of the program is the following.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
result = { 25.13, 54.69, 48.02, 42.99 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the body of `dmdvmult1`, we need to compute the dot product of a
given row by our vector `vec`. Let us just assume that we have such a
function already. In particular, the `ddotprod` function takes the
dimension value, a pointer to the start of a row in the matrix, a
pointer to the vector. The function returns the corresponding dot
product, taking time $O(\mathtt{n})$.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
double ddotprod(long n, const double* row, const double* vec);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now, we see that the body of our `dmdvmult1` function uses a
parallel-for loop to fill each cell in the result vector. The
complexity function `comp` describes the approximate cost of
performing one loop iterate, which is the same as performing one dot
product operation on a specified row.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<double> dmdvmult1(const parray<double>& mtx, const parray<double>& vec) {
  long n = vec.size();
  parray<double> result(n);
  auto comp = [&] (long i) {
    return n;
  };
  parallel_for(0L, n, comp, [&] (long i) {
    result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
  });
  return result;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The above code is perfectly fine for most purposes, because the
granularity-control algorithm of pctl should be able to use the
complexity function to schedule loop iterations efficiently. However,
this particular type of parallel-for loop imposes scheduling-related
overhead that is important to be aware of. In particular, inside the
implementation of this particular parallel-for loop, there is an
operation that is being called to precompute the cost of computing any
subrange of the parallel-for loop iterations.

### The `weights` operation {#pfor-weights}

This operation is a function which takes a number `n` and a weight
function `w` and returns a *weight table*.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
template <class Weight>
parray<long> weights(long n, Weight weight);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The result returned by the call `weights(n, w)` is the sequence `[0,
w(0), w(0)+w(1), w(0)+w(1)+w(2), ..., w(0)+...+w(n-1)]`. Notice that
the size of the value returned by the `weights` function is always
`n+1`. The work and span cost of this operation are linear and
logarithmic in `n`, respectively.

Let us consider how the range-based parallel-for loop in `dmdvmult2`
would call the weights function. Suppose that `n` = 4. Now,
internally, the parallel-for loop is going to compute a weight table
that looks the same as `w` in the code below.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<long> w = weights(4, comp);

std::cout << "w = " << w << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output is going to be as shown below.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
w = { 0, 4, 8, 12, 16 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For one more example, let us consider an application of the `weights`
function where the given weight function is one that returns the value
of its current position.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<long> w = weights(4, [&] (long i) {
  return i;
});

std::cout << "w = " << w << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output is the following.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
w = { 0, 0, 1, 3, 6 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returning to our `dmdvmult2` example, we can now see that the work and
span cost of calculating the weights table are linear and logarithmic
in `n`, respectively. Since the total cost of the multiplication is
$O(\mathtt{n}^2)$, the cost of calculating the weights table is
relatively negligible and can therefore be disregarded in this
case. Nevertheless, in other cases, we may want to avoid the cost of
calculating the weights table. To do so, we need to consider the
*range-based* parallel-for loop.

### Range-based complexity functions for non-constant-time loop bodies

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <
  class Iter,
  class Body,
  class Comp_rng
>
void parallel_for(Iter lo,
                  Iter hi, Comp_rng comp_rng,
                  Body body);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The idea here is that, instead of reporting the cost of computing an
individual iterate, we report the cost of a given range. The function
`comp_rng` is going to calculate and return exactly this value. The
code below shows one way that we can make this modification to our
`dmdvmult2` function.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<double> dmdvmult2(const parray<double>& mtx, const parray<double>& vec) {
  long n = vec.size();
  parray<double> result(n);
  auto comp_rng = [&] (long lo, long hi) {
    return (hi - lo) * n;
  };
  range::parallel_for(0L, n, comp_rng, [&] (long i) {
    result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
  });
  return result;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The idea is that instead of reporting the cost of computing the dot
product on an individual row, we instead report the cost of performing
a sequence of dot products on a specified range. In particular, the
`comp_rng` function calculates the cost of performing `hi-lo` dot
products. In this way, we have bypassed having to calculate the
weights table by providing such a range-based complexity function.

### Sequential-alternative loop bodies {#pfor-sequential-alt}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
void parallel_for(Iter lo,
                  Iter hi,
                  Comp_rng comp_rng,
                  Body body,
                  Seq_body_rng seq_body_rng);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let us suppose that we have some highly optimized sequential algorithm
that we wish to use along with our parallel-for loop. How can we
inject such code into a parallel-for computation? For this purpose, we
have another form of parallel-for loop that takes a sequential-body
function as its last argument.

Now, we can see from the following example code how we can use the new
parallel-for loop. Just after the parallel body, we see a sequential
body, which consists of two ordinary, sequential for loops. When the
scheduler determines that a loop range is too small to benefit from
parallelism, the body that is applied is the sequential body.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<double> dmdvmult3(const parray<double>& mtx, const parray<double>& vec) {
  long n = vec.size();
  parray<double> result(n);
  auto comp_rng = [&] (long lo, long hi) {
    return (hi - lo) * n;
  };
  range::parallel_for(0L, n, comp_rng, [&] (long i) {
    result[i] = ddotprod(n, mtx.cbegin()+(i*n), vec.begin());
  }, [&] (long lo, long hi) {
    for (long i = lo; i < hi; i++) {
      double dotp = 0.0;
      for (long j = 0; j < n; j++) {
        dotp += mtx[i*n+j] * vec[j];
      }
      result[i] = dotp;
    }
  });
  return result;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

At first glance, our optimization of injecting a sequential body might
seem artificial, given that, for a large enough matrix, the sequential
body may never get applied. The reason is that the work involved in
processing just a single, large row may be plenty to warrant parallel
execution. However, if we consider rectangular matrices, or sparce
matrices, it should be clear that the sequential body can improve
performance in certain cases.

[dmdvmult.cpp]: ../example/dmdvmult.cpp

To summarize, we started with a basic parallel-for loop, showing that
the basic version is flexible enough to handle various indexing
formats (e.g., integers, iterators of various kinds). We then saw
that, when the body of the loop is not constant time, we need to
define a complexity function and pass the complexity function to the
parallel-for function. We then saw that we can potentially reduce the
scheduling overhead by instead using a range-based version of parallel
for, where we pass a range-based complexity function. Then, we saw
that we can potentially optimize further by providing a sequential
alternative body to the parallel for. Finally, all of the code
examples presented in this section can be found in the file
[dmdvmult.cpp].

### Template parameters

The following table describes the template parameters that are used by
the different version of our parallel-for function.

+---------------------------------+-----------------------------------+
| Template parameter              | Description                       |
+=================================+===================================+
| [`Iter`](#lp-iter)              | Type of the iterator to be used by|
|                                 |the loop                           |
+---------------------------------+-----------------------------------+
| [`Body`](#lp-i)                 | Loop body                         |
+---------------------------------+-----------------------------------+
| [`Seq_body_rng`](#lp-s-i)       | Sequentialized version of the body|
+---------------------------------+-----------------------------------+
| [`Comp`](#lp-c)                 | Complexity function for a         |
|                                 |specified iteration                |
+---------------------------------+-----------------------------------+
| [`Comp_rng`](#lp-c-r)           | Complexity function for a         |
|                                 |specified range of iterations      |
+---------------------------------+-----------------------------------+

Table: All template parameters used by various instance of the
parallel-for loop.

#### Loop iterator {#lp-iter}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Iter;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

At a minimum, any value of type `Iter` must support the following
operations. Let `a` and `b` denote values of type `Iter` and `n` a
value of type `long`.  Then, we need the subtraction operation `a-b`,
the comparison operation `a!=b`, the addition-by-a-number-operation
`a+n`, and the increment operation `a++`.

As such, the concept of the `Iter` class bears resemblance to the
concept of the [random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator).
The main difference between the two is that, with the random-access
iterator, an iterable value necessarily has the ability to
dereference, whereas with our `Iter` class this feature is not used by
the parallel-for loop and therefore not required.

#### Loop body {#lp-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Body;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The loop body class must provide a call operator of the following
type. The iterator parameter is to receive the value of the current
iterate.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Iter it);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Sequentialized loop body {#lp-s-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_body_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential loop body class must provide a call operator of the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Iter lo, Iter hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This method is called by the scheduler when a given range of loop
iterates is determined to be too small to benefit from parallel
execution. This range to be executed sequentially is the right-open
range [`lo`, `hi`).

#### Complexity function {#lp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Comp;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The complexity-function class must provide a call operator of the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(Iter it);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This method is called by the scheduler to determine the cost of
executing a given iterate, specified by `it`. The return value should
be a positive number that represents an approximate (i.e., asymptotic)
cost required to execute the loop body at the iterate `it`.

#### Range-based compelxity function {#lp-c-r}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Comp_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The range-based complexity-function class must provide a call operator
of the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(Iter lo, Iter hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This method is called by the scheduler to determine the cost of a
*range* of loop iterates. The range is specified by the right-open
range [`lo`, `hi`). Like the above function, the return value should
be a positive number that represents an approximate (i.e., asymptotic)
cost required to execute specified range.

Reductions and scans
--------------------

Parallel-for loops give us the ability to process in parallel over a
specified range of iterates. However, often, we want to, say, take the
sum of a given sequence of numbers, or perhaps, find the lowest point
in a given set of points in two-dimensional space. The problem is that
the parallel-for loop is the wrong tool for this job, because it is
not safe to use a parallel-for loop to accumulate a value in a shared
memory cell. We need another mechanism if we want to program such
accumulation patterns. In general, when we have a collection of values
that we want to combine in a certain fashion, we can often obtain an
efficient and clean solution by using a *reduction*, which is an
operation that combines the items in a specified range in a certain
fashion.

The mechanism that a reduction uses to combine a given collection of
items is a mathematical object called a *monoid*. The reason that
monoids are useful is because, if the problem can be stated in terms
of a monoid, then a monoid provides a simple, yet sufficient condition
to find a corresponding reduction that delivers an efficient parallel
solution. In other words, a monoid is a bit like a design pattern for
programming parallel algorithms. Recall that a monoid is an algebraic
structure that consists of a set $T$, an associative binary operation
$\oplus$ and an identity element $\mathbf{I}$. That is, $(T, \oplus,
\mathbf{I})$ is a monoid if:

- $\oplus$ is associative: for every $x$, $y$ and $z$ in $T$, 
  $x \oplus (y \oplus z) = (x \oplus y) \oplus z$.
- $\mathbf{I}$ is the identity for $\oplus$: for every $x$ in $T$,
  $x \oplus \mathbf{I} = \mathbf{I} \oplus x$.

Examples of monoids include the following:

- $T$ = the set of all integers; $\oplus$ = addition; $\mathbf{I}$
  = 0
- $T$ = the set of 32-bit unsigned integers; $\oplus$ = addition
  modulo $2^{32}$; $\mathbf{I}$ = 0
- $T$ = the set of all strings; $\oplus$ = concatenation;
  $\mathbf{I}$ = the empty string
- $T$ = the set of 32-bit signed integers; $\oplus$ = `std::max`;
  $\mathbf{I}$ = $-2^{32}+1$

Let us now see how we can encode a reduction in C++.

### Basic reduction {#reduce-basic}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter, class Item, class Combine>
Item reduce(Iter lo, Iter hi, Item id, Combine combine);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The most basic reduction pattern that is provided by pctl is the one
above. This template function applies a monoid, encoded by the
identity element `id` and the associative combining operator
`combine`, to return the value computed by combining together the
items contained by the given right-open range [`lo`, `hi`). For
example, suppose that we are given as input an array of numbers and
our task is to find the largest number using linear work and
logarithmic span. We can solve this problem by using our basic
reduction, as shown below.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int max(const parray<int>& xs) {
  int id = std::numeric_limits<int>::lowest();
  return reduce(xs.cbegin(), xs.cend(), id, [&] (int x, int y) {
    return std::max(x, y);
  });
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let us pause for a moment to consider what is the value returned by the
`max` function when it is passed the empty array (i.e., `max({ })`. In
this case, the value is going to be the smallest 

As we are going to see [later](#r0-complexity), the work and span cost
of this operation are linear and logarithmic in the size of the input
array, implying a good parallel solution to this particular problem.

It is worth noting that reduction can be applied to any container type
that provides a [random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator). For
example, we could obtain a similarly valid and efficient parallel
solution to find the max of the items in a chunked sequence, for
example, by simply replacing the type `parray` in the code above by
the type `pchunkedseq`.

### Basic reduction with non-constant-time combining operators {#reduction-nonconstant}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Iter,
  class Item,
  class Weight,
  class Combine
>
Item reduce(Iter lo,
            Iter hi,
            Item id,
            Weight weight,
            Combine combine);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Recall that, for parallel-for loops, we had to take special measures
in order to properly encode loops with non-constant-time loop
bodies. The situation is similar for reductions: if the associative
combining operator is not constant time, then we need to provide
an extra function to report the costs. However, the way that
we are going to report is a little different that the way
we reported to the parallel-for loop.

What we want to report is the *weight* of each item in the input
sequence. The idea is that the cost of combining any two items in the
input sequence using the given associative combining operator is the
sum of the weights of the two items. Let us consider an example of
this pattern. The code below takes as input an array of arrays of
numbers and returns the largest number contained by the
subarrays. Now, observe that the cost of performing our `combine`
operator to any two subarrays `xs1` and `xs2` is the value `xs1.size()
+ xs2.size()`. It should be clear that our reduce operation can
calculate these intermediate costs from the `weight` function, which
we pass to our reduction. Just like the [weight-based parallel-for
loop](#weighted-parallel-for), the reduction operation calculates a
table containing the prefix sums of the weights of the items.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int max0(const parray<parray<int>>& xss) {
  parray<int> id = { std::numeric_limits<int>::lowest() };
  auto weight = [&] (const parray<int>& xs) {
    return xs.size();
  };
  auto combine = [&] (const parray<int>& xs1,
                      const parray<int>& xs2) {
    parray<int> r = { std::max(max(xs1), max(xs2)) };
    return r;
  };
  parray<int> a =
    reduce(xss.cbegin(), xss.cend(), id, weight, combine);
  return a[0];
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[max.hpp]: ../example/max.hpp
[max.cpp]: ../example/max.cpp

When the `max0` function returns, the result is just one number that
is our maximum value. It is therefore unfortunate that our combining
operator has to pay the cost to package the current maximum value in
the array `r`. The abstraction boundaries, in particular, the type of
the `reduce` function here leaves us no choice, however. Later, we are
going to see that, by generalizing our `reduce` function a little, we
can sidestep this issue.

The example codes shown in this section can be found in [max.hpp] and
[max.cpp].

### Basic scan

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Iter,
  class Item,
  class Combine
>
parray<Item> scan(Iter lo,
                  Iter hi,
                  Item id,
                  Combine combine,
                  scan_type st);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A *scan* is an operation that computes the running totals of a given
sequence of values of a specified type. Just like with a reduction, a
scan computes its running totals with respect to a given monoid. So,
accordingliy, the signature of our scan function shown above looks a
lot like the signature of our reduce function.

However, there are a couple differences. First, instead of returning a
single result value, the scan function returns an array (of length `hi
- lo`) of the running totals of the input sequence. Second, the
function takes the argument `st` to determine whether the scan is
inclusive or exclusive and whether the results are to be calculated
from the front to the back of the input sequence or from back to
front.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

using scan_type = enum {
  forward_inclusive_scan,
  forward_exclusive_scan,
  backward_inclusive_scan,
  backward_exclusive_scan
};

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before we define scan formally, let us first consider some example
programs. For our running example, we are going to pick the same
monoid that we used in our running example for the reduce function:
the set of 32-bit signed integers, along with the `std::max` combining
operator and the identity $-2^{32}+1$.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
auto combine = [&] (int x, int y) {
  return std::max(x, y);
};
int id = std::numeric_limits<int>::lowest();  // == -2^32+1 == -2147483648
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We are going to use the following array as the input sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
parray<int> xs = { 1, 3, 9, 0, 33, 1, 1 };
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following code prints the result of the forward-exclusive scan of
the above sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<int> fe =
  scan(xs.cbegin(), xs.cend(), id, combine, forward_exclusive_scan);
std::cout << "fe\t= " << fe << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output is shown below. Notice that the first value is the identity
element, `id` and that the running totals are computed starting from
the beginning to the end of the input sequence. In general, the first
value of the result of any exclusive scan is the identity
element. Moreover, the total of the entire input sequence is not
included in the result array.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
fe	= { -2147483648, 1, 3, 9, 9, 33, 33 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In general, the result of a forward-exclusive reduction is the
sequence $[ (\mathbf{I}), (\mathbf{I} \oplus xs_0), (\mathbf{I} \oplus
xs_0 \oplus xs_1), \ldots, (\mathbf{I} \oplus xs_0 \oplus \ldots
\oplus xs_{n-2}) ]$, where the input sequence is $xs = [ xs_0, \ldots,
xs_{n-1} ]$. Now, let us consider the forward-inclusive scan.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
parray<int> fi =
  scan(xs.cbegin(), xs.cend(), id, combine, forward_inclusive_scan);
std::cout << "fi\t= " << fi << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the forward-inclusive scan, the first value in the result array is
always the first value in the input sequence and the last value is the
total of the entire input sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
fi	= { 1, 3, 9, 9, 33, 33, 33 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The below example shows the backward-oriented versions of the scan
operator.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
parray<int> be =
  scan(xs.cbegin(), xs.cend(), id, combine, backward_exclusive_scan);
std::cout << "be\t= " << be << std::endl;

parray<int> bi =
  scan(xs.cbegin(), xs.cend(), id, combine, backward_inclusive_scan);
std::cout << "bi\t= " << bi << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Output:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
be	= { 33, 33, 33, 33, 1, 1, -2147483648 }
bi	= { 33, 33, 33, 33, 33, 1, 1 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The uses of the scan function that we have considered thus far use a
constant-time combining operator. For such applications, the work and
span complexity of an application is the same as the work and span
complexity of the corresponding reduce operation. In specific, our
applications of scan shown above take linear work and logarithmic span
in the size of the input sequence, and so does our `max` function,
which uses reduce in a similar way. It turns out that, in general,
given the same inputs (disragarding, of course, the scan-type
argument, which has no effect with respect to asymptotic running
time), reduce and scan have the same work and span complexity.

### Basic scan with a non-constant-time combining operator

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Iter,
  class Item,
  class Weight,
  class Combine
>
parray<Item> scan(Iter lo,
                  Iter hi,
                  Item id,
                  Weight weight,
                  Combine combine,
                  scan_type st)

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Just as was the case with reduce, our scan operator has to handle
non-constant-time associative combining operators. To this end, pctl
provides the above scan function, which now takes the corresponding
weight function. An application of this scan function looks just like
with the corresponding reduce function.

### Template parameters

+---------------------------------+-----------------------------------+
| Template parameter              | Description                       |
+=================================+===================================+
| [`Iter`](#r0-iter)              | Type of the iterator to be used to|
|                                 |access items in the input container|
+---------------------------------+-----------------------------------+
| [`Item`](#r0-i)                 | Type of the items in the input    |
|                                 | container                         |
+---------------------------------+-----------------------------------+
| [`Combine`](#r0-a)              | Associative combining operator    |
+---------------------------------+-----------------------------------+
| [`Weight`](#r0-w)               | Weight function (optional)        |
+---------------------------------+-----------------------------------+

Table: Template parameters for basic reduce and scan operations.

#### Item iterator {#r0-iter}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Iter;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An instance of this class must be an implementation of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator).

An iterator value of this type points to a value from the input stream
(i.e., a value of type `Item`).

#### Item {#r0-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the items to be processed by the reduction.

#### Associative combining operator {#r0-a}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Combine;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The combining operator is a C++ functor that takes two items and
returns a single item. The call operator for the `Combine` class
should have the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Item operator()(const Item& x, const Item& y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The behavior of the reduction is well defined only if the combining
operator is *associative*.

***Associativity.*** Let `f` be an object of type `Combine`. The
   operator `f` is associative if, for any `x`, `y`, and `z` that are
   values of type `Item`, the following equality holds:

`f(x, f(y, z)) == f(f(x, y), z)`

#### Weight function {#r0-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Weight;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The weight function is a C++ functor that takes a single item and
returns a non-negative "weight value" describing the size of the
item. The call operator for the weight function should have the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Complexity {#r0-complexity}

There are two cases to consider for any reduction $\mathtt{reduce}(lo,
hi, id, f)$ (or, correspondingly, any scan $\mathtt{scan}(lo, hi, id,
f, st)$ for any scan type $st$):

1. ***Constant-time associative combining operator.*** The amount of
work performed by the reduction is $O(hi-lo)$ and the span is $O(\log
(hi-lo))$.

2. ***Non-constant-time associative combining operator.*** We define
$\mathcal{R}$ to be the set of all function applications $f(x, y)$
that are performed in the reduction tree. Then,

    - The work performed by the reduction is $O(n + \sum_{f(x, y) \in
    \mathcal{R}(f, id, lo, hi)} W(f(x, y)))$.

    - The span of the reduction is $O(\log n \max_{f(x, y) \in
    \mathcal{R}(f, id, lo, hi)} S(f(x, y)))$.

Under certain conditions, we can use the following lemma to deduce a
more precise bound on the amount of work performed by the
reduction.

***Lemma (Work efficiency).*** For any associative combining operator
$f$ and weight function $w$, if for any $x$, $y$,

- $w(f(x, y)) \leq w(x) + w(y)$, and
- $W \leq c (w(x) + w(y))$, for some constant $c$,

where $W$ denotes the amount of work performed by the call $f(x, y)$,
then the amount of work performed by the reduction is $O(\log (hi-lo)
\sum_{lo \leq it < hi} (1 + w(*it)))$.

***Example: analysis of the complexity of a non-constant time
combining operator.*** For this example, let us analyze the `max0`
function, which is defined in a [previous
section](#reduction-nonconstant). We will begin by analyzing the
work. To start, we need to determine whether the combining operator of
the reduction over `xss` is constant-time or not. This combining
operator is not because the combining operator calls the `max`
function (twice, in fact). The first call is applied to the array `xs`
and the second to `ys`. The total work performed by these two calls is
linear in $| \mathtt{xs} | + | \mathtt{ys} |$. Therefore, by applying
the work-lemma shown above, we get that the total work performed by
this reduction is $O(\log | \mathtt{xss} | \max_{\mathtt{xs} \in
\mathtt{xss}} | xs ||)$. The span is simpler to analyze. By applying
our span rule for reduce, we get that the span for the reduction is
$O(\log |xss| \max_{\mathtt{xs} \in \mathtt{xss}} \log |xs|)$.

### Advanced reductions and scans

Although quite general already, the reduce and scan functions that we
have considered thus far are not always sufficiently general. For
example, the basic forms of reduce and scan require that the type of
the result value (or values in the case of scan) is the same as the
type of the values in the input sequence. As we saw in a [previous
section](#reduction-nonconstant), this requirement made the code of
the `max0` function look rather awkward and perhaps unnecessarily
inefficient. Fortunately, in pctl, such issues can be readily
addressed by using more advanced forms of reduce and scan.

In this section, we are going to examine in detail which problems (or,
more precisely, problem patterns) that can be solved more efficiently
and concisely by using more general forms of reduce and scan. Because
the number of possible variations of reduce and scan is quite large,
we are going to use a layered design, whereby, for each generalization
we introduce, we introduce one new abstraction layer on top of the
previous one. The following table summarizes the layers that we define
and what each successive layer adds on top of its predecessor
layer. The lowest level, namely level 0, is the level corresponding to
our basic reduce and scan functions from the beginning of this
section.

+-----------------------------------+-----------------------------------+
| Abstraction layer                 | Description                       |
+===================================+===================================+
| Level 0 (basic reduce and scan)   | Apply a specified monoid to       |
|                                   |combine the items of a range in    |
|                                   |memory that is specified by a pair |
|                                   |of iterator pointer values         |
+-----------------------------------+-----------------------------------+
| [Level 1](#red-l-1)               | Introduces a lift operator that   |
|                                   |allows the client to inline into   |
|                                   |the reduce a specified map         |
|                                   |operation                          |
+-----------------------------------+-----------------------------------+
| [Level 2](#red-l-2)               | Introduces an operator that       |
|                                   |provides a sequentialized          |
|                                   |alternative for the lift operator  |
|                                   |                                   |
+-----------------------------------+-----------------------------------+
| [Level 3](#red-l-3)               | Introduces a "mergeable output"   |
|                                   |type that enables                  |
|                                   |destination-passing style reduction|
+-----------------------------------+-----------------------------------+
| [Level 4](#red-l-4)               | Introduces a "splittlable input"  |
|                                   |type that replaces the iterator    |
|                                   |pointer values                     |
+-----------------------------------+-----------------------------------+

Table: Abstraction layers for reduce and scan that are provided by pctl.

#### Level 1 {#red-l-1}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level1 {

template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              Combine combine,
              Lift lift);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              Combine combine,
              Lift_comp lift_comp,
              Lift lift);


template <
  class Iter,
  class Result,
  class Combine,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    Combine combine,
                    Lift lift,
                    scan_type st);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp,
  class Lift
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    Combine combine,
                    Lift_comp lift_comp,
                    Lift lift,
                    scan_type st);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To motivate level 1, let us return to our running example and, in
particular, the `max0` function from a [previous
section](#reduction-nonconstant). When we examine `max0`, we see that
our identity value is an array, our combine operator takes two arrays
and returns an array, and our result value is itself an array. In
other words, the monoid that we use for `max0` is one where the input
sequence is the set of all arrays of numbers, and the identity and
combining function follow accordingly. As such, to get the final
result value, at the end of the function, we have to extract the first
value from the result array. Why do we need to use these intermediate
arrays? The only reason is that the basic reduce function requires
that the type of the input value (an array of numbers) be the same as
the type of the result value (an array of numbers).

The level-1 reduce function allows us to bypass this limitation,
provided that we define and pass to the level-1 reduce function a
*lift* function of our choice. To see what it looks like, let us
consider how we solve the same problem as before, but this time using
level-1 reduce. In the `max1` function below, we see first that our
identity element and associative combining operator are specified for
base values (i.e., `int`s) rather than arrays of `int`s.  Furthermore,
we see that our lift function is a function that takes (a reference
to) an array and returns the maximum value of the given array, using
the same `max` function that we defined in a [previous
section](#reduce-basic). Now, it should be clear what is the purpose
of the lift function: it describes how to solve the same problem, but
specifically for an individual item in the input.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int max1(const parray<parray<int>>& xss) {
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  auto lift_comp = [&] (const parray<int>& xs) {
    return xs.size();
  };
  auto lift = [&] (const parray<int>& xs) {
    return max(xs);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp, lift);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As usual, because our lift function is not a constant-time function,
we need to report the cost of the lift function. To this end, in the
code above, we use the `lift_comp` function which returns the cost of
the corresponding application of the `lift` function. The cost that we
defined is, in particular, equal to the size of the array, since the
`max` function takes linear time.

In general, our level 1 reduce (and scan) now recognize two types of
values:

1. the type `Item`, which is the type of the items stored in the input
sequence (i.e., the type of values pointed at by an iterator of type
`Iter`)

2. the type `Result`, which is the type of the result value (or values
in the case of scan) that are returned by the reduce (or scan).

In terms of types, the reduce function converts a value of type `Item`
to a corresponding value of type `Result`. In the example above, our
`lift` function converts from a value of type `Item = parray<int>` to
a value of type `int`. 

##### Index-passing reduce and scan

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level1 {

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx
>
Result reducei(Iter lo,
               Iter hi,
               Result id,
               Combine combine,
               Lift_idx lift_idx);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
Result reducei(Iter lo,
               Iter hi,
               Result id,
               Combine combine,
               Lift_comp_idx lift_comp_idx,
               Lift_idx lift_idx);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_idx
>
parray<Result> scani(Iter lo,
                     Iter hi,
                     Result id,
                     Combine combine,
                     Lift_idx lift_idx,
                     scan_type st);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_idx,
  class Lift_idx
>
parray<Result> scani(Iter lo,
                     Iter hi,
                     Result id,
                     Combine combine,
                     Lift_comp_idx lift_comp_idx,
                     Lift_idx lift_idx,
                     scan_type st);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes, it is useful for reduce and scan to pass to their lift
function an extra argument: the corresponding position of the item in
the input sequence. For this reason, pctl provides for each of the
level 1 functions a corresponding *index-passing* version. For
instance, the `reducei` function below now takes a `lift_idx` function
instead of the usual a `lift` function.  This new, index-passing
version of the lift function itself takes an additional position
argument: for item $xs_i$, the lift function is now applied by
`reducei` to each element as before, but along with the position of
the item (i.e., $\mathtt{lift\_idx}(i, xs_i)$).

##### Template parameters

+----------------------------------+-----------------------------------+
| Template parameter               | Description                       |
+==================================+===================================+
| [`Result`](#r1-r)                | Type of the result value to be    |
|                                  |returned by the reduction          |
+----------------------------------+-----------------------------------+
| [`Lift`](#r1-l)                  | Lifting operator                  |
+----------------------------------+-----------------------------------+
| [`Lift_idx`](#r1-li)             | Index-passing lifting operator    |
+----------------------------------+-----------------------------------+
| [`Combine`](#r1-comb)            | Associative combining operator    |
+----------------------------------+-----------------------------------+
| [`Lift_comp`](#r1-l-c)           | Complexity function associated    |
|                                  |with the lift funciton             |
+----------------------------------+-----------------------------------+
| [`Lift_comp_idx`](#r1-l-c-i)     | Index-passing lift complexity     |
|                                  |function                           |
+----------------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 1.

                 
###### Result {#r1-r}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Result               
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the result value to be returned by the reduction.

This class must provide a default (i.e., zero-arity) constructor.

###### Lift {#r1-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift operator is a C++ functor that takes an iterator and returns
a value of type `Result`. The call operator for the `Lift` class
should have the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Index-passing lift {#r1-li}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_idx;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift operator is a C++ functor that takes an index and a
corresponding iterator and returns a value of type `Result`. The call
operator for the `Lift` class should have the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(long pos, const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The value passed in the `pos` parameter is the index corresponding to
the position of item `x`.

###### Associative combining operator {#r1-comb}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Combine;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now, the type of our associative combining operator has changed from
what it is in level 0. In particular, the values that are being passed
and returned are values of type `Result`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(const Result& x, const Result& y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

###### Complexity function for lift {#r1-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_comp;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift-complexity function is a C++ functor that takes a reference
on an item and returns a non-negative number of type `long`. The
`Lift_comp` class should provide a call operator of the following
type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

###### Index-passing lift-complexity function {#r1-l-c-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_comp_idx;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift-complexity function is a C++ functor that takes an index and
an reference on an item and returns a non-negative number of type
`long`. The `Lift_comp_idx` class should provide a call operator of
the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(long pos, const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Complexity
 
Let us now consider the work and span complexity of a level-1
reduction $\mathtt{reduce}(lo, hi, id, f, l)$ (or, correspondingly,
any scan $\mathtt{scan}(lo, hi, id, f, l, st)$ for any scan type
$st$).  Compared to the previous level, in level 1, we now need to
account for the costs associated with applications of the lift
operator. We define $\mathcal{L}$ to be the set of all function
applications of $l(a)$ that are performed in the leaves of the level-1
reduction tree and $\mathcal{R}$ to be the set of all function
applications $f(x, y)$ in the interior nodes of the level-1 reduction
tree. There are two cases with respect to the associative combining
operator $f$:

1. ***Constant-time associative combining operator.*** There are two
cases with respect to the lifting operator $l$.

    a. ***Constant-time lift operator.*** The amount of work performed
    by the reduction is $O(hi - lo)$ and the span is $O(\log(hi -
    lo))$.

    b. ***Non-constant-time lift operator.*** The total amount of
    work performed by the reduction is $O((hi - lo) + \sum_{l(a) \in
    \mathcal{L}(lo, hi, id, f, l)} W(l(a)))$ and the span is
    $O(\log(hi - lo)+ \max_{l(a) \in \mathcal{L}(lo, hi, id, f, l)}
    W(l(a)))$.

2. ***Non-constant-time associative combining operator.*** Then,

    - The work performed by the level-1 reduction is $O(n +
    \sum_{f(x,  y) \in \mathcal{R}(lo, hi, id, f, l)} W(f(x, y)) +
    \sum_{l(a) \in  \mathcal{L}(lo, hi, id, f, l)} W(l(a)))$.

    - The span of the reduction is $O(\log n \max_{f(x, y) \in
    \mathcal{R}(lo, hi, id, f, l)} S(f(x, y)) + \max_{l(a) \in
    \mathcal{L}(lo, hi, id, f, l)} S(l(a)))$.

Under certain conditions, we can use the following lemma to deduce a
more precise bound on the amount of work performed by the level-1
reduction.

***Lemma (Work efficiency).*** For any associative combining operator
$f$ and weight function $w$, if for any $a$

- $w(l(a)) \leq w(a)$, and
- $W_l \leq c (\log w(a) \cdot w(a))$, for some constant $c$.

where $W_l$ denotes the amount of work performed by the call $l(a)$,
and for any $x$, $y$,

- $w(f(x, y)) \leq w(x) + w(y)$, and
- $W_f \leq c (w(x) + w(y))$, for some constant $c$,

where $W_f$ denotes the amount of work performed by the call $f(x, y)$,
then the amount of work performed by the reduction is $O(\log (hi-lo)
\sum_{lo \leq it < hi} (1 + w(*it)))$.

#### Level 2 {#red-l-2}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level2 {

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_reduce_rng
>
Result reduce(Iter lo,
              Iter hi,
              Result id,
              Combine combine,
              Lift_comp_rng lift_comp_rng,
              Lift_idx lift_idx,
              Seq_reduce_rng seq_reduce_rng);

template <
  class Iter,
  class Result,
  class Combine,
  class Lift_comp_rng,
  class Lift_idx,
  class Seq_reduce_rng
>
parray<Result> scan(Iter lo,
                    Iter hi,
                    Result id,
                    Combine combine,
                    Lift_comp_rng lift_comp_rng,
                    Lift_idx lift_idx,
                    Seq_reduce_rng seq_reduce_rng,
                    scan_type st);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the [section on parallel for loops](#pfor-sequential-alt), we saw
that we can combine two algorithms, one parallel and one sequential,
to make a hybrid algorithm that combines the best features of both
algorithms. We rely on the underlying granularity-control algorithm of
pctl to switch intelligently between the parallel and sequential
algorithms as the program runs.

Now, we are going to see how we can apply a similar technique to
reduction. Let us return to the problem of finding the maximum value
in an array of arrays of numbers. The solution we are going to
consider is the one given by the `max2` function below.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int max2(const parray<parray<int>>& xss) {
  using const_iterator = typename parray<parray<int>>::const_iterator;
  const_iterator lo = xss.cbegin();
  const_iterator hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  parray<long> w = weights(xss.size(), [&] (const parray<int>& xs) {
    return xs.size();
  });
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  const_iterator b = lo;
  auto lift_comp_rng = [&] (const_iterator lo, const_iterator hi) {
    return w[hi - b] - w[lo - b];
  };
  auto lift_idx = [&] (int, const parray<int>& xs) {
    return max(xs);
  };
  auto seq_reduce_rng = [&] (const_iterator lo, const_iterator hi) {
    int m = id;
    for (const_iterator i = lo; i != hi; i++) {
      for (auto j = i->cbegin(); j != i->cend(); j++) {
        m = std::max(m, *j);
      }
    }
    return m;
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng,
                                    lift_idx, seq_reduce_rng);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Compared to our `max1` function, we can see two differences:

1. The complexity function for lift, namely `lift_comp_rng`,
calculates the cost for a given range of input values rather than for
a given individual value. The cost is precomputed by the `weights`
function, which is described in a [previous section](#pfor-weights).

2. The last argument to the reduce function is the sequential
alternative body, namely `seq_reduce_rng`.

To summarize, our level-1 solution, namely `max1`, had a parallel
solution that was expressed by nested instances of our reduce
functions. In this section, we saw how to use the level-2 reduce
function to combine parallel and sequential solutions to make a hybrid
solution.

##### Template parameters

+----------------------------+-----------------------------------+
| Template parameter         | Description                       |
+============================+===================================+
| [`Seq_reduce_rng`](#r2-l)  | Sequential alternative body for   |
|                            |the reduce operation               |
+----------------------------+-----------------------------------+
| [`Lift_comp_rng`](#r2-w)   | Range-based lift complexity       |
|                            |function                           |
+----------------------------+-----------------------------------+
|[`Seq_scan_rng_dst`](#r2-ss)| Sequential alternative body for   |
|                            |the scan operation                 |
+----------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 2.


###### Sequential alternative body for the lifting operator {#r2-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_reduce_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential-reduce function is a C++ functor that takes a pair of
iterators and returns a result value. The `Seq_reduce_rng` class
should provide a call operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(Iter lo, Iter hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

###### Range-based lift-complexity function {#r2-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_comp_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The range-based lift-complexity function is a C++ functor that takes a
pair of iterators and returns a non-negative number. The value
returned is a value to account for the amount of work to be performed
to apply the lift function to the items in the right-open range `[lo,
hi)` of the input sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(Iter lo, Iter hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

###### Sequential alternative body for the scan operation {#r2-ss}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_scan_rng_dst;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential-scan function is a C++ functor that takes a pair of
iterators, namely `lo` and `hi`, and writes its result to a range in
memory that is pointed to by `dst_lo`. The `Seq_scan_rng_dst` class should
provide a call operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(Iter lo, Iter hi, typename parray<Result>::iterator dst_lo);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Level 3 {#red-l-3}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level3 {

template <
  class Input_iter,
  class Output,
  class Result,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_reduce_rng_dst
>
void reduce(Input_iter lo,
            Input_iter hi,
            Output out,
            Result id,
            Result& dst,
            Lift_comp_rng lift_comp_rng,
            Lift_idx_dst lift_idx_dst,
            Seq_reduce_rng_dst seq_reduce_rng_dst);

template <
  class Input_iter,
  class Output,
  class Result,
  class Output_iter,
  class Lift_comp_rng,
  class Lift_idx_dst,
  class Seq_scan_rng_dst
>
void scan(Input_iter lo,
          Input_iter hi,
          Output out,
          Result& id,
          Output_iter outs_lo,
          Lift_comp_rng lift_comp_rng,
          Lift_idx_dst lift_idx_dst,
          Seq_scan_rng_dst seq_scan_rng_dst,
          scan_type st);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this section, we are going to introduce the
destination-passing-style interface for reduction and scan. Relative
to the previous one, this level introduces one new concept. An *output
descriptor* is a class that describes how objects of type `Result` are
to be initialized, combined, and copied. Let us see how to use an
output descriptor by continuing with our running example.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
int max3(const parray<parray<int>>& xss) {
  using const_iterator = typename parray<parray<int>>::const_iterator;
  const_iterator lo = xss.cbegin();
  const_iterator hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  parray<long> w = weights(xss.size(), [&] (const parray<int>& xs) {
    return xs.size();
  });
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  using output_type = level3::cell_output<int, decltype(combine)>;
  output_type out(id, combine);
  const_iterator b = lo;
  auto lift_comp_rng = [&] (const_iterator lo, const_iterator hi) {
    return w[hi - b] - w[lo - b];
  };
  auto lift_idx_dst = [&] (int, const parray<int>& xs, int& result) {
    result = max(xs);
  };
  auto seq_reduce_rng = [&] (const_iterator lo, const_iterator hi, int& result) {
    int m = id;
    for (const_iterator i = lo; i != hi; i++) {
      for (auto j = i->cbegin(); j != i->cend(); j++) {
        m = std::max(m, *j);
      }
    }
    result = m;
  };
  int result;
  level3::reduce(lo, hi, out, id, result, lift_comp_rng,
                 lift_idx_dst, seq_reduce_rng);
  return result;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The only change in `max3` relative to `max2` is the use of the output
descriptor `cell_output`. This output descriptor is the simplest
output descriptor: all it does is apply the given combining operator
directly to its arguments, writing the result into a destination
cell. The implementation of the cell output is shown below.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level3 {

template <class Result, class Combine>
class cell_output {
public:
  
  using result_type = Result;
  using array_type = parray<result_type>;
  using const_iterator = typename array_type::const_iterator;
  
  result_type id;
  Combine combine;
  
  cell_output(result_type id, Combine combine)
  : id(id), combine(combine) { }
  
  cell_output(const cell_output& other)
  : id(other.id), combine(other.combine) { }
  
  void init(result_type& dst) const {
    dst = id;
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(const result_type& src, result_type& dst) const {
    dst = combine(dst, src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      dst = combine(*it, dst);
    }
  }
  
};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The constructor takes the identity element and associative combining
operator and stores them in member fields. The `init` method writes
the identity element into the given destination cell. The `copy`
method simply copies the value in `src` to the `dst` cell. There are
two `merge` methods: the first one applies the combining operator,
leaving the result in the `dst` cell. The second uses the combining
operator to accumulate a result value from a given range of input
items, leaving the result in the `dst` cell.

Because it assumes that objects of type `Result` can be copied
efficiently, the cell output descriptor does not demonstrate the
benefit of the output-descriptor mechanism. However, the benefit is
clear when the `Result` objects are container objects, such as
`pchunkedseq` objects, which require linear work to be copied.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
template <class Chunked_sequence>
class chunkedseq_output {
public:
  
  using result_type = Chunked_sequence;
  using const_iterator = const result_type*;
  
  result_type id;
  
  chunkedseq_output() { }
  
  void init(result_type& dst) const {
    
  }
  
  void copy(const result_type& src, result_type& dst) const {
    dst = src;
  }
  
  void merge(result_type& src, result_type& dst) const {
    dst.concat(src);
  }
  
  void merge(const_iterator lo, const_iterator hi, result_type& dst) const {
    dst = id;
    for (const_iterator it = lo; it != hi; it++) {
      merge(*it, dst);
    }
  }
  
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first `merge` method concatenates the `src` and `dst` containers,
leaving the result in `dst` and taking logarithmic work to
complete. As such, the chunkedseq output descriptor can merge results
taking logarithmic work in the size of the chunked sequence, thereby
avoiding costly linear-work copy merges that would be imposed if we
were to instead use the cell output descriptor for the same task. The
second `merge` method simply combines all items in the range using the
first `merge` method.

##### Template parameters

+------------------------------------+--------------------------------+
| Template parameter                 | Description                    |
+====================================+================================+
| [`Input_iter`](#r3-iit)            | Type of an iterator for input  |
|                                    |values                          |
+------------------------------------+--------------------------------+
| [`Output_iter`](#r3-oit)           | Type of an iterator for output |
|                                    |values                          |
+------------------------------------+--------------------------------+
| [`Output`](#r3-o)                  | Type of the object to manage   |
|                                    |the output of the reduction     |
+------------------------------------+--------------------------------+
| [`Lift_idx_dst`](#r3-dpl)          | Lift function in               |
|                                    |destination-passing style       |
+------------------------------------+--------------------------------+
| [`Seq_reduce_rng_dst`](#r3-dpl-seq)| Sequential reduce function in  |
|                                    |destination-passing style       |
+------------------------------------+--------------------------------+

Table: Template parameters that are introduced in level 3.

###### Input iterator {#r3-iit}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_iter;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An instance of this class must be an implementation of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator).

An iterator value of this type points to a value from the input
stream.

###### Output iterator {#r3-oit}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output_iter;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An instance of this class must be an implementation of the
[random-access
iterator](http://en.cppreference.com/w/cpp/concept/RandomAccessIterator).

An iterator value of this type points to a value from the output
stream.
                                      
###### Output {#r3-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the object to receive the output of the reduction.


+-----------------------------+-------------------------------------+
| Constructor                 | Description                         |
+=============================+=====================================+
| [copy constructor](#ro-c-c) | Copy constructor                    |
+-----------------------------+-------------------------------------+

Table: Constructors that are required for the `Output` class.

+-------------------------+-------------------------------------+
| Public method           | Description                         |
+=========================+=====================================+
| [`init`](#ro-i)         | Initialize given result object      |
+-------------------------+-------------------------------------+
| [`copy`](#ro-cop)       | Copy the contents of a specified    |
|                         |object to a specified cell           |
+-------------------------+-------------------------------------+
| [`merge`](#ro-m)        | Merge result objects                |
+-------------------------+-------------------------------------+

Table: Public methods that are required for the `Output` class.

###### Copy constructor {#ro-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Output(const Output& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy constructor.

###### Result initializer {#ro-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void init(Result& dst) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the result object referenced by `dst`.

###### Copy {#ro-cop}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void copy(const Result& src, Result& dst) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy the contents of `src` to `dst`.

###### Merge {#ro-m}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void merge(Result& src, Result& dst) const;                    // (1)
void merge(Output_iter lo, Output_iter hi, Result& dst) const; // (2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

(1) Merge the contents of `src` and `dst`, leaving the result in
`dst`.

(2) Merge the contents of the cells in the right-open range `[lo,
hi)`, leaving the result in `dst`.

###### Destination-passing-style lift {#r3-dpl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_idx_dst;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The destination-passing-style lift function is a C++ functor that
takes an index, an iterator, and a reference on result object. The
call operator for the `Lift_idx_dst` class should have the following
type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(long pos, const Item& xs, Result& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The value that is passed in for `pos` is the index in the input
sequence of the item `x`. The object referenced by `dst` is the object
to receive the result of the lift function.

###### Destination-passing-style sequential lift {#r3-dpl-seq}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_reduce_rng_dst;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The destination-passing-style sequential lift function is a C++
functor that takes a pair of iterators and a reference on an output
object. The call operator for the `Seq_reduce_dst` class should have the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input_iter lo, Input_iter hi, Result& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The purpose of this function is provide an alternative sequential
algorithm that is to be used to process ranges of items from the
input. The range is specified by the right-open range `[lo, hi)`. The
object referenced by `dst` is the object to receive the result of the
sequential lift function.

#### Level 4 {#red-l-4}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level4 {

template <
  class Input,
  class Output,
  class Result,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Seq_convert_reduce
>
void reduce(Input& in,
            Output out,
            Result id,
            Result& dst,
            Convert_reduce_comp convert_comp,
            Convert_reduce convert,
            Seq_convert_reduce seq_convert);

template <
  class Input,
  class Output,
  class Result,
  class Output_iter,
  class Merge_comp,
  class Convert_reduce_comp,
  class Convert_reduce,
  class Convert_scan,
  class Seq_convert_scan
>
void scan(Input& in,
          Output out,
          Result id,
          Output_iter outs_lo,
          Merge_comp merge_comp,
          Convert_reduce_comp convert_reduce_comp,
          Convert_reduce convert_reduce,
          Convert_scan convert_scan,
          Seq_convert_scan seq_convert_scan,
          scan_type st);
            
} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the last level of our scan and reduce operators, we are going to
introduce one more concept. At this level, reduction and scan no
longer take as input a pair of iterator values. Instead, the input is
described in an even more generic form. An *input descriptor* is an
object that describes a process for dividing an input container into
two or more pieces. The following input descriptor is the one we use
to represent a range encoded by a pair or random-access iterator
values.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level4 {

template <class Input_iter>
class random_access_iterator_input {
public:
  
  using self_type = random_access_iterator_input;
  using array_type = parray<self_type>;
  
  Input_iter lo;
  Input_iter hi;
  
  random_access_iterator_input() { }
  
  random_access_iterator_input(Input_iter lo, Input_iter hi)
  : lo(lo), hi(hi) { }
  
  bool can_split() const {
    return size() >= 2;
  }

  long size() const {
    return hi - lo;
  }
  
  void split(random_access_iterator_input& dst) {
    dst = *this;
    long n = size();
    assert(n >= 2);
    Input_iter mid = lo + (n / 2);
    hi = mid;
    dst.lo = mid;
  }
  
  array_type split(long) {
    array_type tmp;
    return tmp;
  }
  
  self_type slice(const array_type&, long _lo, long _hi) {
    self_type tmp(lo + _lo, lo + _hi);
    return tmp;
  }
    
};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As usual, the pair (`lo`, `hi`) represents the right-open index [`lo`,
`hi`). The `can_split` method returns `true` when the range contains
at least two elements and `false` otherwise. The `size` method returns
the number of items in the range. The `split` method divides the range
in two subranges of approximately the same size, leaving the result in
`dst`. The `slice` method creates a subrange starting at `lo + _lo`
and ending at `lo + _hi`.

Now, we are going to see one feature that we have at level 4 that we
lacked in previous levels. Because we can provide a custom input
descriptor, we can provide one that destroys its input as the
reduction operation proceeds. It turns out that this feature is just
what we need to encode a useful algorithmic pattern: a pattern that
updates a chunked sequence container in place and all in
parallel. First, let us see the input descriptor, and then we will see
it put to work. The chunkedseq input descriptor removes items from a
chunked sequence structure as the reduction repeatedly divides the
input.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
template <class Chunkedseq>
class chunkedseq_input {
public:
  
  using self_type = chunkedseq_input<Chunkedseq>;
  using array_type = parray<self_type>;
  
  Chunkedseq seq;
  
  chunkedseq_input(Chunkedseq& _seq) {
    _seq.swap(seq);
  }
  
  chunkedseq_input(const chunkedseq_input& other) { }
  
  bool can_split() const {
    return seq.size() >= 2;
  }
  
  void split(chunkedseq_input& dst) {
    long n = seq.size() / 2;
    seq.split(seq.begin() + n, dst.seq);
  }
  
  array_type split(long) {
    array_type tmp;
    assert(false);
    return tmp;
  }
  
  self_type slice(const array_type&, long _lo, long _hi) {
    self_type tmp;
    assert(false);
    return tmp;
  }
  
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let us see this input descriptor at work. Recall that our
`pchunkedseq` class provides a `keep_if` method, which removes items
from the container according to a given predicate function. The
following example shows how we can remove all odd values from a
chunked sequence of `int`s.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq<int> xs = { 3, 10, 35, 2, 2, 100 };
xs.keep_if([&] (int x) { return x%2 == 0; });
std::cout << "xs = " << xs << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The output is the following. All the odd values were deleted from the
container.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
{ 10, 2, 2, 100 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The body of this method simply calls to a generic function in the
`chunked` module, passing references to the chunked sequence object,
`seq`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
template <class Pred>
void keep_if(const Pred& p) {
  chunked::keep_if(p, seq, seq);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The body of this function is shown below. Here, we see that, before
the reduction, we feed the input sequence `xs` to our input
descriptor, `in`. After the `reduce` operation completes, `xs` is
going to be empty. All the items that survived the filtering process
go into the `dst` chunked sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace chunked {

template <class Pred, class Chunkedseq>
void keep_if(const Pred& p, Chunkedseq& xs, Chunkedseq& dst) {
  using input_type = level4::chunkedseq_input<Chunkedseq>;
  using output_type = level3::chunkedseq_output<Chunkedseq>;
  using value_type = typename Chunkedseq::value_type;
  input_type in(xs);
  output_type out;
  Chunkedseq id;
  auto convert_reduce_comp = [&] (input_type& in) {
    return in.seq.size();
  };
  auto convert_reduce = [&] (input_type& in, Chunkedseq& dst) {
    while (! in.seq.empty()) {
      value_type v = in.seq.pop_back();
      if (p(v)) {
        dst.push_front(v);
      }
    }
  };
  auto seq_convert_reduce = convert_reduce;
  level4::reduce(in, out, id, dst, convert_reduce_comp,
                 convert_reduce, seq_convert_reduce);
}

}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To summarize, we saw that input descriptors are general enough to
describe read-only inputs as well as inputs that are to be consumed
destructively. The `keep_if` example shows the usefulness of the
latter input type. Using `keep_if` we can process our input sequence
destructively and in parallel. As a bonus, during the processing,
chunks can be reused on the fly by the memory allocator to construct
the output chunked sequence, thanks to the property that chunks are
popped off the input sequence as the `reduce` is working.

##### Template parameters

+---------------------------------+-----------------------------------+
| Template parameter              | Description                       |
+=================================+===================================+
| [`Input`](#r4-i)                | Type of input to the reduction    |
+---------------------------------+-----------------------------------+
| [`Convert_reduce`](#r4-c)       | Function to convert the items of a|
|                                 |given input and then produce a     |
|                                 |specified reduction on the         |
|                                 |converted items                    |
+---------------------------------+-----------------------------------+
| [`Convert_scan`](#r4-sca)       | Function to convert the items of a|
|                                 |given input and then produce a     |
|                                 |specified scan on the converted    |
|                                 |items                              |
+---------------------------------+-----------------------------------+
| [`Seq_convert_scan`](#r4-ssca)  | Alternative sequentialized version|
|                                 |of the `Convert_scan` function     |
|                                 |                                   |
+---------------------------------+-----------------------------------+
| [`Convert_reduce_comp`](#r4-i-w)|  Complexity function associated   |
|                                 |with a convert function            |
|                                 |                                   |
+---------------------------------+-----------------------------------+
| [`Seq_convert_reduce`](#r4-s-c) | Alternative sequentialized version|
|                                 |of the `Convert_reduce` function   |
+---------------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 4.

###### Input {#r4-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------+-------------------------------------------+
| Constructor                 | Description                               |
+=============================+===========================================+
| [copy constructor](#r4i-cc) | Copy constructor                          |
+-----------------------------+-------------------------------------------+

Table: Constructors that are required for the `Input` class.

+-----------------------------+-------------------------------------------+
| Public method               | Description                               |
+=============================+===========================================+
| [`can_split`](#r4i-c-s)     | Return value to indicate whether split is |
|                             |possible                                   |
+-----------------------------+-------------------------------------------+
| [`size`](#r4i-sz)           | Return the size of the input              |
+-----------------------------+-------------------------------------------+
| [`slice`](#r4i-slc)         | Return a specified slice of the input     |
+-----------------------------+-------------------------------------------+
| [`split`](#r4i-sp)          | Divide the input into two pieces          |
+-----------------------------+-------------------------------------------+

Table: Public methods that are required for the `Input` class.

###### Copy constructor {#r4i-cc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Input(const Input& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy constructor.

###### Can split {#r4i-c-s}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool can_split() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a boolean value to indicate whether a split is possible.

###### Size {#r4i-sz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the input.

###### Slice {#r4i-slc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Input slice(parray<Input>& ins, long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a slice of the input that occurs logically in the right-open
range `[lo, hi)`, optionally using `ins`, the results of a precomputed
application of the `split` function.

###### Split {#r4i-sp}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void split(Input& dst);          // (1)
parray<Input> split(long n));    // (2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

(1) Transfer a fraction of the contents of the current input object to
the input object referenced by `dst`.

The behavior of this method may be undefined when the `can_split`
function would return `false`.

(2) Divide the contents of the current input object into at most `n`
pieces, returning an array which stores the new pieces.


##### Convert-reduce complexity function {#r4-i-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Convert_reduce_comp;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The convert-complexity function is a C++ functor which returns a
positive number that associates a weight value to a given input
object. The `Convert_reduce_comp` class should provide the following
call operator.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Input& in);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Convert-reduce {#r4-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Convert_reduce;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The convert function is a C++ functor which takes a reference on an
input value and computes a result value, leaving the result value in
an output cell. The `Convert_reduce` class should provide a call
operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Result& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Sequential convert-reduce {#r4-s-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_convert_reduce;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function is a C++ functor whose purpose is to
substitute for the ordinary convert function when input size is small
enough to sequentialize. The `Seq_convert_reduce` class should provide
a call operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Result& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function should always compute the same result
as the ordinary convert function given the same input. 

##### Convert-scan {#r4-sca}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Convert_scan;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The convert function is a C++ functor which takes a reference on an
input value and computes a result value, leaving the result value in
an output cell. The `Convert_scan` class should provide a call
operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Output_iter outs_lo);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Sequential convert-scan {#r4-ssca}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_convert_scan;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function is a C++ functor whose purpose is to
substitute for the ordinary convert function when input size is small
enough to sequentialize. The `Seq_convert_scan` class should provide
a call operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Output_iter outs_lo);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function should always compute the same result
as the ordinary convert function given the same input. 

Derived operations
------------------

***Type-level operators*** Sometimes, as we will see in this section,
the pctl defines a function that both takes as template parameter an
iterator class and extracts from that iterator class the type of the
items that are referenced by the iterator.  For this purpose, the pctl
defines a few type-level functions whose purpose is to extract from
the iterator the corresponding value, reference and pointer types.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter>
using value_type_of = typename std::iterator_traits<Iter>::value_type;

template <class Iter>
using reference_of = typename std::iterator_traits<Iter>::reference;

template <class Iter>
using pointer_of = typename std::iterator_traits<Iter>::pointer;

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


### Pack

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Item_iter, class Flags_iter>
parray<value_type_of<Item_iter>> pack(Item_iter lo,
                                      Item_iter hi,
                                      Flags_iter flags_lo);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


### Filter

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter, class Pred_idx>
parray<value_type_of<Iter>> filteri(Iter lo, Iter hi, Pred_idx pred_idx);

template <class Iter, class Pred>
parray<value_type_of<Iter>> filter(Iter lo, Iter hi, Pred pred);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Predicate

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Pred;     // (1)
class Pred_idx; // (2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool operator()(long pos, const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Max index

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Iter,
  class Item,
  class Compare,
  class Lift
>
long max_index(Iter lo, Iter hi, Item id, Compare compare, Lift lift);

template <
  class Iter,
  class Item,
  class Compare
>
long max_index(Iter lo, Iter hi, Item id, Compare compare);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Lift function

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Item operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Comparison function

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Compare;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool operator()(Item x, Item y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In-place operations
-------------------

Here, document the functions which are exported by `dpsdatapar.hpp`.

Merging and sorting
===================

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Compare;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool operator()(const Item& x, const Item& y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Weight;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Weight_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Item& lo, const Item* hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Comparison-based merge
----------------------

### Iterator based

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <
  class Input_iter,
  class Output_iter,
  class Compare
>
void merge(Input_iter first1, Input_iter last1,
           Input_iter first2, Input_iter last2,
           Output_iter d_first, Compare compare);

template <
  class Input_iter,
  class Output_iter,
  class Weight,
  class Compare
>
void merge(Input_iter first1, Input_iter last1,
           Input_iter first2, Input_iter last2,
           Output_iter d_first, Weight weight,
           Compare compare);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <
  class Input_iter,
  class Output_iter,
  class Weight_rng,
  class Compare
>
void merge(Input_iter first1, Input_iter last1,
           Input_iter first2, Input_iter last2,
           Output_iter d_first, Weight_rng weight_rng,
           Compare compare);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Parallel chunked sequence

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace chunked {

template <class Item, class Compare>
pchunkedseq<Item> merge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys,
                        Compare compare);

template <class Item, class Weight, class Compare>
pchunkedseq<Item> merge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys,
                        Weight weight, Compare compare);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <class Item, class Weight_rng, class Compare>
pchunkedseq<Item> pcmerge(pchunkedseq<Item>& xs, pchunkedseq<Item>& ys,
                          Weight_rng weight_rng, Compare compare);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Comparison-based sort
---------------------

### Iterator based

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter, class Compare>
void sort(Iter lo, Iter hi, Compare compare);

template <class Iter, class Weight, class Compare>
void sort(Iter lo, Iter hi, Weight weight, Compare compare);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <class Iter, class Weight_rng, class Compare>
void sort(Iter lo, Iter hi, Weight_rng weight_rng, Compare compare);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Parallel chunked sequence

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace pchunked {

template <class Item, class Compare>
pchunkedseq<Item> sort(pchunkedseq<Item>& xs, Compare compare);

template <class Item, class Weight, class Compare>
pchunkedseq<Item> sort(pchunkedseq<Item>& xs,
                       Weight weight,
                       Compare compare);


} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace pchunked {
namespace range {

template <class Item, class Weight_rng, class Compare>
pchunkedseq<Item> sort(pchunkedseq<Item>& xs,
                       Weight_rng weight_rng,
                       Compare compare);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Integer sort
------------

### Iterator based

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter>
void integersort(Iter lo, Iter hi);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Parallel chunked sequence

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace pchunked {

template <class Integer>
pchunkedseq<Integer> integersort(pchunekdseq<Integer>& xs);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
