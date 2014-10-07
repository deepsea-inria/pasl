#include <algorithm>

#include "benchmark.hpp"

static
void myprint(int* a, int n) {
  for (int i = 0; i < n; i++)
    std::cout << " " << a[i];
  std::cout << std::endl;
}

// copies the elements in the range, defined by
// [first, last), to another range beginning at
// d_first.
static
void pcopy(int* first, int* last, int* d_first) {
  const size_t cutoff = 10000;
  size_t nb = last-first;
  if (nb <= cutoff) {
    std::copy(first, last, d_first);
  } else {
    size_t m = nb/2;
    pcopy(first,   first+m, d_first);
    pcopy(first+m, last,    d_first+m);
  }
}

// merges contents of:
//   a[first_a1, ..., last_a1-1]   and
//   a[first_a2, ..., last_a2-1]
// assuming that these two subarrays are sorted
// leaving the result in
//   tmp[tmp_first, ...]
static
void merge_seq(const int* a, int* tmp,
               int first_a1, int last_a1,
               int first_a2, int last_a2,
               int tmp_first) {
  int i = first_a1;
  int j = first_a2;
  int z = tmp_first;
  
  // merge two halves until one is empty
  while (i < last_a1 and j < last_a2)
    tmp[z++] = (a[i] < a[j]) ? a[i++] : a[j++];
  
  // copy remaining items
  std::copy(a+i, a+last_a1, tmp+z);
  std::copy(a+j, a+last_a2, tmp+z);
}

// merges contents of:
//   a[first, ..., mid-1]   and
//   a[mid,  ..., last-1]
// assuming that these two subarrays are sorted
// leaving the result in
//   a[first, ...]
// using tmp[first, ...] as scratch space
static
void merge_seq(int* a, int* tmp,
               int first, int mid, int last) {
  merge_seq(a, tmp, first, mid, mid, last, first);
  
  // copy back to source array
  std::copy(tmp+first, tmp+last, a+first);
}

// returns the position of the first item in
// a[first, ..., last-1] which does not compare
// less than val
static
int lower_bound(const int* a, int first, int last, int val) {
  const int* p = std::lower_bound(a+first, a+last, val);
  return (int)(p-a);
}

// merges contents of:
//   a[first_a1, ..., last_a1-1]   and
//   a[first_a2, ..., last_a2-1]
// assuming that each of these two subarrays are sorted
// leaving the result in
//   tmp[tmp_first, ...]
static
void merge_par(const int* a, int* tmp,
               int first_a1, int last_a1,
               int first_a2, int last_a2,
               int tmp_first) {
  int n1 = last_a1 - first_a1;
  int n2 = last_a2 - first_a2;
  
  if (n1 < n2) {
    // to ensure that the first subarray being sorted is the larger or the two
    merge_par(a, tmp, first_a2, last_a2, first_a1, last_a1, tmp_first);
    
  } else if (n1 == 1) {
    
    if (n2 == 0) {
      // a1 singleton; a2 empty
      tmp[tmp_first] = a[first_a1];
    } else {
      // both singletons
      tmp[tmp_first+0] = std::min(a[first_a1], a[first_a2]);
      tmp[tmp_first+1] = std::max(a[first_a1], a[first_a2]);
    }
    
  } else {
    
    // select pivot positions
    int mid_a1 = (first_a1+last_a1)/2;
    int mid_a2 = lower_bound(a, first_a2, last_a2, a[mid_a1]);
    
    // number of items to be treated by the first parallel call
    int k = (mid_a1-first_a1) + (mid_a2-first_a2);
    
    merge_par(a, tmp, first_a1, mid_a1,   first_a2, mid_a2,   tmp_first);
    merge_par(a, tmp, mid_a1,   last_a1,  mid_a2,   last_a2,  tmp_first+k);
    
  }
}

// merges contents of:
//   a[first, ..., mid-1]   and
//   a[mid,  ..., last-1]
// assuming that each of these two subarrays are sorted
// leaving the result in
//   a[first, ...]
// using tmp[first, ...] as scratch space
static
void merge_par(int* a, int* tmp,
               int first, int mid, int last) {
  merge_par(a, tmp, first, mid, mid, last, first);
  
  // copy back to source array
  pcopy(tmp+first, tmp+last, a+first);
}

// same comment as above
static
void merge(int* a, int* tmp,
           int first, int mid, int last) {
  merge_par(a, tmp, first, mid, last);
}

// sorts contents of:
//    a[first, ..., last-1]
// leaving the result in
//    a[first, ...]
// using tmp[first, ...] as scratch space
static
void sort(int* a, int* tmp,
          int first, int last) {
  // base case, 1: empty sequence.
  if (first + 1 >= last)
    return;
  
  int mid = (first + last) / 2;
  
  // base case, 2: 1 or 2 elements in the sequence.
  if (last - first <= 2) {
    if (a[first] > a[mid])
      std::swap(a[first], a[mid]);
    return;
  }
  
  sort (a, tmp, first, mid);
  sort (a, tmp, mid, last);
  
  merge(a, tmp, first, mid, last);
}

// sorts the items in the range [first, last) in
// ascending order
static
void sort(int* first, int* last) {
  int n = (int)(last-first);
  int* tmp = (int*)malloc(sizeof(int)*n);
  sort(first, tmp, 0, n);
  free(tmp);
}

template <
  class Container,
  class Copy_fct,
  class Are_equal_fct,
  class Trusted_sort_fct,
  class Untrusted_sort_fct
>
bool check(const Container& input,
           const Copy_fct& copy_fct,
           const Are_equal_fct& are_equal_fct,
           const Trusted_sort_fct& trusted_sort_fct,
           const Untrusted_sort_fct& untrusted_sort_fct) {
  Container trusted_input;
  Container untrusted_input;
  copy_fct(input, trusted_input);
  copy_fct(input, untrusted_input);
  trusted_sort_fct(trusted_input);
  untrusted_sort_fct(untrusted_input);
  return are_equal_fct(trusted_input, untrusted_input);
}

// later: parallelize
static
bool are_equal(int* a, int* b, int n) {
  for (int i = 0; i < n; i++)
    if (a[i] != b[i])
      return false;
  return true;
}

int main(int argc, char** argv) {
  
  int a[] = { 9, 1, 3, 5, 7, 0,
              2, 4, 6, 8 };
  const int nb = sizeof(a) / sizeof(int);
  
#if 0
  int first = 1;
  int last = 9;
  int mid = 5;
  int tmp[nb];
  merge_seq(a, tmp, first, mid, last);
//  merge_par(a, tmp, first, mid, last);
  std::cout << "size of a: " << nb << std::endl;
  std::cout << "contents of a:";
  myprint(a+first, last-first);
  
#else
  
  sort(a, a+nb);
  myprint(a, nb);
  
#endif
  
  auto init = [&] {

  };
  auto run = [&] (bool sequential) {

  };
  auto output = [&] {

  };
  auto destroy = [&] {
    ;
  };
  
  pasl::sched::launch(argc, argv, init, run, output, destroy);
  
  return 0;
}