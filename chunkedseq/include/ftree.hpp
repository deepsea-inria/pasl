/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Hinze & Patterson's 2-3 finger tree
 * \file ftree.hpp
 *
 */

#include "fixedcapacity.hpp"

#ifndef _PASL_DATA_FTREE_H_
#define _PASL_DATA_FTREE_H_

namespace pasl {
namespace data {
namespace ftree {
    
/***********************************************************************/

template <
  class Top_item_base,  // use "foo" to obtain a sequence of "foo*" items
  int Chunk_capacity,
  class Cached_measure,
  class Top_item_deleter = chunkedseq::Pointer_deleter, // provides: static void dealloc(foo* x)
  class Top_item_copier = chunkedseq::Pointer_deep_copier, // provides: static void copy(foo* x)
  template <
    class Item,
    int Capacity,
    class Item_alloc
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptr,
  class Size_access=chunkedseq::itemsearch::no_size_access
>
class ftree {
public:
  
  using cache_type = Cached_measure;
  using measured_type = typename cache_type::measured_type;
  using algebra_type = typename cache_type::algebra_type;
  using measure_type = typename cache_type::measure_type;
  
  class digit;
  class node;
  class branch_node;

  class leaf_node;

  struct split_type {
    ftree* fr;
    leaf_node* middle;
    ftree* bk;
  };
  
  using node_p = node*;
  using leaf_node_p = leaf_node*;
  using ftree_p = ftree*;
  using branch_node_p = branch_node*;
  
  using size_type = size_t;
  using leaf_node_type = leaf_node;
  using leaf_item_type = Top_item_base*;
  
  /*---------------------------------------------------------------------*/
  /* 2-3 tree node */
  
  class node {
  private:
    
    // note: uninitialized_tag is only needed for debug mode
    typedef enum {
      leaf_node_tag,
      branch_node_tag,
      uninitialized_tag
    } tag_t;
    
    tag_t tag;
    
  protected:
    
    node()
    : tag(uninitialized_tag) { }
    
    node(tag_t _tag)
    : tag(_tag) { }
    
    node(const node& other)
    : tag(other.tag) { }
    
    virtual ~node() { }
    
    bool is_leaf() const {
      return tag == leaf_node_tag;
    }
    
    bool is_branch() const {
      return tag == branch_node_tag;
    }
    
    void set_tag(tag_t _tag) {
      assert(tag == uninitialized_tag);
      tag = _tag;
    }
    
    void make_leaf() {
      set_tag(leaf_node_tag);
    }
    
    void make_branch() {
      set_tag(branch_node_tag);
    }
    
    virtual measured_type get_cached() const = 0;
    
    template <class Pred>
    static const leaf_node_type* down(const node* t,
                                      const Pred& p,
                                measured_type& prefix);
    
    virtual void clear() = 0;
    
    friend class ftree;
  };
  
  /*---------------------------------------------------------------------*/
  /* Leaf */
  
public:
  
  class leaf_node : public node {
  private:
    
    typedef leaf_node* leaf_node_p;
    
    static leaf_node_p force(node_p t) {
      assert(t != NULL);
      assert(t->is_leaf());
      return (leaf_node_p)t;
    }
    
    static const leaf_node_type* cforce(const node* t) {
      assert(t != NULL);
      assert(t->is_leaf());
      return (const leaf_node_type*)t;
    }
    
  public:
    
    mutable leaf_item_type item;
    
    leaf_node() : node() {
      node::make_leaf();
    }
    
    leaf_node(const leaf_item_type& leaf)
    : node(), item(leaf) {
      node::make_leaf();
    }
    
    leaf_node(const leaf_node& other)
    : node(other), item(other.item) { }
    
    measured_type get_cached() const {
      measure_type meas_fct;
      measured_type m = meas_fct(item);
      return m;
    }
    
    template <class Body>
    void for_each(const Body& body) const {
      body(item);
    }
    
    void clear() {
      
    }
    
    friend class ftree;
  };
  
  /*---------------------------------------------------------------------*/
  /* Branch */
  
public:
  
  class branch_node : public node {
  private:
    
    typedef branch_node* branch_node_p;
    
    static const int max_nb_branches = 3;
    
    int nb;    // number of branches (== 2 or 3)
    node_p branches[max_nb_branches];
    measured_type cached;
    
    static branch_node_p force(node_p t) {
      assert(t->is_branch());
      return (branch_node_p)t;
    }
    
    static const branch_node* cforce(const node* t) {
      assert(t->is_branch());
      return (const branch_node*)t;
    }
    
    void set_branch(int id, node_p t) {
      branches[id] = t;
    }
    
    void refresh_cache() {
      cached = algebra_type::identity();
      for (int i = 0; i < nb; i++)
        cached = algebra_type::combine(cached, branches[i]->get_cached());
    }
    
  public:
    
    branch_node(node_p t0, node_p t1)
    : node(), nb(2) {
      node::make_branch();
      set_branch(0, t0);
      set_branch(1, t1);
      refresh_cache();
    }
    
    branch_node(node_p t0, node_p t1, node_p t2)
    : node(), nb(3) {
      node::make_branch();
      set_branch(0, t0);
      set_branch(1, t1);
      set_branch(2, t2);
      refresh_cache();
    }
    
    branch_node(const branch_node& other)
    : node(other), nb(other.nb) {
      for (int i = 0; i < other.nb_branches(); i++)
        set_branch(i, make_deep_copy_tree(other.branches[i]));
      refresh_cache();
    }
    
    int nb_branches() const {
      return nb;
    }
    
    node_p get_branch(int i) const {
      return branches[i];
    }
    
    measured_type get_cached() const {
      return cached;
    }
    
    void clear() {
      for (int i = 0; i < nb; i++)
        delete branches[i];
    }
    
    template <class Body>
    void for_each(const Body& body) const {
      for (int i = 0; i < nb; i++)
        node_for_each(body, branches[i]);
    }
    
    friend class ftree;
  };
  
  static node* make_deep_copy_tree(node* n) {
    node* new_node;
    if (n->is_leaf()) {
      leaf_node_type* new_leaf = new leaf_node(*leaf_node::force(n));
      new_node = new_leaf;
    } else {
      new_node = new branch_node(*branch_node::force(n));
    }
    return new_node;
  }
  
  template <class Body>
  static void node_for_each(const Body& body, const node* n) {
    if (n->is_leaf()) {
      const leaf_node_type* l = leaf_node::cforce(n);
      l->for_each(body);
    } else {
      const branch_node* b = branch_node::cforce(n);
      b->for_each(body);
    }
  }

  /*---------------------------------------------------------------------*/
  /* Digit */
  
  class digit {
  private:
    
    typedef node* node_p;
    typedef branch_node* branch_node_p;
    typedef leaf_node* leaf_node_p;
    typedef digit* digit_p;
    static const int max_nb_digits = 4;
    using buffer_type = fixedcapacity::heap_allocated::ringbuffer_ptr<node_p, max_nb_digits>;
    
    buffer_type d;
    
  public:
    
    digit() { }
    
    size_type size() const {
      return d.size();
    }
    
    bool empty() const {
      return d.empty();
    }
    
    node_p back() const {
      return d.back();
    }
    
    node_p front() const {
      return d.front();
    }
    
    void push_back(node_p x) {
      d.push_back(x);
    }
    
    void push_front(node_p x) {
      d.push_front(x);
    }
    
    void pop_back() {
      d.pop_back();
    }
    
    void pop_front() {
      d.pop_front();
    }
    
    void pop_front(int nb) {
      for (int i = 0; i < nb; i++)
        pop_front();
    }
    
    void pop_back(int nb) {
      for (int i = 0; i < nb; i++)
        pop_back();
    }
    
    node_p operator[](size_type ix) const {
      assert(ix >= 0 && ix < d.size());
      return d[ix];
    }
    
    void clear() {
      size_type sz = size();
      for (size_type i = 0; i < sz; i++) {
        d[i]->clear();
        delete d[i];
      }
      d.clear();
    }
    
    bool is_one() const {
      return size() == 1;
    }
    
    bool is_four() const {
      return size() == 4;
    }
    
    void make_deep_copy(digit& dst) const {
      for (size_type i = 0; i < size(); i++)
        dst.push_back(make_deep_copy_tree(d[i]));
    }
    
    /*
     * Assigns new contents to the digit container,
     * replacing current items with those of the
     * branches of the given tree
     */
    void assign(branch_node_p x) {
      clear();
      for (int i = 0; i < x->nb_branches(); i++)
        push_back(x->get_branch(i));
    }
    
    branch_node_p front_three() const {
      assert(is_four());
      return new branch_node(d[0], d[1], d[2]);
    }
    
    branch_node_p back_three() const {
      assert(is_four());
      return new branch_node(d[1], d[2], d[3]);
    }
    
    /*
     * Returns the index of the first tree t in d
     * for which p(t.cached+i) holds
     */
    template <typename Pred>
    size_type find(const Pred& p, measured_type i) const {
      size_type ix;
      measured_type j = algebra_type::identity();
      j = algebra_type::combine(j, i);
      for (ix = 0; ix < size(); ix++) {
        j = algebra_type::combine(j, d[ix]->get_cached());
        if (p(j))
          break;
      }
      return ix;
    }
    
    static digit concat3(digit d1, digit d2, digit d3) {
      static constexpr int max_concat_nb_digits = max_nb_digits*3;
      using tmp_buffer_type = fixedcapacity::heap_allocated::ringbuffer_ptr<node_p, max_concat_nb_digits>;
      tmp_buffer_type tmp;
      digit digits[3] = {d1, d2, d3};
      for (int k = 0; k < 3; k++) {
        digit d = digits[k];
        for (size_type i = 0; i < d.size(); i++)
          tmp.push_back(d[i]);
      }
      digit res;
      size_type sz = tmp.size();
      for (size_type i = 0; i < sz; ) {
        size_type m = sz - i;
        if (m == 2) {
          res.push_back(new branch_node(tmp[i], tmp[i+1]));
        } else if (m == 3) {
          res.push_back(new branch_node(tmp[i], tmp[i+1], tmp[i+2]));
        } else if (m == 4) {
          res.push_back(new branch_node(tmp[i], tmp[i+1]));
          res.push_back(new branch_node(tmp[i+2], tmp[i+3]));
        } else {
          res.push_back(new branch_node(tmp[i], tmp[i+1], tmp[i+2]));
          m = 3;
        }
        i += m;
      }
      return res;
    }
    
    measured_type get_cached() const {
      measured_type cached = algebra_type::identity();
      for (size_type i = 0; i < size(); i++)
        cached = algebra_type::combine(cached, d[i]->get_cached());
      return cached;
    }
    
    template <class Pred>
    static const node* down(const Pred& p, const digit* d, measured_type& prefix) {
      measured_type v = prefix;
      node_p n = nullptr;
      for (size_type i = 0; i < d->size(); i++) {
        n = (*d)[i];
        v = algebra_type::combine(v, n->get_cached());
        if (p(v))
          return n;
        prefix = v;
      }
      return n;
    }
    
    template <class Body>
    void for_each(const Body& body) const {
      for (int i = 0; i < size(); i++)
        node_for_each(body, d[i]);
    }
    
    void swap(digit& other) {
      d.swap(other.d);
    }
    
  };
 
  /*---------------------------------------------------------------------*/
  /* Finger tree */
  
public:
  
  measured_type cached;
  digit fr;
  ftree_p middle;
  digit bk;
  
  bool single() const {
    return fr.is_one() && bk.empty();
  }
  
  bool deep() const {
    return ! (empty() || single());
  }
  
  void refresh_cache() {
    cached = algebra_type::identity();
    cached = algebra_type::combine(cached, fr.get_cached());
    if (deep())
      cached = algebra_type::combine(cached, middle->get_cached());
    cached = algebra_type::combine(cached, bk.get_cached());
  }
  
  void initialize() {
    refresh_cache();
  }
  
  ftree_p take_middle() {
    ftree_p m = middle;
    middle = NULL;
    return m;
  }

  void make_deep_copy(const ftree& other) {
    other.fr.make_deep_copy(fr);
    middle = NULL;
    if (other.deep())
      middle = new ftree(*other.middle);
    other.bk.make_deep_copy(bk);
    initialize();
  }
  
  ftree(digit fr, ftree_p middle, digit bk)
  : fr(fr), middle(middle), bk(bk) {
    initialize();
  }
  
  ftree(digit d) {
    size_type sz = d.size();
    if (sz == 0) {
      ;
    } else if (sz == 1) {
      fr.push_front(d[0]);
    } else if (sz == 2) {
      fr.push_front(d[0]);
      bk.push_back(d[1]);
    } else if (sz == 3) {
      fr.push_front(d[1]);
      fr.push_front(d[0]);
      bk.push_back(d[2]);
    } else {
      assert(sz == 4);
      fr.push_front(d[2]);
      fr.push_front(d[1]);
      fr.push_front(d[0]);
      bk.push_back(d[3]);
    }
    if (sz > 1)
      middle = new ftree();
    else
      middle = NULL;
    initialize();
  }
  
  bool empty() const {
    return fr.empty();
  }
  
  node_p _back() const {
    assert(! empty());
    if (single())
      return fr.front();
    return bk.back();
  }
  
  node_p _front() const {
    assert(! empty());
    return fr.front();
  }
  
  void _push_back(node_p x) {
    if (empty()) {
      fr.push_front(x);
    } else if (single()) {
      middle = new ftree();
      bk.push_back(x);
    } else {
      if (bk.is_four()) {
        branch_node_p y = bk.front_three();
        bk.pop_front(3);
        bk.push_back(x);
        middle->_push_back(y);
      } else {
        bk.push_back(x);
      }
    }
    cached = algebra_type::combine(cached, x->get_cached());
  }
  
  void _push_front(node_p x) {
    if (empty()) {
      fr.push_front(x);
      cached = algebra_type::combine(cached, x->get_cached());
      return;
    } else if (single()) {
      middle = new ftree();
      bk.push_back(fr.front());
      fr.pop_front();
      fr.push_front(x);
    } else {
      if (fr.is_four()) {
        branch_node_p y = fr.back_three();
        fr.pop_back(3);
        fr.push_front(x);
        middle->_push_front(y);
      } else {
        fr.push_front(x);
      }
    }
    cached = algebra_type::combine(x->get_cached(), cached);
  }
  
  void _pop_back() {
    assert(! empty());
    if (algebra_type::has_inverse)
      cached = algebra_type::combine(cached, algebra_type::inverse(_back()->get_cached()));
    if (single()) {
      fr.pop_front();
      assert(empty());
    } else { // deep
      bk.pop_back();
      if (bk.empty()) {
        if (middle->empty()) {
          if (fr.is_one()) {
            delete middle;
          } else {
            node_p x = fr.back();
            fr.pop_back();
            bk.push_back(x);
          }
        } else {
          branch_node_p x = branch_node::force(middle->_back());
          middle->_pop_back();
          bk.assign(x);
          delete x;
        }
      }
    }
    if (! algebra_type::has_inverse)
      refresh_cache();
  }
  
  void _pop_front() {
    assert(! empty());
    if (algebra_type::has_inverse)
      cached = algebra_type::combine(algebra_type::inverse(_front()->get_cached()), cached);
    if (single()) {
      fr.pop_front();
      assert(empty());
    } else { // deep
      fr.pop_front();
      if (fr.empty()) {
        if (middle->empty()) {
          fr.push_front(bk.front());
          bk.pop_front();
          if (bk.empty())
            delete middle;
        } else {
          branch_node_p x = branch_node::force(middle->_front());
          middle->_pop_front();
          fr.assign(x);
          delete x;
        }
      }
    }
    if (! algebra_type::has_inverse)
      refresh_cache();
  }
  
  void _push_back(digit d) {
    for (size_type i = 0; i < d.size(); i++)
      _push_back(d[i]);
  }
  
  void _push_front(digit d) {
    for (int i = (int)d.size() - 1; i >= 0; i--)
      _push_front(d[i]);
  }
  
  static ftree_p app3(ftree_p fr, digit m, ftree_p bk) {
    ftree_p r;
    if (fr->empty()) {
      r = bk;
      r->_push_front(m);
      delete fr;
    } else if (bk->empty()) {
      r = fr;
      r->_push_back(m);
      delete bk;
    } else if (fr->single()) {
      r = bk;
      node_p x = fr->_back();
      r->_push_front(m);
      r->_push_front(x);
      delete fr;
    } else if (bk->single()) {
      r = fr;
      node_p x = bk->_back();
      r->_push_back(m);
      r->_push_back(x);
      delete bk;
    } else {
      digit m2 = digit::concat3(fr->bk, m, bk->fr);
      ftree_p n = app3(fr->take_middle(), m2, bk->take_middle());
      r = new ftree(fr->fr, n, bk->bk);
      delete fr;
      delete bk;
    }
    return r;
  }
  
  bool is_front(const digit* d) const {
    return d == &fr;
  }
  
  bool is_back(const digit* d) const {
    return d == &bk;
  }
  
  typedef struct {
    ftree_p fr;       //!< front
    node_p middle;    //!< middle
    ftree_p bk;       //!< back
  } split_rec_type;
  
  template <typename Pred>
  static split_rec_type split_rec(const Pred& p, measured_type i, ftree_p f) {
    assert(! f->empty());
    split_rec_type v;
    if (f->single()) {
      v.fr = new ftree();
      v.middle = f->_back();
      v.bk = new ftree();
      delete f;
    } else { // deep
      measured_type vfr = algebra_type::identity();
      vfr = algebra_type::combine(vfr, i);
      vfr = algebra_type::combine(vfr, f->fr.get_cached());
      size_type sz;
      size_type ix;
      if (p(vfr)) {
        ix = f->fr.find(p, i);
        sz = f->fr.size();
        v.fr = new ftree(f->fr);
        v.middle = f->fr[ix];
        v.bk = f;
      } else {
        measured_type vm = algebra_type::identity();
        vm = algebra_type::combine(vm, vfr);
        vm = algebra_type::combine(vm, f->middle->get_cached());
        if (p(vm)) {
          split_rec_type ms = split_rec(p, vfr, f->take_middle());
          digit xs;
          xs.assign(branch_node::force(ms.middle));
          delete ms.middle;
          measured_type tmp = algebra_type::identity();
          tmp = algebra_type::combine(tmp, vfr);
          tmp = algebra_type::combine(tmp, ms.fr->get_cached());
          ix = xs.find(p, tmp);
          sz = xs.size();
          v.fr = new ftree(f->fr, ms.fr, xs);
          v.middle = xs[ix];
          v.bk = new ftree(xs, ms.bk, f->bk);
          delete f;
        } else {
          ix = f->bk.find(p, vm);
          sz = f->bk.size();
          v.fr = f;
          v.middle = f->bk[ix];
          v.bk = new ftree(f->bk);
        }
      }
      for (size_type i = 0; i < sz - ix; i++)
        v.fr->_pop_back();
      for (size_type i = 0; i < ix + 1; i++)
        v.bk->_pop_front();
    }
    return v;
  }
  
  template <class Pred>
  static const digit* down(const ftree* ft, const Pred& p, measured_type& prefix) {
    assert(! ft->empty());
    if (ft->single())
      return &(ft->fr);
    assert(ft->deep());
    measured_type v = prefix;
    v = algebra_type::combine(v, ft->fr.get_cached());
    if (p(v))
      return &(ft->fr);
    prefix = v;
    v = algebra_type::combine(v, ft->middle->get_cached());
    if (p(v))
      return down(ft->middle, p, prefix);
    prefix = v;
    v = algebra_type::combine(v, ft->bk.get_cached());
    if (p(v))
      return &(ft->bk);
    assert(ft->bk.size() > 0);
    return &(ft->bk);
  }
  
  leaf_node_type* __back() const {
    return leaf_node::force(_back());
  }
  
  leaf_node_type* __front() const {
    return leaf_node::force(_front());
  }
  
  template <class Body>
  void _for_each(const Body& body) const {
    if (empty())
      return;
    if (single()) {
      fr.for_each(body);
      return;
    }
    fr.for_each(body);
    middle->_for_each(body);
    bk.for_each(body);
  }
  
  void ___push_back(leaf_node_type* x) {
    _push_back(x);
  }
  
  void ___push_front(leaf_node_type* x) {
    _push_front(x);
  }
  
  void ___pop_back() {
    assert(_back()->is_leaf());
    __back();
    _pop_back();
  }
  
  void ___pop_front() {
    assert(_front()->is_leaf());
    __front();
    _pop_front();
  }
  
  template <class Pred>
  static const leaf_node* search_aux(const Pred& p, const ftree* start, measured_type& prefix) {
    const digit* d = down(start, p, prefix);
    const node* n = digit::down(p, d, prefix);
    return leaf_node::down(n, p, prefix);
  }
  
  
  template <typename Pred>
  static split_type split_aux(const Pred& p, measured_type prefix, ftree* f) {
    split_rec_type s1 = split_rec(p, prefix, f);
    leaf_node_p l = leaf_node::force(s1.middle);
    split_type s2 = {s1.fr, l, s1.bk};
    return s2;
  }
  
  static ftree* concatenate(ftree* fr, ftree* bk) {
    digit d;
    return app3(fr, d, bk);
  }
  
public:
  
  /*---------------------------------------------------------------------*/
  
  ftree()
  : middle(NULL) {
    initialize();
  }
  
  ftree(const ftree& other) {
    make_deep_copy(other);
  }
  
  ~ftree() {
    if (middle != NULL && deep())
      delete middle;
  }
  
  measured_type get_cached() const {
    return cached;
  }
  
};
  
template <
class Top_item_base,
int Chunk_capacity,
class Cached_measure,
class Top_item_deleter,
class Top_item_copier,
template<class Item, int Capacity, class Item_alloc> class Chunk_struct,
class Size_access
>
template <class Pred>
const typename ftree< Top_item_base,
Chunk_capacity,
Cached_measure,
Top_item_deleter,
Top_item_copier,
Chunk_struct,
Size_access>::leaf_node_type*
ftree< Top_item_base,
Chunk_capacity,
Cached_measure,
Top_item_deleter,
Top_item_copier,
Chunk_struct,
Size_access>::node::down(const node* t,
                         const Pred& p,
                         measured_type& prefix) {
  if (t->is_leaf())
    return ftree< Top_item_base,
    Chunk_capacity,
    Cached_measure,
    Top_item_deleter,
    Top_item_copier,
    Chunk_struct,
    Size_access>::leaf_node::cforce(t);
  const branch_node* b = branch_node::cforce(t);
  int nb = b->nb_branches();
  for (int i = 0; i < nb; i++) {
    node_p s = b->get_branch(i);
    measured_type v = prefix;
    v = algebra_type::combine(v, s->get_cached());
    if (p(v))
      return down(s, p, prefix);
    prefix = v;
  }
  return down(b->get_branch(nb-1), p, prefix);
}
  
/*---------------------------------------------------------------------*/

template <
  class Top_item_base,  // use "foo" to obtain a sequence of "foo*" items
  int Chunk_capacity,
  class Cached_measure,
  class Top_item_deleter = chunkedseq::Pointer_deleter, // provides: static void dealloc(foo* x)
  class Top_item_copier = chunkedseq::Pointer_deep_copier, // provides: static void copy(foo* x)
  template <
    class Item,
    int Capacity,
    class Item_alloc
  >
  class Chunk_struct = fixedcapacity::heap_allocated::ringbuffer_ptr,
  class Size_access=chunkedseq::itemsearch::no_size_access
>
class tftree {
public:
  
  using ftree_type = ftree<Top_item_base, Chunk_capacity, Cached_measure,
                           Top_item_deleter, Top_item_copier, Chunk_struct,
                           Size_access>;
  
  using split_type = typename ftree_type::split_type;
  using leaf_node_type = typename ftree_type::leaf_node_type;
  using measured_type = typename ftree_type::measured_type;
  using leaf_item_type = typename ftree_type::leaf_item_type;
  using leaf_node = typename ftree_type::leaf_node;
  using algebra_type = typename ftree_type::algebra_type;
  
  ftree_type* ft;
  
  tftree() {
    ft = new ftree_type();
  }
  
  tftree(const tftree& other) {
    ft = new ftree_type(*(other.ft));
  }
  
  ~tftree() {
    delete ft;
  }
  
  bool empty() const {
    return ft->empty();
  }
  
  leaf_item_type back() const {
    return ft->__back()->item;
  }
  
  leaf_item_type cback() const {
    return back();
  }
  
  leaf_item_type front() const {
    return ft->__front()->item;
  }
  
  template <class M>
  void push_back(M, leaf_item_type item) {
    ft->___push_back(new leaf_node(item));
  }
  
  template <class M>
  void push_front(M, leaf_item_type item) {
    ft->___push_front(new leaf_node(item));
  }
  
  template <class M>
  leaf_item_type pop_back(M) {
    leaf_node_type* p = ft->__back();
    ft->___pop_back();
    leaf_item_type item = p->item;
    delete p;
    return item;
  }
  
  template <class M>
  leaf_item_type pop_front(M) {
    leaf_node_type* p = ft->__front();
    ft->___pop_front();
    leaf_item_type item = p->item;
    delete p;
    return item;
  }
  
  measured_type get_cached() const {
    return ft->get_cached();
  }
  
  template <class Body>
  void for_each(const Body& body) const {
    auto _body = [&] (leaf_item_type item) {
      body(item);
    };
    ft->_for_each(_body);
  }
  
  void swap(tftree& other) {
    std::swap(ft, other.ft);
  }
  
  template <class Pred>
  measured_type search_for_chunk(const Pred& p, measured_type prefix,
                                 const Top_item_base*& item) const {
    measured_type pr = prefix;
    const leaf_node* ptr = ft->search_aux(p, ft, pr);
    item = ptr->item;
    return pr;
  }
  
  template <class M, class Pred>
  measured_type split(M, const Pred& p, measured_type prefix,
                      leaf_item_type& item, tftree& other) {
    if (empty())
      return prefix;
    measured_type pr = prefix;
    split_type r = ft->split_aux(p, pr, ft);
    ft = r.fr;
    delete other.ft;
    other.ft = r.bk;
    item = r.middle->item;
    delete r.middle;
    return algebra_type::combine(prefix, ft->get_cached());
  }
  
  template <class M>
  void concat(M, tftree& other) {
    ft = ft->concatenate(ft, other.ft);
    other.ft = new ftree_type();
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_DATA_FTREE_H_ */