#include "granularity-lite.hpp"
#include <iostream>
#include <cstring>
#include <math.h>
#include "sequencedata.hpp"
#include "sequenceio.hpp"
#include "io.hpp"
#include "container.hpp"

using std::pair;
using std::min;
using std::max;
/*---------------------------------------------------------------------*/

#ifdef CMDLINE
  typedef control_by_cmdline control_type;
#elif PREDICTION
  typedef control_by_prediction control_type;
#elif CUTOFF_WITH_REPORTING
  typename control_by_cutoff_with_reporting control_type;
#elif CUTOFF_WITHOUT_REPORTING
  typename control_by_cutoff_without_reporting control_type;
#endif

void print_array(int* a, int len){
  std::cerr << len << "\n";
  for (int i = 0; i < len; i++) {
    std::cerr << a[i] << " ";
  }
  std::cerr << "\n";
}

bool equals(int* a, int* b, int len) {
  for (int i = 0; i < len; i++) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

void merge_two_parts(int* a, int left1, int right1, int left2, 
                     int right2, int* result, int l){
  int i = left1;
  int j = left2;
  int z = l;
   
  while (i <= right1 and j <= right2) {
    if (a[i] < a[j]) {
      result[z++] = a[i++];
    } else {
      result[z++] = a[j++];
    }
  }
   
  while (i <= right1) {
    result[z++] = a[i++];
  }
   
  while (j <= right2) {
    result[z++] = a[j++];
  }
}

int lower_bound(int* a, int left, int right, int x) {
  int l = -1;
  int r = right - left;
  while (l < r - 1) {
    int m = (l + r) >> 1;
    if (a[m+left] <= x)
      l = m;
    else
      r = m;
  }
  return l;
}

pair<int, int> find(int* a, int left, int mid, int right, int c) {
  int l = -1;
  int r = mid - left;
   
  int ll = -1;
  int rr = -1;
   
  while (l < r - 1) {
    int m = (l + r) >> 1;
    int k = lower_bound(a, mid, right, a[m + left]);
      
    if (m + 1 + k + 1 <= c) {
      l = m;
      ll = m;
      rr = k;
    } else {
      r = m;
    }
  }
   
  rr += (c - ll - 1 - rr - 1);
   
  return pair<int, int>(ll + left, rr + mid);
}

/*void merge_parallel(int* a, int left, int mid, int right) {
   pasl::sched::native::parallel_for(0, (right - left + BLOCK - 1) / BLOCK, [&](int k) {
      pair<int, int> l = find(a, left, mid, right, k * BLOCK);
      pair<int, int> r = find(a, left, mid, right, min((k + 1) * BLOCK, right - left));
      merge_two_parts(a, l.first + 1, r.first, l.second + 1, r.second, tmp, left + k * BLOCK);
   });

   pasl::sched::native::parallel_for(0, (right - left + BLOCK - 1) / BLOCK, [&](int k) {
        memcpy((a + left + k * BLOCK), (tmp + left + k * BLOCK), sizeof(int) * (min((k + 1) * BLOCK, right - left) - k * BLOCK));
    });
}*/

control_type cmemcpy("parallel memcpy");
int memcpy_cutoff_const;

void memcpy_parallel(int* a, int* tmp, int left, int mid, int right, int L, int R) {
  cstmt(cmemcpy, 
        [&] { return R - L <= memcpy_cutoff_const; }, 
        [&] { return R - L; }, 
        [&] {fork2([&] {memcpy_parallel(a, tmp, left, mid, right, L, (L + R) / 2); },
                   [&] {memcpy_parallel(a, tmp, left, mid, right, (L + R) / 2, R); });},
        [&] {if (R == L) return;
             memcpy((a + left + L), (tmp + left + L), sizeof(int) * (R - L));}
  );
}

control_type cmergeroutine("merge parallel routine");
int merge_routine_cutoff_const;

void merge_parallel_routine(int* a, int* tmp, int left, int mid, int right, int L, int R) {
  cstmt(cmergeroutine, 
        [&] { return R - L <= merge_routine_cutoff_const; }, 
        [&] { return R - L; }, 
        [&] {fork2([&] {merge_parallel_routine(a, tmp, left, mid, 
                                               right, L, (L + R) / 2); },
                   [&] {merge_parallel_routine(a, tmp, left, mid, 
                                               right, (L + R) / 2, R); });},
        [&] {if (R == L) return;
             pair<int, int> l = find(a, left, mid, right, L);
             pair<int, int> r = find(a, left, mid, right, R);

             merge_two_parts(a, l.first + 1, r.first, l.second + 1, 
                             r.second, tmp, left + L);}
  );
}

void merge_parallel(int* a, int* tmp, int left, int mid, int right) {
  merge_parallel_routine(a, tmp, left, mid, right, 0, right - left);
  memcpy_parallel(a, tmp, left, mid, right, 0, right - left);
}

void merge_seq(int*& a, int* tmp, int left, int mid, int right) {
  int i = left;
  int j = mid;
  int z = left;

  while (i < mid and j < right) {
    if (a[i] < a[j]) {
      tmp[z++] = a[i];
      i++;
    } else {
      tmp[z++] = a[j];
      j++;
    }
  }

  while (i < mid) {
    tmp[z++] = a[i];
    i++;
  }
   
  while (j < right) {
    tmp[z++] = a[j];
    j++;
  }
   
  for (int i = left; i < right; i++) {
    a[i] = tmp[i];
  }
}
control_type cmerge("merge");
int merge_cutoff_const;

static
void merge(int*& a, int* tmp, int left, int mid, int right) {
  cstmt(cmerge, 
        [&] { return right - left <= merge_cutoff_const; }, 
        [&] { return right - left; }, 
        [&] {merge_parallel(a, tmp, left, mid, right);}, 
        [&] {merge_seq(a, tmp, left, mid, right);}
  );
}

control_type csort("mergesort");
int sort_cutoff_const;

static
void sort(int* a, int* tmp, int left, int right) {
  if (left + 1 >= right) 
    return;

  int mid = (left + right) / 2;
    
  cstmt(csort, 
        [&] { return (right - left) * log(right - left) <= sort_cutoff_const; }, 
        [&] { return (right - left) * log(right - left);}, 
        [&] {fork2([&] { sort(a, tmp, left, mid);},
                   [&] { sort(a, tmp, mid, right); }); 
             merge(a, tmp, left, mid, right);},
        [&] {std::sort(&a[left], &a[right]);}
  );
}

/*---------------------------------------------------------------------*/

void initialization() {
  pasl::util::ticks::set_ticks_per_seconds(1000);
//  local_constants.init(undefined);
  cmemcpy.initialize(1);
  cmergeroutine.initialize(1);
  cmerge.initialize(1);
  csort.initialize(1);
  execmode.init(dynidentifier<execmode_type>());
}

int main(int argc, char** argv) {
  int n = 0;
  int* a;
  int* tmp;
  int* b;
  std::string running_mode;
  bool check_mode;
  
  auto init = [&] {
    initialization();
    memcpy_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "memcpy_cutoff", 1000);
    merge_routine_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "merge_routine_cutoff", 1000); 
    merge_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "merge_cutoff", 1000);
    sort_cutoff_const = (long)pasl::util::cmdline::parse_or_default_int(
        "sort_cutoff", 1000);

    bool generator = true;
    std::string filename;
    generator = !(pasl::util::cmdline::exists("file"));
    
    int seed;
    if (!generator) { 
      filename = pasl::util::cmdline::parse_string("file"); 
      std::cout << "Read array from file " << filename << std::endl;
    } else {
      n = (long)pasl::util::cmdline::parse_or_default_int("n", 10);
      seed = pasl::util::cmdline::parse_or_default_int("seed", 239);
    }

    if (generator) {
      std::string generator_type = pasl::util::cmdline::parse_or_default_string(
            "gen", std::string("random"));
      std::cout << "Generate array with seed " 
                << seed 
                << " with length " 
                << n 
                << " with generator " << generator_type << std::endl;
        if (generator_type.compare("random") == 0) {
          a = pbbs::dataGen::random_array(n, seed);
        } else if (generator_type.compare("increasing") == 0) {
          a = pbbs::dataGen::increasing_array(n, seed);
        } else if (generator_type.compare("decreasing") == 0) {
          a = pbbs::dataGen::decreasing_array(n, seed);
        } else {
          std::cerr << "Wrong generator type " << generator_type << "\n";
          exit(-1);
        }
      } else {
        pbbs::_seq<int> seq = pbbs::benchIO::readIntArrayFromFile<int>(&filename[0]);
        n = (int)seq.n;
        a = seq.A;
      }
      tmp = pasl::data::mynew_array<int>(n);
      running_mode = pasl::util::cmdline::parse_or_default_string(
          "mode", std::string("by_force_sequential"));
      std::cout << "Using " << running_mode << " mode" << std::endl;cmemcpy.set(running_mode);
      cmergeroutine.set(running_mode);
      cmerge.set(running_mode);
      csort.set(running_mode);

      check_mode = pasl::util::cmdline::exists("check");
      if (check_mode) {
        b = pasl::data::mynew_array<int>(n);
        std::copy(a, a + n, b);
      }
  };
  auto run = [&] (bool sequential) {
    sort(a, tmp, 0, n);
  };

  auto output = [&] {
    if (check_mode) {
      std::sort(b, b + n);
      if (equals(a, b, n)) {
        std::cout << "Ok" << std::endl;
      } else {
        std::cout << "Wrong sort" << std::endl;
        print_array(a, n);
        print_array(b, n);
        exit(1);
      }
    } else {
      std::cout << "The evaluation have finished" << std::endl;
    }
  };
  auto destroy = [&] {
    free(a);
    if (check_mode)
      myfree(b);
    myfree(tmp);           
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}

/***********************************************************************/
