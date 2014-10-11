#include <iostream>
#include <limits.h>
#include <vector>
#include <memory>

#include "benchmark.hpp"

/*---------------------------------------------------------------------*/

using value_type = long;

class array {
public:
  
  using value_type = value_type;
  
private:
  
  class Deleter {
  public:
    void operator()(value_type* ptr) {
      free(ptr);
    }
  };
  
  std::unique_ptr<value_type[], Deleter> ptr;
  long sz = -1l;
  
  void alloc() {
    value_type* p = (value_type*)malloc(sizeof(value_type) * sz);
    assert(p != nullptr);
    ptr.reset(p);
  }
  
  void check(long i) const {
    assert(ptr != nullptr);
    assert(i >= 0);
    assert(i < sz);
  }
  
public:
  
  array(long sz)
  : sz(sz) {
    alloc();
  }
  
  array(std::initializer_list<value_type> xs) {
    sz = xs.size();
    alloc();
    long i = 0;
    for (auto it = xs.begin(); it != xs.end(); it++)
      ptr[i++] = *it;
  }
  
  array(const array& other);
  array& operator=(const array& other);
  
  value_type& operator[](long i) {
    check(i);
    return ptr[i];
  }
  
  const value_type& operator[](long i) const {
    check(i);
    return ptr[i];
  }
  
  long size() const {
    return sz;
  }
  
};

using array_move = array&&;
using array_ref = array&;
using const_array_move = const array&&;
using const_array_ref = const array&;

std::ostream& operator<<(std::ostream& out, const_array_ref xs) {
  out << "[";
  size_t sz = xs.size();
  for (long i = 0; i < sz; i++) {
    out << xs[i];
    if (i+1 < sz)
      out << ", ";
  }
  out << "]";
  return out;
}

template< class T >
typename std::remove_reference<T>::type&& mv( T&& t ) {
  return std::move(t);
}

/*---------------------------------------------------------------------*/

auto identity_fct = [] (value_type x) {
  return x;
};

auto plus_fct = [] (value_type x, value_type y) {
  return x+y;
};

auto max_fct = [] (value_type x, value_type y) {
  return std::max(x, y);
};

auto min_fct = [] (value_type x, value_type y) {
  return std::min(x, y);
};

auto plus1_fct = [] (value_type x) {
  return plus_fct(x, 1);
};

auto incr_fct = [] (value_type& x) {
  x++;
};

auto is_even_fct = [] (value_type x) {
  return x % 2 == 0;
};

/*---------------------------------------------------------------------*/

template <class Func>
void iter(const Func& f, array_move xs) {
  for (long i = 0; i < xs.size(); i++)
    f(xs[i]);
}

template <class Func>
array map(const Func& f, const_array_move xs) {
  long sz = xs.size();
  array tmp = array(sz);
  for (long i = 0; i < sz; i++)
    tmp[i] = f(xs[i]);
  return tmp;
}

array take(const_array_move xs, long n) {
  assert(n <= xs.size());
  assert(n >= 0);
  array tmp = array(n);
  for (long i = 0; i < n; i++)
    tmp[i] = xs[i];
  return tmp;
}

array drop(const_array_move xs, long n) {
  long sz = xs.size();
  assert(n <= sz);
  assert(n >= 0);
  long m = sz-n;
  array tmp = array(m);
  for (long i = 0; i < m; i++)
    tmp[i] = xs[i+n];
  return tmp;
}

array copy(const_array_move xs) {
  return take(mv(xs), xs.size());
}

template <class Assoc_op, class Lift_func>
value_type reduce(const Assoc_op& op, const Lift_func& lift, value_type id, const_array_move xs) {
  value_type x = id;
  for (long i = 0; i < xs.size(); i++)
    x = op(x, lift(xs[i]));
  return x;
}

template <class Assoc_op>
value_type reduce(const Assoc_op& op, value_type id, const_array_move xs) {
  return reduce(op, identity_fct, id, mv(xs));
}

value_type sum(value_type id, const_array_move xs) {
  return reduce(plus_fct, id, mv(xs));
}

value_type sum(const_array_move xs) {
  return reduce(plus_fct, 0, mv(xs));
}

value_type max(const_array_move xs) {
  return reduce(max_fct, LONG_MIN, mv(xs));
}

value_type min(const_array_move xs) {
  return reduce(min_fct, LONG_MAX, mv(xs));
}

template <class Assoc_op, class Lift_func>
array scan(const Assoc_op& op, const Lift_func& lift, value_type id, const_array_move xs) {
  long sz = xs.size();
  value_type x = id;
  array tmp = array(sz);
  for (long i = 0; i < sz; i++) {
    tmp[i] = x;
    x = op(x, lift(xs[i]));
  }
  return tmp;
}

template <class Assoc_op>
array scan(const Assoc_op& op, value_type id, const_array_move xs) {
  return scan(op, identity_fct, id, mv(xs));
}

array partial_sums(value_type id, const_array_move xs) {
  return scan(plus_fct, identity_fct, id, mv(xs));
}

array partial_sums(const_array_move xs) {
  return scan(plus_fct, identity_fct, 0, mv(xs));
}

array pack(const_array_move flags, const_array_move xs) {
  array offsets = partial_sums(mv(flags));
  long n = xs.size();
  long last = n-1;
  value_type m = offsets[last] + flags[last];
  array result = array(m);
  for (long i = 0; i < n; i++)
    if (flags[i] == 1)
      result[offsets[i]] = xs[i];
  return result;
}

template <class Pred>
array filter(const Pred& p, const_array_move xs) {
  array flags = map(p, mv(xs));
  return pack(mv(flags), mv(xs));
}

array just_evens(const_array_move xs) {
  return filter(is_even_fct, mv(xs));
}

/*---------------------------------------------------------------------*/

array duplicate(const_array_move xs) {
  array tmp(xs.size());
  return tmp;
}

array ktimes(const_array_move xs) {
  array tmp(xs.size());
  return tmp;
}

template <class Pred, class Assoc_op, class Lift_func>
value_type filter_reduce(const Pred& p, const Assoc_op& op, const Lift_func& lift, value_type id, const_array_move xs) {
  return id;
}

template <class Pred, class Func>
array filter_map(const Pred& p, const Func& f, const_array_move xs) {
  array tmp(xs.size());
  return tmp;
}

/*---------------------------------------------------------------------*/

const value_type open_paren = 1;
const value_type close_paren = 0;

value_type p(char c) {
  assert(c == '(' || c == ')');
  value_type v = open_paren;
  if (c == ')')
    v = close_paren;
  return v;
}

char u(value_type v) {
  assert(v == open_paren || v == close_paren);
  char c = '(';
  if (v == close_paren)
    c = ')';
  return c;
}

array from_parens(std::string str) {
  long sz = str.size();
  array tmp = array(sz);
  for (long i = 0; i < sz; i++)
    tmp[i] = p(str[i]);
  return tmp;
}

std::string to_parens(const_array_move xs) {
  long sz = xs.size();
  std::string str(sz, 'x');
  for (long i = 0; i < sz; i++)
    str[i] = u(xs[i]);
  return str;
}

bool matching_parens(const_array_move xs) {
  return true;
}

/*---------------------------------------------------------------------*/

void doit() {
  array xs = { 0, 1, 2, 3, 4, 5, 6 };
  iter(incr_fct, mv(xs));
  std::cout << "xs=" << xs << std::endl;
  array ys = map(plus1_fct, mv(xs));
  std::cout << "xs(copy)=" << copy(mv(xs)) << std::endl;
  std::cout << "ys=" << ys << std::endl;
  value_type v = sum(mv(ys));
  std::cout << "v=" << v << std::endl;
  array zs = partial_sums(mv(xs));
  std::cout << "zs=" << zs << std::endl;
  std::cout << "max=" << max(mv(ys)) << std::endl;
  std::cout << "min=" << min(mv(ys)) << std::endl;
  std::cout << "tmp=" << map(plus1_fct, array({100, 101})) << std::endl;
  std::cout << "evens=" << just_evens(mv(ys)) << std::endl;
  
  std::cout << "take=" << take(mv(xs), 3) << std::endl;
  std::cout << "drop=" << drop(mv(xs), 4) << std::endl;
  
  std::cout << "parens=" << to_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "matching=" << matching_parens(from_parens("()()((()))")) << std::endl;
  std::cout << "not_matching=" << matching_parens(from_parens("()(((()))")) << std::endl;

  std::cout << "empty=" << array({}) << std::endl;
  
}

int main(int argc, char** argv) {

  auto init = [&] {

  };
  auto run = [&] (bool) {
    doit();
  };
  auto output = [&] {
  };
  auto destroy = [&] {
    ;
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}
