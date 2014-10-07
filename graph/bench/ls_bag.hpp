/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2013 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Port of Leiserson and Shardl's bag data structure
 * \file ls_bag.hpp
 *
 * The code in this file was mostly copied from code provided by
 * Tao Shardl. For the original source code, see the following link:
 * http://web.mit.edu/~neboat/www/code.html
 */

#ifndef _PASL_LS_BAG_H_
#define _PASL_LS_BAG_H_

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

const int BAG_SIZE = 64;
/* be careful, changing this value to 512 caused pbfs to crash on
   certain inputs (e.g., square_grid; 1000000 edges)
 */
const int BLK_SIZE = 128;

namespace pasl {
namespace data {

/***********************************************************************/
  
template <typename T> class Bag;

template <typename T>
class Pennant
{
private:
  T* els;
  Pennant<T> *l, *r;
  
public:
  Pennant() {
    this->els = new T[BLK_SIZE];
    this->l = NULL;
    this->r = NULL;
  }
  ~Pennant() {
    if (els != NULL) {
      delete[] els;
      els = NULL;
    }
  }
  
  inline const T* const getElements() {
    return this->els;
  }
  inline Pennant<T>* getLeft() const {
    return l;
  }
  inline Pennant<T>* getRight() const {
    return r;
  }
  
  inline void clearChildren() {
    l = NULL;
    r = NULL;
  }
  
  inline Pennant<T>* combine(Pennant<T>* that) {
    that->r = this->l;
    this->l = that;
    
    return this;
  }
  inline Pennant<T>* split() {
    Pennant<T>* that;
    
    that = this->l;
    this->l = that->r;
    that->r = NULL;
    
    return that;
  }
  
  void destroy() {
    assert(els != NULL);
    delete [] els;
    els = NULL;
    if (l != NULL)
      l->destroy();
    if (r != NULL)
      r->destroy();
    clearChildren();
    delete this;
  }
  
  template <class Body>
  inline void for_each(const Body& body, int size) const {
    assert(els != NULL);
    if (getLeft() != NULL)
      getLeft()->for_each(body, BLK_SIZE);
    if (getRight() != NULL)
      getRight()->for_each(body, BLK_SIZE);
    for (int i = 0; i < size; i++)
      body(els[i]);
  }
  
  friend class Bag<T>;
};

template <typename T>
class Bag
{
private:
  // fill points to the bag entry one position beyond the MSP
  int fill;
  Pennant<T>* *bag;
  Pennant<T>* filling;
  // size points to the filling array entry on position
  // beyond last valid element.
  int _size;
  
public:
  Bag() : fill(0), _size(0)
  {
    this->bag = new Pennant<T>*[BAG_SIZE];
    this->filling = new Pennant<T>();
  }
  Bag(Bag<T> *that) : fill(that->fill), _size(that->_size)
  {
    this->bag = new Pennant<T>*[BAG_SIZE];
    for (int i = 0; i < BAG_SIZE; i++)
      this->bag[i] = that->bag[i];
    
    this->filling = that->filling;
  }
  
  ~Bag()
  {
    if (this->bag != NULL) {
      for (int i = 0; i < this->fill; i++) {
        if (this->bag[i] != NULL) {
	  //	  this->bag[i]->destroy();
	  delete this->bag[i];
          this->bag[i] = NULL;
        }
      }
      
      delete[] this->bag;
    }
    
    delete this->filling;
  }
  
  inline void insert(Pennant<T>* c) {
    int i = 0;
    
    do {
      switch(i < this->fill && this->bag[i] != NULL) {
        case false:
          this->bag[i] = c;
          this->fill = this->fill > i+1 ? this->fill : i+1;
          return;
        case true:
          c = this->bag[i]->combine(c);
          this->bag[i] = NULL;
          break;
        default: break;
      }
      
      ++i;
      
    } while (i < BAG_SIZE);
    
    this->fill = BAG_SIZE;

  }
  
  inline void insert(T el) {
    int sz = this->_size;
    if (sz < BLK_SIZE) {
      this->filling->els[sz] = el;
      this->_size++;
      return;
    }
    
    Pennant<T> *c = this->filling;
    this->filling = new Pennant<T>();
    this->filling->els[0] = el;
    this->_size = 1;
    insert(c);
  }
  void merge(Bag<T>* that) {
    Pennant<T> *c = NULL;
    char x;
    int i;
    
    // Deal with the partially-filled Pennants
    if (this->_size < that->_size) {
      i = this->_size - (BLK_SIZE - that->_size);
      
      if (i >= 0) {
        
        memcpy(that->filling->els + that->_size,
               this->filling->els + i,
               (BLK_SIZE - that->_size) * sizeof(T));
        
        c = that->filling;
        this->_size = i;
      } else {
        
        if (this->_size > 0)
          memcpy(that->filling->els + that->_size,
                 this->filling->els,
                 this->_size * sizeof(T));
        
        delete this->filling;
        
        this->filling = that->filling;
        this->_size += that->_size;
      }
      
    } else {
      i = that->_size - (BLK_SIZE - this->_size);
      
      if (i >= 0) {
        memcpy(this->filling->els + this->_size,
               that->filling->els + i,
               (BLK_SIZE - this->_size) * sizeof(T));
        
        c = this->filling;
        this->filling = that->filling;
        this->_size = i;
      } else {
        
        if (that->_size > 0)
          memcpy(this->filling->els + this->_size,
                 that->filling->els,
                 that->_size * sizeof(T));
        
        this->_size += that->_size;
      }
    }
    
    that->filling = NULL;
    
    // Update this->fill (assuming no final carry)
    int min, max;
    if (this->fill > that->fill) {
      min = that->fill;
      max = this->fill;
    } else {
      min = this->fill;
      max = that->fill;
    }
    
    // Merge
    for (i = 0; i < min; ++i) {
      
      x =
      (this->bag[i] != NULL) << 2 |
      (i < that->fill && that->bag[i] != NULL) << 1 |
      (c != NULL);
      
      switch(x) {
        case 0x0:
          this->bag[i] = NULL;
          c = NULL;
          that->bag[i] = NULL;
          break;
        case 0x1:
          this->bag[i] = c;
          c = NULL;
          that->bag[i] = NULL;
          break;
        case 0x2:
          this->bag[i] = that->bag[i];
          that->bag[i] = NULL;
          c = NULL;
          break;
        case 0x3:
          c = that->bag[i]->combine(c);
          that->bag[i] = NULL;
          this->bag[i] = NULL;
          break;
        case 0x4:
          /* this->bag[i] = this->bag[i]; */
          c = NULL;
          that->bag[i] = NULL;
          break;
        case 0x5:
          c = this->bag[i]->combine(c);
          this->bag[i] = NULL;
          that->bag[i] = NULL;
          break;
        case 0x6:
          c = this->bag[i]->combine(that->bag[i]);
          that->bag[i] = NULL;
          this->bag[i] = NULL;
          break;
        case 0x7:
          /* this->bag[i] = this->bag[i]; */
          c = that->bag[i]->combine(c);
          that->bag[i] = NULL;
          break;
        default: break;
      }
    }
    
    that->fill = 0;
    
    if (this->fill == max) {
      if (c == NULL)
        return;
      
      do {
        switch(i < max && this->bag[i] != NULL) {
          case false:
            this->bag[i] = c;
            this->fill = max > i+1 ? max : i+1;
            return;
          case true:
            c = this->bag[i]->combine(c);
            this->bag[i] = NULL;
            break;
          default: break;
        }
        
        ++i;
        
      } while (i < BAG_SIZE);
    } else { // that->fill == max
      int j;
      if (c == NULL) {
        this->fill = max;
        for (j = i; j < this->fill; ++j) {
          this->bag[j] = that->bag[j];
          that->bag[j] = NULL;
        }
        return;
      }
      
      do {
        switch(i < max && that->bag[i] != NULL) {
          case false:
            this->bag[i] = c;
            this->fill = max > i+1 ? max : i+1;
            for (j = i+1; j < this->fill; ++j) {
              this->bag[j] = that->bag[j];
              that->bag[j] = NULL;
            }
            return;
          case true:
            c = that->bag[i]->combine(c);
            this->bag[i] = NULL;
            that->bag[i] = NULL;
            break;
          default: break;
        }
        
        ++i;
        
      } while (i < BAG_SIZE);
    }
    
    this->fill = BAG_SIZE;
  }
  inline int split(Pennant<T>** p) {
    if (this->fill == 0) {
      *p = NULL;
      return 0;
    }
    
    this->fill--;
    int pos = this->fill;
    *p = this->bag[this->fill];
    this->bag[this->fill] = NULL;
    
    for (this->fill; this->fill > 0; this->fill--) {
      if (this->bag[this->fill-1] != NULL)
        break;
    }
    return pos;
  }
  int split(Pennant<T>** p, int pos) {
    if (pos < 0 || this->fill <= pos) {
      *p = NULL;
      return this->fill - 1;
    }
    
    *p = this->bag[pos];
    
    for (int i = pos-1; i >= 0; i--) {
      if (this->bag[i] != NULL)
        return i;
    }
    
    return -1;
  }
  
  inline int numElements() const {
    int count = this->_size;
    int k = 1;
    for (int i = 0; i < this->fill; i++) {
      if (this->bag[i] != NULL)
        count += k*BLK_SIZE;
      k = k * 2;
    }
    
    return count;
  }
  inline int getFill() const {
    return this->fill;
  }
  inline bool isEmpty() const {
    return this->fill == 0 && this->_size == 0;
  }
  inline Pennant<T>* getFirst() const {
    return this->bag[0];
  }
  inline Pennant<T>* getFilling() const {
    return this->filling;
  }
  inline int getFillingSize() const {
    return this->_size;
  }

  
  typedef Bag<T> self_type;
  typedef Bag<T> container_t;
  typedef T value_type;
  typedef size_t size_type;
  
  Bag(size_type nb, value_type v) {
    assert(false);
  }
  
  inline bool empty() const {
    return isEmpty();
  }
  
  inline size_type size() const {
    return (size_type)numElements();
  }
  
  inline void push_back(const value_type& x) {
    insert(x);
  }
  
  void push_front(const value_type& x) {
    assert(false);
  }
  
  value_type pop_back() {
    assert(false);
    value_type x;
    return x;
  }
  
  value_type pop_front() {
    assert(false);
    value_type x;
    return x;
  }
  
  class iterator {
  public:
    iterator() {
    }
    iterator(size_type ix) : ix(ix) {
    }
    size_type ix;
    value_type operator*() {
      assert(false);
      value_type v;
      return v;
    }
    
    iterator operator+(const size_type n) const {
      return 0;
    }
    
    iterator& operator++() {
      assert(false);
      iterator it;
      return it;
    }
    
    iterator operator++(int) {
      assert(false);
      iterator it;
      return it;
    }
  };
  
  iterator begin() {
    return iterator(0);
  }
  iterator end() {
    return iterator(size() - 1);
  }
  
  iterator insert(iterator pos, const value_type& val) {
    assert(false);
  }
  
  void clear() {
    for (int i = 0; i < fill; i++) {
      if (this->bag[i] != NULL) {
        this->bag[i]->destroy();
        this->bag[i] = NULL;
      }
    }
    this->fill = 0;
    this->_size = 0;
  }
  
  void transfer_from_back_to_front_by_position(self_type& dst, iterator foo) {
    assert(false);
  }
  
  // need to assume that dst is empty
  void split_approximate(self_type& dst) {
    size_type sz = size();
    int r = getFill();
    Pennant<T>* saved = NULL;
    assert(dst.size() == 0);
    
    for (int i = 0; i < BAG_SIZE; i++)
      dst.bag[i] = NULL;
    if (this->fill > 0 && this->bag[0] != NULL) {
      saved = this->bag[0];
      this->bag[0] = NULL;
      if (this->fill == 1)
        this->fill = 0;
    }
    for (int k = 1; k < r; k++) { // traversing the spine
      if (this->bag[k] != NULL) {
        dst.bag[k-1] = NULL;
        //	assert(dst.bag[k-1] == NULL);
        dst.bag[k - 1] = this->bag[k]->split();
        this->bag[k - 1] = this->bag[k];
        this->bag[k] = NULL;
      }
    }
    this->fill = r - 1;
    dst.fill = r - 1;
    /*
     for (int i = r; i < BAG_SIZE; i++) {
     this->bag[i] = NULL;
     dst.bag[i] = NULL;
     } */
    if (saved != NULL)
      insert(saved);
    assert(this->filling != NULL);
    assert(this->filling->els != NULL);
    assert(dst.filling != NULL);
    assert(dst.filling->els != NULL);
    size_type sz2 = size();
    assert(sz2 < sz);
  }
  
  void concat(self_type& other) {
    merge(&other);
    other._size = 0;
    for (int i = 0; i < other.fill; i++) {
      if (other.bag[i] != NULL) {
        //	delete other.bag[i];
        other.bag[i] = NULL;
      }
    }
    other.fill = 0;
    assert(other.size() == 0);
    assert(other.filling == NULL);
    other.filling = new Pennant<T>();
    
    assert(this->filling != NULL);
    assert(this->filling->els != NULL);
    assert(other.filling != NULL);
    assert(other.filling->els != NULL);
    /*
     for (int i = 0; i < BAG_SIZE; i++)
     other.bag[i] = NULL;
     */
  }
  
  void pushn_back(value_type* vs, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(vs[i]);
  }
  void pushn_back(const value_type v, size_t nb) {
    for (size_t i = 0; i < nb; i++)
      push_back(v);
  }
  void popn_back(value_type* vs, size_t nb) {
    assert(false);
  }
  value_type& operator[](int ix) const {
    assert(false);
    value_type x;
    return x;
  }
  value_type& operator[](size_t ix) const {
    assert(false);
    value_type x;
    return x;  }
  
  void swap(self_type& other) {
    std::swap(fill, other.fill);
    std::swap(bag, other.bag);
    std::swap(filling, other.filling);
    std::swap(_size, other._size);
  }
  
  template <class Body>
  inline void for_each(const Body& body) const {
    getFilling()->for_each(body, _size);
    for (int i = 0; i < getFill(); i++)
      if (this->bag[i] != NULL)
        this->bag[i]->for_each(body, BLK_SIZE);
  }

};
  
/***********************************************************************/

}
}

#endif /*! _PASL_LS_BAG_H_ */
