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

***Complexity.*** Let `m` be the maximum of the size of the container
   just before and the size just after the resize operation, namely
   `n`. Work and span are linear and logarithmic in `m`, respectively.

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
|                                   |allows the client to inline a "map"|
|                                   |operation into the leaves of the   |
|                                   |reduce tree                        |
+-----------------------------------+-----------------------------------+
| [Level 2](#red-l-2)               | Introduces a range-based lift     |
|                                   |operation                          |
+-----------------------------------+-----------------------------------+
| [Level 3](#red-l-3)               | Introduces a "mergeable output"   |
|                                   |abstraction instead of the monoid  |
+-----------------------------------+-----------------------------------+
| [Level 4](#red-l-4)               | Introduces a "splittlable input"  |
|                                   |instead of a container             |
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
class Item
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
Item operator()(Item x, Item y);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The behavior of the reduction is well defined only if the combining
operator is *associative*, in the following sense.

***Post condition: associativity.*** Let `f` be an object of type
`Assoc_oper`. The operator `f` is associative if, for any `x`, `y`,
and `z` that are values of type `Item`, the following equality holds:

`f(x, f(y, z)) == f(f(x, y), z)`

For instance, the following functor is associative because the
`std::max` function is itself associative.

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
returns a non-negative "weight value" describing the size of the item.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long operator()(Item x);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Post condition: congruence.*** Let `w` be a value of type `Weight`
   and `f` be a value of type `Combine`.  Then, for any `x` and `y`
   that are values of type `Item`, the following holds.  Let $C(
   \mathtt{f(x,y)} )$ denote the cost (i.e., asymptotic work
   complexity) of performing the call `f(x, y)`.  Then, $\mathtt{w(x)}
   + \mathtt{w(y)} \leq O(C(\mathtt{f(x,y)}))$.

#### Parallel array

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

***Complexity.*** TODO

***Example: taking the maximum value of an array of numbers.*** The
following code takes the maximum value of `xs` using our `Max_combine`
functor.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<long>& xs) {
  return reduce(xs, 0, Max_combine());
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Alternatively, one can use specify the same algorithm as above by
replacing the `Max_combine` functor by an appropriate C++ lambda
expression.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<long>& xs) {
  return reduce(xs, LONG_MIN, [&] (long x, long y) {
    return std::max(x, y);
  });
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Example: using a non-constant time combining operator.***

Now, let us consider a case where the associative combining operator
takes linear time in proportion with its two arguments. For this
example, we will consider the following max function, which examines a
given array of arrays.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long max(const parray<parray<long>>& xss) {
  auto weight = [&] (const parray<long>& xs) {
    return xs.size();
  };
  parray<long> id = { LONG_MIN };
  parray<long> a =
    reduce(xss, id, weight, [&] (const parray<long>& xs1,
                                 const parray<long>& xs2) {
      parray<long> r = { std::max(max(xs1), max(xs2)) };
      return r;
    });
  return a[0];
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It should be clear that the weight function that we are using is a
valid weight function with respect to our [congruence
condition](#r0-w). The reason is that the amount of work required to
find the maximum number in a given array of numbers is simply linear
in the size of the array. As such, the size is the correct measure for
the weight.

Given that we are only interested in the single maximum value, it is
unfortunate that our combining operator has to pay to package the
current maximum value in the array `r`. The abstraction boundaries, in
particular, the type of the `reduce` function here leaves us no
choice, however. In the next level of abstraction, we are going to see
that, by generalizing our `reduce` function a little, we can sidestep
this issue.

### Level 1 {#red-l-1}

+----------------------------------+-----------------------------------+
| Template parameter               | Description                       |
+==================================+===================================+
| [`Result`](#r1-r)                | Type of the result being returned |
|                                  |by the reduction                   |
+----------------------------------+-----------------------------------+
| [`Lift`](#r1-l)                  | Lifting operator                  |
+----------------------------------+-----------------------------------+
| [`Lift_compl`](#r1-l-c)          | Complexity function for the lift  |
|                                  |operator                           |
+----------------------------------+-----------------------------------+
| [`Assoc_oper`](#r1-a-o)          | Associative combining operator    |
+----------------------------------+-----------------------------------+
| [`Assoc_oper_compl`](#r1-a-o-c)  | Complexity function for the       |
|                                  |associative combining operator     |
+----------------------------------+-----------------------------------+

#### Result {#r1-r}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Result               
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the result value to be returned by the reduction.

#### Lift {#r1-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift {
public:
  Result operator()(Item x);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Lift complexity function {#r1-l-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_compl {
public:
  long operator()(const Item* lo, const Item* hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Associative combining operator {#r1-a-o}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Assoc_oper {
public:
  Result operator()(Result x, Result y);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Complexity function {#r1-a-o-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Assoc_oper_compl {
public:
  long operator()(long lo, long hi);
};
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
  class Assoc_oper,
  class Lift
>
Result reduce(const parray<Item>& xs,
              Result id,
              Assoc_oper assoc_oper,
              Lift lift);


template <
  class Item,
  class Result,
  class Assoc_oper,
  class Assoc_oper_compl,
  class Lift,
  class Lift_complexity
>
Result reduce(const parray<Item>& xs,
              Result id,
              Assoc_oper assoc_oper,
              Assoc_oper_compl assoc_oper_compl,
              Lift lift,
              Lift_compl lift_compl);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### STL string

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace pstring {
namespace level1 {

template <
  class Item,
  class Result,
  class Assoc_oper
  class Lift
>
Result reduce(const std::string& str,
              Result id,
              Assoc_oper assoc_oper,
              Lift lift);

template <
  class Item,
  class Result,
  class Assoc_oper,
  class Assoc_oper_compl,
  class Lift,
  class Lift_complexity
>
Result reduce(const std::string& str,
              Result id,
              Assoc_oper assoc_oper,
              Assoc_oper_compl assoc_oper_compl,
              Lift lift,
              Lift_compl lift_compl);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Example: nested reduction.***
In this example, we consider a simple problem whose solution makes use
of nested parallelism. Our input is an array of strings and a
character value. The objective is to count the number of occurrences
of the character in the given array of strings. For the solution, we
use an inner reduction to count the number of occurrences in a given
string and an outer reduction to sum across all strings in a given
input. Note that we pass to the outer reduction the function that
computes the size of a given string. This function is used by the
outer reduction to appropriately assign weights to the inner
reductions.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
auto string_size = [&] (std::string& str) {
  return str.size();
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long occurrence_count(const std::string& str, char c) {
  return reduce(str, 0, plus, [&] (char d) {
    return (c == d) ? 1 : 0;
  });
}

long occurrence_count(const parray<std::string>& strs, char c) {
  return reduce(strs, 0, plus, string_size, [&] (const std::string& str) {
    return occurrence_count(str, c);
  });
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 2 {#red-l-2}

+--------------------------+-----------------------------------+
| Template parameter       | Description                       |
+==========================+===================================+
| [`Liftn`](#r2-l)         | Range-based lifting operator      |
+--------------------------+-----------------------------------+

#### Range-based lifting operator {#r2-l}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Liftn {
public:
  Result operator(const Item* lo, const Item* hi);
};
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
  class Assoc_oper,
  class Assoc_oper_compl,
  class Liftn,
  class Lift_compl
>
Result reduce(const parray<Item>& xs,
              Result id,
              Assoc_oper assoc_oper,
              Assoc_oper_compl assoc_oper_compl,
              Liftn liftn,
              Lift_compl lift_compl);

} } } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 3 {#red-l-3}

+-----------------------------+--------------------------------+
| Template parameter          | Description                    |
+=============================+================================+
| [`Output`](#r3-o)           | Type of the object to receive  |
|                             |the output of the reduction     |
+-----------------------------+--------------------------------+
| [`Output_base`](#r3-o-b)    | Function for combining a range |
|                             |of output values in memory      |
|                             |                                |
+-----------------------------+--------------------------------+
| [`Output_compl`](#r3-o-c)   | Complexity function for        |
|                             |combining ranges of output items|
|                             |                                |
+-----------------------------+--------------------------------+


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

***Example: cell.***

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace datapar {

template <class Result, class Merge>
class cell {
public:

  Result result;
  Merge merge;

  cell(Result result, Merge merge)
  : result(result), merge(merge) { }

  cell(const cell& other)
  : result(other.result), merge(other.merge) { }

  void init(cell& other) {

  }

  void merge(const cell& other) {
    merge(other.result, result);
  }

};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Initialize {#ro-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void init(Output& output);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the output.

##### Merge {#ro-m}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void merge(Output& source);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transfer the contents referenced by `source` to the output.

#### Output base {#r3-o-b}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output_base {
public:
  void operator()(Output input, Output& output);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Output complexity function {#r3-o-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output_compl {
public:
  long operator()(long lo, long hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 4 {#red-l-4}

+-------------------------------+-----------------------------------+
| Parameter                     | Description                       |
+===============================+===================================+
| [`Input`](#r4-i)              | Type of input to the reduction    |
+-------------------------------+-----------------------------------+
| [`Input_base`](#r4-i-b)       | Functor to compute for the base   |
|                               |processing of the input            |
+-------------------------------+-----------------------------------+
| [`Input_compl`](#r4-i-c)      | Functor to return the complexity  |
|                               |value for processing a given input |
+-------------------------------+-----------------------------------+
| [`Compute_weights`](#r4-c-w)  | Functor to compute the array of   |
|                               |weight values associated with a    |
|                               |given input                        |
+-------------------------------+-----------------------------------+
| [`Weighted_compl`](#r4-w-c)   | Functor using precomputed weights |
|                               |to return the complexity value for |
|                               |processing a given input           |
+-------------------------------+-----------------------------------+
                                                                   
#### Input {#r4-i}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input
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
| [`size`](#r4i-si)           | Return size                               |
+-----------------------------+-------------------------------------------+
| [`nb_blocks`](#r4i-n-b)     | Return the number of blocks               |
+-----------------------------+-------------------------------------------+
| [`block_size`](#r4i-b-s)    | Return the number of elements per block   |
+-----------------------------+-------------------------------------------+
| [`split`](#r4i-sp)          | Divide the input into two pieces          |
+-----------------------------+-------------------------------------------+
| [`range`](#r4i-sl)          | Copy a range to a given input object      |
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

##### Size {#r4i-si}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of items in the input.

##### Number of blocks {#r4i-n-b}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long nb_blocks();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of blocks in the input.

##### Block size {#r4i-b-s}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long block_size() const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of items contained by each block
in the input.

#### Split {#r4i-sp}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void split(Input& dest);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transfer a fraction of the input to the input referenced by
`dest`.

The behavior of this method may be undefined when the `can_split`
function would return `false`.

##### Range {#r4i-sl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void range(long lo2, long hi2, Input& dest) const;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Construct the input object referenced by `dest` with the right-open
range `[lo2, hi2)`.

#### Input base {#r4-i-b}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_base {
public:
  void operator()(Input input, Output& output);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Input complexity function {#r4-i-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_compl {
public:
  long operator()(Input input);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Compute Weights {#r4-c-w}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Compute_weights {
public:
  parray<long> operator()(long nb_blocks);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Weighted complexity function {#r4-w-c}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Weighted_compl {
public:
  long operator()(const long* lo, const long* hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Template-function signature

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace datapar {

template <
  class Input,
  class Output,
  class Input_base,
  class Output_base,
  class Input_complexity,
  class Output_complexity,
  class Compute_weights,
  class Weighted_complexity
>
void reduce(Input in,
            Output& out,
            Input_base input_base,
            Output_base output_base,
            Input_complexity input_complexity,
            Output_complexity output_complexity,
            Compute_weights compute_weights,
            Weighted_complexity weighted_complexity);

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