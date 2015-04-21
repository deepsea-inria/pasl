#include <iostream>
#ifndef _PASL_GRAPH_BASE_ALGO_H_
#define _PASL_GRAPH_BASE_ALGO_H_
	
/***********************************************************************/

namespace pasl {
  namespace graph {
    
    template <class Adjlist_seq>
    class base_algo {
    public: 
      virtual std::string get_impl_name(int index) = 0;
      virtual int get_impl_count() = 0;      
      virtual void print_res(int* res, int vertices, std::ofstream& to) = 0;      
      virtual int* get_dist(int algo_id, const adjlist<Adjlist_seq>& graph, int source) = 0;                  
    };
            
  } // end namespace graph
} // end namespace pasl

/***********************************************************************/

#endif /*! _PASL_GRAPH_BFS_H_ */
