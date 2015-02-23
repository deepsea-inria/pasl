% The Parallel-Container Template Library User's Guide
% Deepsea Project
% 10 February 2015

Introduction
============

The purpose of this document is to serve as a working draft of the
design and implementation of the Parallel Container Template Library
(PCTL).

***Essential terminology.***

A *function call operator* (or, just "call operator") is a member
function of a C++ class that is specified by the name `operator()`.

A *functor* is a C++ class which defines a call operator.

A *right-open range* is ...

*work* *span* 

Containers
==========

Sequence containers
-------------------

Class name                           | Description
-------------------------------------|---------------------------------
[`array`](#parray)                   | Array class
[`pchunkedseq`](#pchunkedseq)        | Parallel chunked sequence class

Table: Sequence containers that are provided by pctl.

Associative containers
----------------------

Class name          | Description
--------------------|-------------------------
set                 | Set class
map                 | Associative map class

Table: Associative containers that are provided by pctl.

Parallel array {#parray}
==============

Interface and cost model
------------------------

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
                                                           
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <class Item, class Alloc = std::allocator<Item>>
class parray;

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

Table: Parallel-array type definitions.

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

### Template parameters

#### Item type {#pa-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the items to be stored in the container.

Objects of type `Item` should be default constructable.

#### Allocator {#pa-alloc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Alloc;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Allocator class.

### Constructors and destructors

#### Empty container constructor {#pa-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

#### Fill container {#pa-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(long n, const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

#### Copy constructor {#pa-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(const parray& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

#### Initializer-list constructor {#pa-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

#### Move constructor {#pa-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray(parray&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

#### Destructor {#pa-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~parray();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

### Operations

+------------------------+--------------------------------------+
| Operation              | Description                          |
+========================+======================================+
| [`operator[]`](#pa-i-o)| Access member item                   |
|                        |                                      |
+------------------------+--------------------------------------+
| [`size`](#pa-si)       | Return size                          |
+------------------------+--------------------------------------+
| [`resize`](#pa-rsz)    | Change size                          |
+------------------------+--------------------------------------+
| [`swap`](#pa-sw)       | Exchange contents                    |
+------------------------+--------------------------------------+

Table: Parallel-array member functions.

#### Indexing operator {#pa-i-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
reference operator[](long i);
const_reference operator[](long i) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a reference at the specified location `i`. No bounds check is
performed.

***Complexity.*** Constant time.

#### Size operator {#pa-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

#### Resize {#pa-rsz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void resize(long n, const value_type& val);
void resize(long n) {
  value_type val;
  resize(n, val);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container so that it contains `n` items.

The contents of the current container are removed and replaced by `n`
copies of the item referenced by `val`.

***Complexity.*** Let $m$ be the size of the container just before and
   $n$ just after the resize operation. Then, the work and span are
   linear and logarithmic in $\max(m, n)$, respectively.

#### Exchange operation {#pa-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(parray& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

Slice
-----

The `slice` structure provides an abstraction of a subarray for
parallel arrays. A `slice` value can be viewed as a triple `(p, lo,
hi)`, where `p` is a pointer to the underlying parallel array, `lo` is
the starting index, and `hi` is the index one past the end of the
range. The `slice` container class maintains the following invariant:
`0 <= lo` and `hi <= a->size()`.

The `slice` class is a template class that takes a single template
parameter, namely `Item`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace parray {

template <class Item>
class slice {
public:

  using value_type = Item;
  using parray_type = parray<value_type>;

  const parray_type* pointer;
  long lo;
  long hi;

  slice();
  slice(const parray_type* _pointer);
  slice(long _lo, long _hi, const parray_type _pointer=nullptr);
  
};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------------+-----------------------------------+
| Public field                      | Description                       |
+===================================+===================================+
| [`pointer`](#pa-sl-p)             | Pointer to a parallel-array       |
|                                   |structure (or a null pointer)      |
+-----------------------------------+-----------------------------------+
| [`lo`](#pa-sl-lo)                 | Starting index                    |
+-----------------------------------+-----------------------------------+
| [`hi`](#pa-sl-hi)                 | Index that is one past the last   |
|                                   |item in therange                   |
+-----------------------------------+-----------------------------------+

Table: Parallel-array slice fields.

### Member fields

#### Pointer {#pa-sl-p}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
const parray<Item>* pointer;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pointer to the parrallel-array structure (or null pointer, if range is
empty).

#### Starting index {#pa-sl-lo}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long lo;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Starting index of the range.

#### One-past-end index {#pa-sl-hi}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long hi;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

One position past the end of the range.

+-----------------------------------+-----------------------------------+
| Constructor                       | Description                       |
+===================================+===================================+
| [empty slice](#pa-sl-e-c) (default| Construct an empty slice with no  |
|constructor)                       |items                              |
+-----------------------------------+-----------------------------------+
| [full range](#pa-sl-pt)           | Construct a slice for a full range|
|                                   |                                   |
+-----------------------------------+-----------------------------------+
| [specified range](#pa-sl-ra)      | Construct a slice for a specified |
|                                   |range                              |
+-----------------------------------+-----------------------------------+

Table: Parallel-array slice constructors.

### Constructors

#### Empty slice {#pa-sl-e-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct an empty slice with no items.

#### Full range {#pa-sl-pt}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice(const parray<Item>* _pointer);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct a slice with the full range of the items in the parallel
array pointed to by `_pointer`.

#### Specified range {#pa-sl-ra}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice(long _lo, long _hi, const parray<Item>* _pointer=nullptr);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct a slice with the right-open range `[lo, hi)` of the items in
the parallel array pointed to by `_pointer`.

If the range is empty, then `_pointer` may be the null pointer
value. Otherwise, `_pointer` must be a valid pointer to a
parallel-array object.

Behavior is undefined if the range does not fit in the size of the
underlying parallel array.

Parallel chunked sequence {#pchunkedseq}
=========================

Interface and cost model
------------------------

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
                                                           
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <class Item, class Alloc = std::allocator<Item>>
class pchunkedseq;

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

Table: Parallel chunked sequence type definitions.

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

### Template parameters

#### Item type {#cs-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the items to be stored in the container.

Objects of type `Item` should be default constructable.

#### Allocator {#cs-alloc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Alloc;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Allocator class.

### Constructors and destructors

#### Empty container constructor {#cs-e-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Complexity.*** Constant time.

Constructs an empty container with no items;

#### Fill container {#cs-e-f-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(long n, const value_type& val);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with `n` copies of `val`.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

#### Copy constructor {#cs-e-cp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(const pchunkedseq& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with a copy of each of the items in `other`, in
the same order.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the resulting container, respectively.

#### Initializer-list constructor {#cs-i-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(initializer_list<value_type> il);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container with the items in `il`.

***Complexity.*** Work and span are linear in the size of the resulting
   container.

#### Move constructor {#cs-m-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq(pchunkedseq&& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constructs a container that acquires the items of `other`.

***Complexity.*** Constant time.

#### Destructor {#cs-destr}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
~pchunkedseq();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Destructs the container.

***Complexity.*** Work and span are linear and logarithmic in the size
   of the container, respectively.

### Sequential operations

+-----------------------------+--------------------------------------+
| Operation                   | Description                          |
+=============================+======================================+
| [`seq.operator[]`](#cs-i-o) | Access member item                   |
+-----------------------------+--------------------------------------+
| [`seq.size`](#cs-si)        | Return size                          |
+-----------------------------+--------------------------------------+
| [`seq.resize`](#cs-rsz)     | Change size                          |
+-----------------------------+--------------------------------------+
| [`seq.swap`](#cs-sw)        | Exchange contents                    |
+-----------------------------+--------------------------------------+

Table: Sequential operations of the parallel chunked sequence.

#### Indexing operator {#cs-i-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
reference operator[](long i);
const_reference operator[](long i) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a reference at the specified location `i`. No bounds check is
performed.

***Complexity.*** Constant time.

#### Size operator {#cs-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns the size of the container.

***Complexity.*** Constant time.

#### Exchange operation {#cs-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(pchunkedseq& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

### Parallel operations

+------------------------+--------------------------------------+
| Operation              | Description                          |
+------------------------+--------------------------------------+
| [`resize`](#cs-rsz)    | Change size                          |
+------------------------+--------------------------------------+
  
Table: Parallel operations of the parallel chunked sequence.

#### Resize {#cs-rsz}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void resize(long n, const value_type& val);
void resize(long n) {
  value_type val;
  resize(n, val);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Resizes the container so that it contains `n` items.

The contents of the current container are removed and replaced by `n`
copies of the item referenced by `val`.

***Complexity.*** Let $m$ be the size of the container just before and
   $n$ just after the resize operation. Then, the work and span are
   linear and logarithmic in $\max(m, n)$, respectively.

Data-parallel operations
========================

Indexed-based for loop
----------------------

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
pchunkedseq<long> xs(4);
parallel_for(0, n, [&] (long i) {
  xs[i] = i+1;
});

std::cout << "xs = " << xs << std::endl;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
xs = { 1, 2, 3, 4 }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


### Template parameters

The following table describes the template parameters that are used by
the different version of our parallel-for function.

+---------------------------------+-----------------------------------+
| Template parameter              | Description                       |
+=================================+===================================+
| [`Iter`](#lp-i)                 | Iterator function                 |
+---------------------------------+-----------------------------------+
| [`Seq_iter_rng`](#lp-s-i)       | A function for performing a       |
|                                 |specified range of iterations in a |
|                                 |sequential fashion                 |
+---------------------------------+-----------------------------------+
| [`Comp`](#lp-c)                 | Complexity function for a         |
|                                 |specified iteration                |
+---------------------------------+-----------------------------------+
| [`Comp_rng`](#lp-c-r)           | Complexity function for a         |
|                                 |specified range of iterations      |
+---------------------------------+-----------------------------------+

Table: All template parameters used by various instance of the
parallel-for loop.

#### Iterator function {#lp-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Iter;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(long i);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Sequential range-based iterator function {#lp-s-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_iter_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Complexity function {#lp-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Comp;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(long i);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#### Range-based compelxity function {#lp-c-r}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Comp_rng;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Instances

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {

template <class Iter>
void parallel_for(long lo, long hi, Iter iter);

template <
  class Iter,
  class Comp
>
void parallel_for(long lo, long hi, Comp comp, Iter iter);

} }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace range {

template <
  class Iter,
  class Comp_rng
>
void parallel_for(long lo, long hi, Comp_rng comp_rng, Iter iter);

template <
  class Iter,
  class Comp_rng,
  class Seq_iter_rng
>
void parallel_for(long lo,
                  long hi,
                  Comp_rng comp_rng,
                  Iter iter,
                  Seq_iter_rng seq_iter_rng);


} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Reduction
---------

+-----------------------------------+-----------------------------------+
| Abstraction layer                 | Description                       |
+===================================+===================================+
| [Level 0](#red-l-0)               | Apply a specified monoid to       |
|                                   |combine the items of a specified   |
|                                   |sequence container                 |
+-----------------------------------+-----------------------------------+
| [Level 1](#red-l-1)               | Introduces a lift operator that   |
|                                   |allows the client to combine the   |
|                                   |reduction with a specified         |
|                                   |tabulation, such that the          |
|                                   |tabulation is injected into the    |
|                                   |leaves of the reduction tree       |
+-----------------------------------+-----------------------------------+
| [Level 2](#red-l-2)               | Introduces an operator that       |
|                                   |provides a sequentialized          |
|                                   |alternative for the lift operator  |
+-----------------------------------+-----------------------------------+
| [Level 3](#red-l-3)               | Introduces a "mergeable output"   |
|                                   |type that enables                  |
|                                   |destination-passing style reduction|
+-----------------------------------+-----------------------------------+
| [Level 4](#red-l-4)               | Introduces a "splittlable input"  |
|                                   |type that abstracts from the type  |
|                                   |of the input container             |
+-----------------------------------+-----------------------------------+

Table: Abstraction layers used by pctl for reduction operators.

### Level 0 {#red-l-0}

+---------------------------------+-----------------------------------+
| Template parameter              | Description                       |
+=================================+===================================+
| [`Item`](#r0-i)                 | Type of the items in the input    |
|                                 |sequence                           |
+---------------------------------+-----------------------------------+
| [`Combine`](#r0-a)              | Associative combining operator    |
+---------------------------------+-----------------------------------+
| [`Weight`](#r0-w)               | Weight function (optional)        |
+---------------------------------+-----------------------------------+

Table: Shared template parameters for all level-0 reduce operations.

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

***Example: the "max" combining operator.*** The following functor is
   associative because the `std::max` function is itself associative.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Max_combine {
public:
  long operator()(long x, long y) {
    return std::max(x, y);
  }
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

***Example: the array-weight function.*** Let `Item` be
   `parray<long>`. Then, one valid weight function is the weight
   function that returns the size of the given array.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class PArray_weight {
public:
  long operator()(const parray<long>& xs) {
    return xs.size();
  }
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Parallel array {#r0-parray}

At this level, we have two types of reduction for parallel arrays. The
first one assumes that the combining operator takes constant time and
the second does not.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <class Item, class Combine>
Item reduce(const parray<Item>& xs, Item id, Combine combine);

template <
  class Item,
  class Weight,
  class Combine
>
Item reduce(const parray<Item>& xs,
            Item id,
            Weight weight,
            Combine combine);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Example: taking the maximum value of an array of numbers.*** The
following code takes the maximum value of `xs` using our `Max_combine`
functor.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<long>& xs) {
  return reduce(xs, 0, Max_combine());
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Alternatively, one can use C++ lambda expressions to implement the
same algorithm.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<long>& xs) {
  return reduce(xs, LONG_MIN, [&] (long x, long y) {
    return std::max(x, y);
  });
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Complexity {#r0-complexity}

There are two cases to consider for any reduction $\mathtt{reduce}(xs,
id, f)$: (1) the associative combining operator $f$ takes constant
time and (2) $f$ does not.

***(1) Constant-time associative combining operator.*** The amount of
work performed by the reduction is $O(| xs |)$ and the span is $O(\log
| xs |)$.

***(2) Non-constant-time associative combining operator.*** We define
$\mathcal{R}$ to be the set of all function applications $f(x, y)$
that are performed in the reduction tree. Then,

- The work performed by the reduction is $O(n + \sum_{f(x, y) \in
\mathcal{R}(f, id, xs)} W(f(x, y)))$.

- The span of the reduction is $O(\log n \max_{f(x, y) \in
\mathcal{R}(f i xs)} S(f(x, y)))$.

Under certain conditions, we can use the following lemma to deduce a
more precise bound on the amount of work performed by the
reduction.

***Lemma (Work efficiency).*** For any associative combining operator
$f$ and weight function $w$, if for any $x$, $y$,

1. $w(f(x, y)) \leq w(x) + w(y)$, and
2. $W \leq c (w(x) + w(y))$, for some constant $c$,

where $W$ denotes the amount of work performed by the call $f(x, y)$,
then the amount of work performed by the reduction is $O(\log | xs |
\sum_{x \in xs} (1 + w(x)))$.

***Example: using a non-constant time combining operator.*** Now, let
us consider a case where the associative combining operator takes
linear time in proportion with the combined size of its two
arguments. For this example, we will consider the following max
function, which examines a given array of arrays.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<parray<long>>& xss) {
  parray<long> id = { LONG_MIN };
  auto weight = [&] (const parray<long>& xs) {
    return xs.size();
  };
  auto combine = [&] (const parray<long>& xs1,
                      const parray<long>& xs2) {
    parray<long> r = { std::max(max(xs1), max(xs2)) };
    return r;
  };  
  parray<long> a = reduce(xss, id, weight, combine);
  return a[0];
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let us now analyze the efficiency of this algorithm. We will begin by
analyzing the work. To start, we need to determine whether the
combining operator of the reduction over `xss` is constant-time or
not. This combining operator is not because the combining operator
calls the `max` function twice. The first call is applied to the array
`xs` and the second to `ys`. The total work performed by these two
calls is linear in `xs.size() + ys.size()`. Therefore, by applying the
work-lemma shown above, we get that the total work performed by this
reduction is $O(\log | \mathtt{xss} | \max_{\mathtt{xs} \in
\mathtt{xss}} | xs ||)$. The span is simpler to analyze. By applying
our span rule for reduce, we get that the span for the reduction is
$O(\log |xss| \max_{\mathtt{xs} \in \mathtt{xss}} \log |xs|)$.

When the `max` function returns, the result is just one number that is
our maximum value. It is therefore unfortunate that our combining
operator has to pay to package the current maximum value in the array
`r`. The abstraction boundaries, in particular, the type of the
`reduce` function here leaves us no choice, however. In the next level
of abstraction, we are going to see that, by generalizing our `reduce`
function a little, we can sidestep this issue.

### Level 1 {#red-l-1}

***Index passing.*** TODO: explain

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
| [`Item_weight`](#r1-i-w)         | Weight function for the `Item`    |
|                                  |type                               |
+----------------------------------+-----------------------------------+
| [`Item_weight_idx`](#r1-i-w-i)   | Index-passing lifting operator    |
+----------------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 1.
                 
#### Result {#r1-r}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Result               
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the result value to be returned by the reduction.

This class must provide a default (i.e., zero-arity) constructor.

#### Lift {#r1-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift operator is a C++ functor that takes a value of type `Item`
and returns a value of type `Result`. The call operator for the `Lift`
class should have the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Index-passing lift {#r1-li}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_idx;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The lift operator is a C++ functor that takes an index and a value of
type `Item` and returns a value of type `Result`. The call operator
for the `Lift` class should have the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(long pos, const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The value passed in the `pos` parameter is the index in the source
array of item `x`.

#### Associative combining operator {#r1-comb}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Combine;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now, the type of our associative combining operator has changed from
what it is in level 0. In particular, the values that are being passed
and returned are values of type `Result`.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(const Result& x, const Result& y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Item-weight function {#r1-i-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item_weight;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The item-weight function is a C++ functor that takes a value of type
`Item` and returns a non-negative number of type `long`. The
`Item_weight` class should provide a call operator of the following
type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Index-passing item-weight function {#r1-i-w-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item_weight_idx;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The item-weight function is a C++ functor that takes an index and a
value of type `Item` and returns a non-negative number of type
`long`. The `Item_weight_idx` class should provide a call operator of
the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(long pos, const Item& x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {
namespace level1 {

template <
  class Item,
  class Result,
  class Combine,
  class Lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Combine combine,
              Lift lift);

template <
  class Item,
  class Result,
  class Combine,
  class Item_weight,
  class Lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Combine combine,
              Item_weight item_weight,
              Lift lift);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<parray<long>>& xss) {
  auto combine = [&] (long x, long y) {
    return std::max(x, y);
  };
  auto item_weight = [&] (const parray<long>& xs) {
    return xs.size();
  };
  auto lift = [&] (const parray<long>& xs) {
    return max(xs);
  };
  return level1::reduce(xss, 0, combine, item_weight, lift);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {
namespace level1 {

template <
  class Item,
  class Result,
  class Combine,
  class Lift_idx
>
Result reducei(const parray<Item>& xs,
               Result id,
               Combine combine,
               Lift_idx lift_idx);

template <
  class Item,
  class Result,
  class Combine,
  class Item_weight_idx,
  class Lift_idx
>
Result reducei(const parray<Item>& xs,
               Result id,
               Combine combine,
               Item_weight_idx item_weight_idx,
               Lift_idx lift_idx);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 2 {#red-l-2}

+---------------------------+-----------------------------------+
| Template parameter        | Description                       |
+===========================+===================================+
| [`Seq_lift`](#r2-l)       | Sequential alternative body for   |
|                           |the lift function                  |
+---------------------------+-----------------------------------+
| [`Item_rng_weight`](#r2-w)| Item weight by range.             |
|                           |                                   |
+---------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 2.

#### Sequential alternative body for the lifting operator {#r2-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_lift;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential-lift function is a C++ functor that takes a pair of
indices and returns a result value. The `Seq_lift` class should
provide a call operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Result operator()(long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Range-based item weight {#r2-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item_rng_weight;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The range-based item-weight function is a C++ functor that takes a
pair of indices and returns a non-negative number. The value returned
is to account for the combined weights of the items in the right-open
range `[lo, hi)` of the input sequence.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Parallel array
             
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {
namespace level2 {

template <
  class Item,
  class Result,
  class Combine,
  class Item_rng_weight,
  class Lift_idx,
  class Seq_lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Combine combine,
              Item_rng_weight item_rng_weight,
              Lift_idx lift_idx,
              Seq_lift seq_lift);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
template <class Item, class Weight>
parray<long> weights(const parray<Item>& xs, Weight weight);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<parray<long>>& xss) {
  parray<long> w = weights(xss, [&] (const parray<long>& xs) {
    return xs.size();
  });
  auto item_rng_weight = [&] (long lo, long hi) {
    return w[hi] - w[lo];
  };
  auto combine = [&] (long x, long y) {
    return std::max(x, y);
  };
  auto lift = [&] (const parray<long>& xs) {
    return max(xs);
  };
  auto seq_lift = [&] (long lo, long hi) {
    return max_seq(xss, lo, hi);
  };
  return level2::reduce(xss, 0, combine, item_rng_weight, lift, seq_lift);
}

long max_seq(const parray<parray<xs>>& xss, long lo, long hi) {
  long m = LONG_MAX;
  for (long i = lo; i < hi; i++) {
    const parray<long>& xs = xss[i];
    long n = xs.size();
    for (long j = 0; j < n; j++) {
      m = std::max(m, xs[j]);
    }
  }
  return m;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 3 {#red-l-3}

+----------------------------------+--------------------------------+
| Template parameter               | Description                    |
+==================================+================================+
| [`Output`](#r3-o)                | Type of the object to receive  |
|                                  |the output of the reduction     |
+----------------------------------+--------------------------------+
| [`Lift_idx_dst`](#r3-dpl)        | Lift function in               |
|                                  |destination-passing style       |
+----------------------------------+--------------------------------+
| [`Seq_lift_dst`](#r3-dpl-seq)    | Sequential lift function in    |
|                                  |destination-passing style       |
+----------------------------------+--------------------------------+

Table: Template parameters that are introduced in level 3.
                                      
#### Output {#r3-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the object to receive the output of the reduction.


+-----------------------------+-------------------------------------+
| Constructor                 | Description                         |
+==================================+================================+
| [copy constructor](#ro-c-c) | Copy constructor                    |
|                             |                                     |
+-----------------------------+-------------------------------------+

Table: Constructors that are required for the `Output` class.

Table: Required constructors for the `Output` class.

+-------------------------+-------------------------------------+
| Public method           | Description                         |
+=========================+=====================================+
| [`merge`](#ro-m)        | Merge contents                      |
+-------------------------+-------------------------------------+

Table: Public methods that are required for the `Output` class.

##### Copy constructor {#ro-c-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Output(const Output& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy constructor.

##### Merge {#ro-m}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void merge(Output& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Merge the contents of the current output with those of the output
referenced by `dst`, leaving the result in `dst`.

##### Example: cell output {#ro-co}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace datapar {
namespace level3 {

template <class Result, class Combine>
class cell {
public:

  Result result;
  Combine combine;

  cell(Result result, Combine combine)
  : result(result), combine(combine) { }

  cell(const cell& other)
  : combine(other.combine) { }

  void merge(cell& dst) {
    dst.result = combine(dst.result, result);
    Result empty;
    result = empty;
  }

};

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Destination-passing-style lift {#r3-dpl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_idx_dst;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The destination-passing-style lift function is a C++ functor that
takes an index, a reference on an item, and a reference on an output
object. The call operator for the `Lift_idx_dst` class should have the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(long pos, const Item& x, Output& out);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The value that is passed in for `pos` is the index in the input
sequence of the item `x`. The object referenced by `out` is the object
to receive the result of the lift function.

#### Destination-passing-style sequential lift {#r3-dpl-seq}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_lift_dst;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The destination-passing-style sequential lift function is a C++
functor that takes a pair of indices and a reference on an output
object. The call operator for the `Seq_lift_dst` class should have the
following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(long lo, long hi, Output& out);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The purpose of this function is provide an alternative sequential
algorithm that is to be used to process ranges of items from the
input. The range is specified by the right-open range `[lo, hi)`. The
object referenced by `out` is the object to receive the result of the
sequential lift function.

#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace level3 {

template <
  class Item,
  class Output,
  class Item_rng_weight,
  class Lift_idx_dst,
  class Seq_lift_dst
>
void reduce(const parray<Item>& xs,
            Output& out,
            Item_rng_weight item_rng_weight,
            Lift_idx_dst lift_idx_dst,
            Seq_lift_dst seq_lift_dst);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 4 {#red-l-4}

+-------------------------------+-----------------------------------+
| Template parameter            | Description                       |
+===============================+===================================+
| [`Input`](#r4-i)              | Type of input to the reduction    |
+-------------------------------+-----------------------------------+
| [`Input_weight`](#r4-i-w)     | Function to return the weight of a|
|                               |specified input                    |
+-------------------------------+-----------------------------------+
| [`Convert`](#r4-c)            | Function to convert from an input |
|                               |to an output                       |
+-------------------------------+-----------------------------------+
| [`Seq_convert`](#r4-s-c)      | Alternative sequentialized version|
|                               |of the `Convert` function.         |
+-------------------------------+-----------------------------------+

Table: Template parameters that are introduced in level 4.

#### Input {#r4-i}

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
| [`split`](#r4i-sp)          | Divide the input into two pieces          |
+-----------------------------+-------------------------------------------+

Table: Public methods that are required for the `Input` class.

##### Copy constructor {#r4i-cc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Input(const Input& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy constructor.

##### Can split {#r4i-c-s}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool can_split() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a boolean value to indicate whether a split is possible.

##### Split {#r4i-sp}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void split(Input& dst);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transfer a fraction of the contents of the current input object to the
input object referenced by `dst`.

The behavior of this method may be undefined when the `can_split`
function would return `false`.

##### Example: parallel-array-slice input

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace parray {
namespace level4 {

template <class Item>
class slice_input {
public:

  using slice_type = slice<Item>;

  slice_type slice;

  slice_input(const slice_input& other)
  : slice(slice) { }

  bool can_split() const {
    return slice.hi - slice.lo > 1;
  }

  void split(slice_input& dst) {
    dst.slice = slice;
    long mid = (slice.lo + slice.hi) / 2;
    slice.hi = mid;
    dst.slice.lo = mid;
  }

};

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Input weight {#r4-i-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_weight;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The input-weight function is a C++ functor which returns a positive
number that associates a weight value to a given input object. The
`Input_weight` class should provide the following call operator.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(const Input& in);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Convert {#r4-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Convert;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The convert function is a C++ functor which takes a reference on an
input value and computes a result value, leaving the result value in
an output cell. The `Convert` class should provide a call operator
with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Output& out);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Sequential convert {#r4-s-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Seq_convert;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function is a C++ functor whose purpose is to
substitute for the ordinary convert function when input size is small
enough to sequentialize. The `Seq_convert` class should provide a call
operator with the following type.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void operator()(Input& in, Output& out);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sequential convert function should always compute the same result
as the ordinary convert function given the same input. 

#### Generic reduce function

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace datapar {
namespace level4 {

template <
  class Input,
  class Output,
  class Input_weight,
  class Convert,
  class Seq_convert
>
void reduce(Input& in,
            Output& out,
            Input_weight input_weight,
            Convert convert,
            Seq_convert seq_convert);
            
} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 5 {#red-l-5}

+--------------------------------+-----------------------------------+
| Template parameter             | Description                       |
+================================+===================================+
| [`Block_input`](#r5-i)         | Type of input to the reduction    |
+--------------------------------+-----------------------------------+
| [`Block_input_weight`](#r5-i-w)| Function to return the weight of a|
|                                |specified input                    |
+--------------------------------+-----------------------------------+
| [`Convert`](#r4-c)             | Function to convert from an input |
|                                |to an output                       |
+--------------------------------+-----------------------------------+
| [`Seq_convert`](#r4-s-c)       | Alternative sequentialized version|
|                                |of the `Convert` function.         |
+--------------------------------+-----------------------------------+

#### Block input {#r5-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Blocked_input;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {
namespace level5 {

class block_input {
public:

  bool can_split() const {
  }

  long size() const {
  }

  long nb_blocks() const {
  }

  long block_size() const {
  }

};
            
} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 6 {#red-l-6}

#### Block input {#r6-b-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Block_input;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Block convert {#r6-b-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Block_convert;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<Output> operator()(Block_input& ins);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Block output {#r6-b-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Block_output;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
parray<Output> merge(const parray<Output>& blocks);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace datapar {
namespace level6 {

template <
  class Block_input,
  class Block_output,
  class Block_input_weight,
  class Block_convert,
  class Seq_convert
>
void reduce(Block_input& block_in,
            Block_output& block_out,
            Block_input_weight block_input_weight,
            Block_convert convert,
            Seq_convert seq_convert);
            
} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Scan
----

Derived operations
------------------

Sorting
=======

Merge sort
----------

Quick sort
----------

Sample sort
-----------

Radix sort
----------