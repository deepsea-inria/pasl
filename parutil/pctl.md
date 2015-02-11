% The Parallel-Container Template Library User's Guide
% Deepsea Project
% 10 February 2015

Introduction
============

The purpose of this document is to serve as a working draft of the
design and implementation of the Parallel Container Template Library
(PCTL).

Containers
==========

Sequence containers
-------------------

Class name                           | Description
-------------------------------------|-------------------------
[`array`](#parray)                   | Array class
[`chunkedseq`](#chunkedseq)          | Chunked Sequence class

Associative containers
----------------------

Class name          | Description
--------------------|-------------------------
set                 | Set class
map                 | Associative map class

Parallel array {#parray}
==============

Interface and cost model
------------------------

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <class Item>
class parray;

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type                 | Description
---------------------|---------------------------------------------------
`value_type`         | Alias for template parameter `Item`

+----------------+--------------------------------------+
| Operation      | Description                          |
+================+======================================+
| `operator[]`   | Access element                       |
+----------------+--------------------------------------+
| `size`         | Return size                          |
+----------------+--------------------------------------+
| `swap`         | Exchange contents                    |
+----------------+--------------------------------------+

Chunked sequence {#chunkedseq}
================

Data-parallel operations
========================

Tabulation
----------

Reduction
---------

### Level 0

+--------------------------------------------+-----------------------------------+
| Template parameter                         | Description                       |
+============================================+===================================+
| [`Item`](#reduce-0-item)                   | Type of the items in the input    |
|                                            |sequence                           |
+--------------------------------------------+-----------------------------------+
| [`Result`](#reduce-0-result)               | Type of the result of the         |
|                                            |reduction                          |
+--------------------------------------------+-----------------------------------+
| [`Assoc_oper`](#reduce-0-assoc)            | Associative combining operator    |
+--------------------------------------------+-----------------------------------+
| [`Assoc_oper_compl`](#reduce-0-assoc-compl)| Complexity function for the       |
|                                            |associative combining operator     |
+--------------------------------------------+-----------------------------------+

#### Item {#reduce-0-item}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Item
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the items being processed by the reduction.

#### Result {#reduce-0-result}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Result               
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the result value to be returned by the reduction.

#### Associative combining operator {#reduce-0-assoc}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Assoc_oper {
public:
  Result operator()(Result x, Result y);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Complexity function {#reduce-0-assoc-compl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Assoc_oper {
public:
  long operator()(Result* lo, Result* hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <
  class Item,
  class Assoc_oper
>
Result reduce(const parray<Item>& xs,
              Result id,
              Assoc_oper op);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <
  class Item,
  class Assoc_oper,
  class Assoc_oper_compl
>
Result reduce(const parray<Item>& xs,
              Result id,
              Assoc_oper op,
              Assoc_oper_compl assoc_oper_compl);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

***Example: summing an array of numbers.***

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
auto plus = [&] (long x, long y) {
  return x + y;
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long sum(const parray<long>& xs) {
  return reduce(xs, 0, plus);
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#### STL string

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace pstring {

template <
  class Result,
  class Assoc_oper,
  class Lift
>
Result reduce(const std::string& str,
              Result id,
              Assoc_oper op);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace pstring {

template <
  class Result,
  class Assoc_oper,
  class Assoc_oper_compl
>
Result reduce(const std::string& str,
              Result id,
              Assoc_oper op,
              Assoc_oper_compl assoc_oper_compl);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Chunked sequence

### Level 1

+--------------------------------------------+-----------------------------------+
| Template parameter                         | Description                       |
+============================================+===================================+
| [`Lift`](#reduce-1-lift)                   | Lifting operator                  |
+--------------------------------------------+-----------------------------------+
| [`Lift_compl`](#reduce-1-lift-compl)       | Complexity function for the lift  |
|                                            |operator                           |
+--------------------------------------------+-----------------------------------+

#### Lift {#reduce-1-lift}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift {
public:
  Result operator(Item x);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#### Lift complexity function {#reduce-1-lift-compl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Lift_compl {
public:
  long operator(const Item* lo, const Item* hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#### Parallel array

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

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
              Lift_complexity lift_complexity);

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### STL string

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace pstring {

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
              Lift_complexity lift_complexity);

} } }
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

### Level 2

+--------------------------------------------+-----------------------------------+
| Template parameter                         | Description                       |
+============================================+===================================+
| [`Liftn`](#reduce-2-liftn)                 | Range-based lifting operator      |
+--------------------------------------------+-----------------------------------+

#### Range-based lifting operator {#reduce-2-liftn}

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

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 3

+--------------------------------------------+--------------------------------+
| Template parameter                         | Description                    |
+============================================+================================+
| [`Output`](#reduce-3-output)               | Type of the object to receive  |
|                                            |the output of the reduction     |
+--------------------------------------------+--------------------------------+
| [`Output_base`](#reduce-3-output-base)     | Function for combining a range |
|                                            |of output values in memory      |
|                                            |                                |
+--------------------------------------------+--------------------------------+
| [`Output_compl`](#reduce-3-output-compl)   | Complexity function for        |
|                                            |combining ranges of output items|
|                                            |                                |
+--------------------------------------------+--------------------------------+


#### Output {#reduce-3-output}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Type of the object to receive the output of the reduction.

+----------------------------------------+-------------------------------------+
| Operation                              | Description                         |
+========================================+=====================================+
| [`initialize`](#dpo-output-initialize) | Initialize contents                 |
+----------------------------------------+-------------------------------------+
| [`merge`](#dpo-output-merge)           | Merge contents                      |
+----------------------------------------+-------------------------------------+

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

  void initialize(cell& other) {

  }

  void merge(const cell& other) {
    merge(other.result, result);
  }

};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Initialize {#dpo-output-initialize}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void initialize(Output& output);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the output.

##### Merge {#dpo-output-merge}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void merge(Output& source);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transfer the contents referenced by `source` to the output.

#### Output base {#reduce-3-output-base}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output_base {
public:
  void operator()(Output input, Output& output);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Output complexity function {#reduce-3-output-compl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Output_compl {
public:
  long operator()(Output* lo, Output* hi);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Level 4

+-------------------------------------------------------+-----------------------------------+
| Parameter                                             | Description                       |
+=======================================================+===================================+
| [`Input`](#reduce-4-input)                            | Type of input to the reduction    |
+-------------------------------------------------------+-----------------------------------+
| [`Input_base`](#reduce-4-input-base)                  | Functor to compute for the base   |
|                                                       |processing of the input            |
+-------------------------------------------------------+-----------------------------------+
| [`Input_compl`](#reduce-4-input-compl)                | Functor to return the complexity  |
|                                                       |value for processing a given input |
+-------------------------------------------------------+-----------------------------------+
| [`Compute_weights`](#reduce-4-compute-weights)        | Functor to compute the array of   |
|                                                       |weight values associated with a    |
|                                                       |given input                        |
+-------------------------------------------------------+-----------------------------------+
| [`Weighted_compl`](#reduce-4-weighted-compl)          | Functor using precomputed weights |
|                                                       |to return the complexity value for |
|                                                       |processing a given input           |
+-------------------------------------------------------+-----------------------------------+
                                                                   
#### Input {#reduce-4-input}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+-------------------------------------------+-------------------------------------------+
| Operation                                 | Description                               |
+===========================================+===========================================+
| [`initialize`](#reduce-4-input-initialize)| Initialize contents                       |
|                                           |                                           |
+-------------------------------------------+-------------------------------------------+
| [`can_split`](#reduce-4-input-can-split)  | Return value to indicate whether split is |
|                                           |possible                                   |
+-------------------------------------------+-------------------------------------------+
| [`size`](#reduce-4-input-size)            | Return size                               |
+-------------------------------------------+-------------------------------------------+
| [`nb_blocks`](#reduce-4-input-nb-blocks)  | Return the number of blocks               |
+-------------------------------------------+-------------------------------------------+
| [`block_size`](#reduce-4-input-block-size)| Return the number of elements per block   |
+-------------------------------------------+-------------------------------------------+
| [`split`](#reduce-4-input-split)          | Split the input                           |
+-------------------------------------------+-------------------------------------------+
| [`slice`](#reduce-4-input-slice)          | Return a new slice                        |
+-------------------------------------------+-------------------------------------------+

***Example: Parallel array slice.***

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
namespace pasl {
namespace data {
namespace parray {

template <class PArray>
class slice {
public:

  const PArray* array;
  long lo;
  long hi;
  
  slice()
  : lo(0), hi(0), array(nullptr) {}
  
  slice(const PArray* array)
  : lo(0), hi(0), array(array) {
    lo = 0;
    hi = array->size();
  }
  
  void initialize(const slice& other) {
    *this = other;
  }
  
  bool can_split() const {
    return size() <= 1;
  }
  
  long size() const {
    return hi - lo;
  }
  
  long nb_blocks() const {
    return 1 + ((size() - 1) / block_size());
  }
  
  long block_size() const {
    return (long)std::pow(size(), 0.5);
  }
  
  void split(slice& destination) {
    assert(can_split());
    destination = *this;
    long mid = (lo + hi) / 2;
    hi = mid;
    destination.lo = mid;
  }
  
  slice slice(long lo2, long hi2) {
    assert(lo2 >= lo2);
    assert(hi2 <= hi);
    slice tmp = *this;
    tmp.lo = lo2;
    tmp.hi = hi2;
    return tmp;
  }
  
};

} } }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

##### Initialize {#reduce-4-input-initialize}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void initialize(Input& input);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Initialize the contents of the input.

##### Can split {#reduce-4-input-can-split}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
bool can_split();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a boolean value to indicate whether a split is possible.

##### Size {#reduce-4-input-size}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long size();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of items in the input.

##### Number of blocks {#reduce-4-input-nb-blocks}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long nb_blocks();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of blocks in the input.

##### Block size {#reduce-4-input-block-size}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
long block_size();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a number to report the number of items contained by each block
in the input.

#### Split {#reduce-4-input-split}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
void split(Input& destination);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transfer a fraction of the input to the input referenced by
`destination`.

The behavior of this method may be undefined when the `can_split`
function would return `false`.

##### Slice {#reduce-4-input-slice}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
Input slice(long lo, long hi);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return a new slice that represents the items in the right-open range
`[lo, hi)`.

#### Input base {#reduce-4-input-base}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_base {
public:
  void operator()(Input input, Output& output);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Input complexity function {#reduce-4-input-compl}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Input_compl {
public:
  long operator()(Input input);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Compute Weights {#reduce-4-compute-weights}

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.cpp}
class Compute_weights {
public:
  parray<long> operator()(long nb_blocks);
};
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#### Weighted complexity function {#reduce-4-weighted-compl}

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