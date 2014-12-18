/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file hash.hpp
 * \brief Hash function
 *
 */


#ifndef _MINICOURSE_HASH_H_
#define _MINICOURSE_HASH_H_

/***********************************************************************/

// taken from https://gist.github.com/badboy/6267743

long hash64shift(long key) {
  auto unsigned_right_shift = [] (long x, int y) {
    unsigned long r = (unsigned long) x >> y;
    return (long) r;
  };
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (unsigned_right_shift(key, 24));
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (unsigned_right_shift(key, 14));
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (unsigned_right_shift(key, 28));
  key = key + (key << 31);
  return key;
}

long hash_signed(long key) {
  return hash64shift(key);
}

unsigned long hash_unsigned(long key) {
  return (unsigned long)hash_signed(key);
}

unsigned long random_index(long key, long n) {
  unsigned long x = (unsigned long)hash64shift(key);
  return x % n;
}

int log2_up(unsigned long i) {
  int a=0;
  long b=i-1;
  while (b > 0) {b = b >> 1; a++;}
  return a;
}

/***********************************************************************/

#endif /*! _MINICOURSE_HASH_H_ */