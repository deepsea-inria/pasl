/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * Producer-consumer buffers (built on top of linked lists)
 *
 */

#ifndef _PCB_H_
#define _PCB_H_

#include <atomic>

namespace pasl {
namespace data {
/**
 * \defgroup pcb Producer-consumer buffer
 * \brief Concurrent single producer, single consumer buffer
 * @ingroup data_structures
 * @{
 *
 */
namespace pcb {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/**
 * \class signature
 * \ingroup pcb
 * \brief PCB interface
 */
template <typename Item>
class signature {
public:
  virtual bool empty() = 0;
  //! Pushes item `m` on the PCB
  virtual void push(Item m) = 0;
  /*! \brief Pops an item from the PCB.
   *  \param m place to write the popped item
   */
  virtual void pop(Item& m) = 0;
  /*! \brief Tries to pop from the PCB.
   *  \param m place to write the popped item
   *  \return true if an item is popped and false otherwise.
   */
  virtual bool try_pop(Item& m) = 0;    
};

/*---------------------------------------------------------------------*/
/**
 * \class linked
 * \ingroup pcb
 * \brief PCB implemented with a singly-linked list
 */
template <typename Item>
class linked : public pcb::signature<Item> {
  
private:
  struct item_s {
    std::atomic<Item> msg;
    std::atomic<struct item_s*> next;
  };
  
  typedef struct item_s item_t;
  typedef item_t* item_p;
  
  item_p head;
  item_p tail;

  item_p item_create() {
    item_p item = new item_t;
    item->next.store(NULL);
    return item;
  }
 
public:
  
  void init() {
    item_p tail_item = item_create();
    head = tail_item;
    tail = tail_item;
  }

  void destroy() {
    // pop any remaining items off the PC
    Item m;
    while(try_pop(m));
    delete head;
    head = NULL;
  }
  
  linked() { }
  
  linked(const linked& src) {
    init();
  }

  bool empty() {
    return head->next == NULL;
  }
  
  void push(Item m) {
    assert(false);
    /*
    item_p item = item_create();
    tail->msg.store(m);
    // need store-store barrier
    tail->next.store(item);
    tail = item;
    */
  }
  
  void pop(Item& m) {
    /*
   assert(! empty());
    m = head->msg.load();
    item_p tmp = head;
    head = head->next.load();
    delete tmp;
    */
  }
  
  bool try_pop(Item& m) {
    if (empty()) {
      return false;
    } else {
      pop(m);
      return true;
    }
  }
};
  
  
/***********************************************************************/
  
} // end namespace
/*! @} */
} // end namespace
} // end namespace

#endif /*! _PCB_H_ */

