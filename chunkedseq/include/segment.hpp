/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Memory segment
 * \file segment.hpp
 *
 */

#ifndef _PASL_DATA_SEGMENT_H_
#define _PASL_DATA_SEGMENT_H_

namespace pasl {
namespace data {

/***********************************************************************/

/*---------------------------------------------------------------------*/
/*! \brief Segment descriptor
 *
 * A segment consists of a pointer along with a right-open interval
 * that describes a contiguous region of memory surrounding the 
 * pointer.
 *
 * Invariant:
 * `begin <= middle < end`
 *
 */
//! [segment]
template <class Pointer>
class segment {
public:
  
  using pointer_type = Pointer;
  
  // points to the first cell of the interval
  pointer_type begin;
  // points to a cell contained in the interval
  pointer_type middle;
  // points to the cell that is one cell past the last cell of interval
  pointer_type end;
  
  segment()
  : begin(nullptr), middle(nullptr), end(nullptr) { }
  
  segment(pointer_type begin, pointer_type middle, pointer_type end)
  : begin(begin), middle(middle), end(end) { }
  
};
//! [segment]
  
/*---------------------------------------------------------------------*/

/*! \brief Returns a segment that contains pointer `p` in a given ringbuffer
 *
 * \param p pointer on an item in the ringbuffer
 * \param fr pointer on the first item
 * \param bk pointer on the last item
 * \param a pointer on the first cell of the array
 * \param capacity size in number of items of the array
 */
template <class Pointer>
segment<Pointer> segment_of_ringbuffer(Pointer p, Pointer fr, Pointer bk, Pointer a, int capacity) {
  segment<Pointer> seg;
  assert(p >= a);
  assert(p < a + capacity);
  seg.middle = p;
  if (fr <= bk) { // no wraparound
    seg.begin = fr;
    seg.end = bk + 1;
  } else if (p >= fr) { // p points into first segment
    seg.begin = fr;
    seg.end = a + capacity;
  } else { // p points into second segment
    assert(p <= bk);
    seg.begin = a;
    seg.end = bk + 1;
  }
  assert(seg.begin <= p);
  assert(seg.middle == p);
  assert(p < seg.end);
  return seg;
}

template <class Item>
segment<const Item*> make_const_segment(segment<Item*> seg) {
  segment<const Item*> res;
  res.begin = seg.begin;
  res.middle = seg.middle;
  res.end = seg.end;
  return res;
}

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PASL_DATA_SEGMENT_H_ */