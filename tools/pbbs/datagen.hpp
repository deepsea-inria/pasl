



#include "atomic.hpp"
#include "utils.hpp"

#ifndef _PBBS_DATAGEN_H_
#define _PBBS_DATAGEN_H_

namespace pbbs {
  
  namespace dataGen {
    
    using namespace std;
    
#define HASH_MAX_INT ((unsigned) 1 << 31)
    
    //#define HASH_MAX_LONG ((unsigned long) 1 << 63)
    
    template <class T> T hash(int i);
    
    template <>
    int hash<int>(int i) {
      return utils::hash(i) & (HASH_MAX_INT-1);}
    
    template <>
    unsigned int hash<unsigned int>(int i) {
      return utils::hash(i);}
    
    template <>
    double hash<double>(int i) {
      return ((double) hash<int>(i)/((double) HASH_MAX_INT));}
    
    template <>
    long hash<long>(int) {
      pasl::util::atomic::die("bogus");
      return 0;
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
