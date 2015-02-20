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
-------------------------------------|-------------------------
[`array`](#parray)                   | Array class
[`chunkedseq`](#chunkedseq)          | Chunked Sequence class

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

### Resize {#pa-rsz}

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

### Exchange operation {#pa-sw}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void swap(parray& other);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exchanges the contents of the container with those of `other`. Does
not invoke any move, copy, or swap operations on individual items.

***Complexity.*** Constant time.

Slice
-----

The `slice` structure provides an abstraction of a subarray for
parallel arrays. A `slice` value can be viewed as a triple `(a, lo,
hi)`, where `a` is a pointer to the underlying parallel array, `lo` is
the starting index, and `hi` is the index one past the end of the
range. The `slice` container class maintains the following invariant:
`0 <= lo` and `hi <= a->size()`.

The `slice` class is a template class that takes a single parameter:
the `PArray_pointer`. For correct behavior, this type must be that of
a pointer to either a const or non-const pointer to an object of the
`parray` class.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace pctl {
namespace parray {

template <class PArray_pointer>
class slice {
public:

  PArray_pointer pointer;
  long lo;
  long hi;

  slice();
  slice(PArray_pointer _pointer);
  slice(long _lo, long _hi, PArray_pointer _pointer=nullptr);
  
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

### Pointer {#pa-sl-p}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
PArray_pointer pointer;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pointer to the parrallel-array structure (or null pointer, if range is
empty).

### Starting index {#pa-sl-lo}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long lo;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Starting index of the range.

### One-past-end index {#pa-sl-hi}

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

### Empty slice {#pa-sl-e-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct an empty slice with no items.

### Full range {#pa-sl-pt}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice(PArray_pointer _pointer);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct a slice with the full range of the items in the parallel
array pointed to by `_pointer`.

### Specified range {#pa-sl-ra}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
slice(long _lo, long _hi, PArray_pointer _pointer=nullptr);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct a slice with the right-open range `[lo, hi)` of the items in
the parallel array pointed to by `_pointer`.

If the range is empty, then `_pointer` may be the null pointer
value. Otherwise, `_pointer` must be a valid pointer to a
parallel-array object.

Behavior is undefined if the range does not fit in the size of the
underlying parallel array.

Chunked sequence {#chunkedseq}
================

Data-parallel operations
========================

Tabulation
----------

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

***(Lemma) Work efficiency.*** For any associative combining operator
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

> TODO: Introduce `reducei` function, which is a version of level-1
> `reduce` that passes to `lift` (and to `item_weight`) the
> corresponding position in the input sequence.

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

> TODO: specify in the `parray` class the `compute_weights` function

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<parray<long>>& xss) {
  parray<long> weights = compute_weights(xss, [&] (const parray<long>& xs) {
    return xs.size();
  });
  auto item_rng_weight = [&] (long lo, long hi) {
    return weights[hi] - weights[lo];
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
                                      
#### Output {#r3-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the object to receive the output of the reduction.

+-------------------------+-------------------------------------+
| Operation               | Description                         |
+=========================+=====================================+
| [`init`](#ro-i)         | Initialize contents                 |
+-------------------------+-------------------------------------+
| [`merge`](#ro-m)        | Merge contents                      |
+-------------------------+-------------------------------------+

##### Initialize {#ro-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void init(Output& output);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the output.

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

template <class Result, class Combine>
class cell {
public:

  Result result;
  Combine combine;

  cell(Result result, Combine combine)
  : result(result), combine(combine) { }

  cell(const cell& other)
  : combine(other.combine) { }

  void init(cell& other) {

  }

  void merge(cell& dst) {
    dst.result = combine(dst.result, result);
    Result empty;
    result = empty;
  }

};

} } }
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

The `init` method of `out` is called once, prior to the call to the
lift function.

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

The `init` method of `out` is called once, prior to the call to the
lift function.

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
| Parameter                     | Description                       |
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

#### Input {#r4-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-----------------------------+-------------------------------------------+
| Operation                   | Description                               |
+=============================+===========================================+
| [`init`](#r4i-i)            | Initialize contents                       |
|                             |                                           |
+-----------------------------+-------------------------------------------+
| [`can_split`](#r4i-c-s)     | Return value to indicate whether split is |
|                             |possible                                   |
+-----------------------------+-------------------------------------------+
| [`split`](#r4i-sp)          | Divide the input into two pieces          |
+-----------------------------+-------------------------------------------+

##### Initialize {#r4i-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void init(Input& input);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the input.

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

TODO

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
            
} } }
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