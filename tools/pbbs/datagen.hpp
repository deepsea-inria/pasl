



#include "atomic.hpp"
#include "utils.hpp"

#ifndef _PBBS1_DATAGEN_H_
#define _PBBS1_DATAGEN_H_

namespace pbbs {
  
  namespace dataGen {
    
    using namespace std;
    
#define HASH_MAX_INT ((unsigned) 1 << 31)
    
    //#define HASH_MAX_LONG ((unsigned long) 1 << 63)
    
    static inline int hashi(int i) {
      return utils::hash(i) & (HASH_MAX_INT-1);}
    
    static inline unsigned int hashu(int i) {
      return utils::hash(i);}
    
    static inline double hashd(int i) {
      return ((double) hashi(i)/((double) HASH_MAX_INT));}
    
    template <class T> T hash(int i) {
      if (typeid(T) == typeid(int)) {
        return hashi(i);
      } else if (typeid(T) == typeid(unsigned int)) {
        return hashu(i);
      } else if (typeid(T) == typeid(double)) {
        return hashd(i);
      } else {
        pasl::util::atomic::die("bogus");
        return 0;
      }
    }
    
  
    /* template <class T> T hash(long i); */
    
    /*
    template <>
    long hash<long>(long i) {
      return utils::hash(i) & (HASH_MAX_LONG-1);}
    */
    
    /* template <> */
    /* int hash<int>(long i) { */
    /*   return utils::hash(i) & (HASH_MAX_INT-1);} */
    
    /* template <> */
    /* unsigned int hash<unsigned int>(long i) { */
    /*   return utils::hash(i);} */
    
    /* template <> */
    /* double hash<double>(long i) { */
    /*   return ((double) hash<long>(i)/((double) HASH_MAX_INT));} */
    
  };

  
} // end namespace

#endif /*! _PBBS_DATAGEN_H_ */
