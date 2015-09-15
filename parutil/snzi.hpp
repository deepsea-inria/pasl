/*!
 * \file snzi.cpp
 * \brief Implementation of the Scalable Non-Zero Indicator
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 *
 * SNZI node
 * =========
 *
 * Each SNZI node stores a pointer to its parent SNZI node. The 
 * constructor for the node class (optionally) takes this
 * pointer as argument.
 
   node(node* parent = nullptr)
 
 * Only the root node in a SNZI tree should have a null parent
 * pointer. For any other SNZI node, the parent pointer should
 * point to another SNZI node.
 *
 * The following methods are provided by the SNZI class. The
 * call n.arrive() increments the counter in n by one, n.depart() 
 * decrements by one, and n.is_nonzero() returns true if the 
 * counter value is zero and false otherwise.
 
   void arrive();
   void depart();
   bool is_nonzero() const;
 
 * Calls to these three methods can occur concurrently. As specified
 * in the SNZI paper, the counter value of a SNZI node is not allowed
 * to be negative. As such, for any given SNZI node the number of
 * calls to the depart method should never outnumber the number of 
 * calls to arrive.
 *
 * In addition, this implementation provides a mechanism that can
 * be used by the client of the SNZI class to insert a word-sized
 * value into the root node of the SNZI tree. That is, the call
 * set_root_annotation(n, x) stores an item x into the
 * parent-pointer cell in the root node of the SNZI tree. The
 * call get_root_node(n) returns the value stored in the same cell
 * cell.
 
   template <class Item>
   static void set_root_annotation(node* n, Item x);
   template <class Item>
   static Item get_root_annotation(node* n);
 
 * The storage comes at no cost, since the parent pointer of the
 * root node otherwise stores a null pointer. The operations take
 * time in proportion to the path from the node pointed by n to
 * the root node.
 *
 * SNZI tree
 * =========
 *
 * Our SNZI tree builder can construct a tree of SNZI nodes
 * with specified branching factor and number of levels. The
 * constructor takes one argument for each of these parameters.
 
   tree(int branching_factor, int nb_levels)
 
 * The resulting tree is one where all leaf nodes occur at the same
 * level. Construction and destruction time are, of course, 
 * branching_factor^nb_levels. The tree builder assigns to
 * each leaf node in the SNZI tree an index starting from zero to
 * n-1, where n = the number of leaf nodes in the tree. The core 
 * interface of the tree builder consists of the following methods.
 
   int get_nb_leaf_nodes() const;
   node* ith_leaf_node(int i) const;
   bool is_nonzero() const;
 
 * The call t.get_nb_leaf_nodes() returns the number of leaf
 * nodes stored in the SNZI tree t. The call t.ith_leaf_node(i)
 * returns a pointer to the leaf node in the SNZI tree t at
 * poisition i. The result of this call is undefined if the
 * requested position is out of bounds. The call t.is_nonzero() 
 * returns true if the counter value represented in the tree is
 * nonzero and false otherwise.
 *
 * We have provided two additional convenience methods. The call
 * t.random_leaf_of(x) takes a value x that must be a word-sized
 * value of primitive type (e.g., long) and returns a leaf node
 * in the SNZI tree t that is selected pseudo randomly (by a
 * hash function) that is computed from the bits of x.
 
   template <class Item>
   node* random_leaf_of(Item x) const;
 
 * The call t.set_root_annotation(x) sets the annotation in the
 * root node to be the value x, which must be a value of primitive
 * type that can fit into a machine word.
 
   template <class Item>
   void set_root_annotation(Item x);
 
 *
 * Credits
 * =======
 *
 * The source code in this file is adapted from the algorithm described
 * in the following research paper.
 
 @inproceedings{ellen2007snzi,
 title={SNZI: Scalable nonzero indicators},
 author={Ellen, Faith and Lev, Yossi and Luchangco, Victor and Moir, Mark},
 booktitle={Proceedings of the twenty-sixth annual ACM symposium on Principles of distributed computing},
 pages={13--22},
 year={2007},
 organization={ACM}
 }
 
 *
 *
 */

#ifndef _PASL_PARUTIL_SNZI_H_
#define _PASL_PARUTIL_SNZI_H_

#include <atomic>

#include "tagged.hpp"

/***********************************************************************/

namespace pasl {
namespace data {
namespace snzi {
  
class node {
private:
  
  template <class T>
  static T* tagged_pointer_of(T* n) {
    return pasl::data::tagged::extract_value<T*>(n);
  }
  
  template <class T>
  static int tagged_tag_of(T* n) {
    return (int)pasl::data::tagged::extract_tag<long>(n);
  }
  
  template <class T>
  static T* tagged_tag_with(T* n, int t) {
    return pasl::data::tagged::create<T*, T*>(n, (long)t);
  }
  
  using contents_type = struct {
    int c; // counter value
    int v; // version number
  };
  
  static constexpr int one_half = -1;
  
  static constexpr int root_node_tag = 1;
  
  std::atomic<contents_type> X;
  
  node* parent;
  
  static bool is_root_node(const node* n) {
    return tagged_tag_of(n) == 1;
  }
  
  template <class Item>
  static node* create_root_node(Item x) {
    return (node*)tagged_tag_with(x, root_node_tag);
  }
  
public:
  
  node(node* _parent = nullptr) {
    parent = (_parent == nullptr) ? create_root_node(_parent) : _parent;
    contents_type init;
    init.c = 0;
    init.v = 0;
    X.store(init);
  }
  
  void arrive() {
    bool succ = false;
    int undo_arr = 0;
    while (! succ) {
      contents_type x = X.load();
      if (x.c >= 1) {
        contents_type orig = x;
        contents_type next = x;
        next.c++;
        succ = X.compare_exchange_strong(orig, next);
      }
      if (x.c == 0) {
        contents_type orig = x;
        contents_type next = x;
        next.c = one_half;
        next.v++;
        if (X.compare_exchange_strong(orig, next)) {
          succ = true;
          x.c = one_half;
          x.v++;
        }
      }
      if (x.c == one_half) {
        if (! is_root_node(parent)) {
          parent->arrive();
        }
        contents_type orig = x;
        contents_type next = x;
        next.c = 1;
        if (! X.compare_exchange_strong(orig, next)) {
          undo_arr++;
        }
      }
    }
    if (is_root_node(parent)) {
      return;
    }
    while (undo_arr > 0) {
      parent->depart();
      undo_arr--;
    }
  }
  
  // returns true if this call to depart caused a zero count, false otherwise
  bool depart() {
    while (true) {
      contents_type x = X.load();
      assert(x.c >= 1);
      contents_type orig = x;
      contents_type next = x;
      next.c--;
      if (X.compare_exchange_strong(orig, next)) {
        bool s = (x.c == 1);
        if (is_root_node(parent)) {
          return s;
        } else if (s) {
          return parent->depart();
        } else {
          return false;
        }
      }
    }
  }
  
  bool is_nonzero() const {
    return (X.load().c > 0);
  }
  
  template <class Item>
  static void set_root_annotation(node* n, Item x) {
    node* m = n;
    assert(! is_root_node(m));
    while (! is_root_node(m->parent)) {
      m = m->parent;
    }
    assert(is_root_node(m->parent));
    m->parent = create_root_node(x);
  }
  
  template <class Item>
  static Item get_root_annotation(node* n) {
    node* m = n;
    while (! is_root_node(m)) {
      m = m->parent;
    }
    assert(is_root_node(m));
    return (Item)tagged_pointer_of(m);
  }
  
};

class tree {
private:
  
  int branching_factor;
  int nb_levels;
  
  std::vector<node*> nodes;
  
  void build() {
    nodes.push_back(new node);
    for (int i = 1; i <= nb_levels; i++) {
      int e = (int)nodes.size();
      int s = e - std::pow(branching_factor, i - 1);
      for (int j = s; j < e; j++) {
        for (int k = 0; k < branching_factor; k++) {
          nodes.push_back(new node(nodes[j]));
        }
      }
    }
  }
  
  static unsigned int hashu(unsigned int a) {
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
  }
  
public:
  
  tree(int branching_factor, int nb_levels)
  : branching_factor(branching_factor), nb_levels(nb_levels) {
    build();
  }
  
  ~tree() {
    for (node* n : nodes) {
      delete n;
    }
  }
  
  int get_nb_leaf_nodes() const {
    return std::pow(branching_factor, nb_levels - 1);
  }
  
  node* ith_leaf_node(int i) const {
    assert((i >= 0) && (i < get_nb_leaf_nodes()));
    int n = (int)nodes.size();
    int j = n - (i + 1);
    return nodes[j];
  }
  
  template <class Item>
  node* random_leaf_of(Item x) const {
    union {
      Item x;
      long b;
    } bits;
    bits.x = x;
    int h = std::abs((int)hashu((unsigned int)bits.b));
    int n = get_nb_leaf_nodes();
    int l = h % n;
    return ith_leaf_node(l);
  }
  
  bool is_nonzero() const {
    return nodes[0]->is_nonzero();
  }
  
  template <class Item>
  void set_root_annotation(Item x) {
    node::set_root_annotation(nodes[0], x);
  }
  
};
  
} // end namespace
} // end namespace
} // end namespace

/***********************************************************************/


#endif /*! _PASL_PARUTIL_SNZI_H_ */