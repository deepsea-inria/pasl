/*!
 * \file tree-contract.cpp
 * \brief Parallel tree contraction
 * \example tree-contract.cpp
 * \date 2014
 * \copyright COPYRIGHT (c) 2012 Umut Acar, Vitaly Aksenov, Arthur Chargueraud, 
 * Michael Rainey, and Sam Westrick. All rights reserved.
 *
 * \license This project is released under the GNU Public License.
 *
 * Arguments:
 * ==================================================================
 *   - `-n <int>` (default=3)
 *     ?
 *   - `-cutoff <int>` (default=20)
 *      ?
 *
 * Implementation: Miller-Reif algorithm.
 *
 */


#include <math.h>
#include "benchmark.hpp"

/***********************************************************************/

namespace par = pasl::sched::native;

long cutoff = 0;


/*---------------------------------------------------------------------*/


template<std::size_t Max_Degree>
class Node {
  int id; 
  int degree; 
  bool is_live;    
  int edges[Max_Degree];
  int INVALID_EDGE = -1;

  Node (int i) {
    id = i;
    degree = 0;
    is_live = true;

    for (j = 0; j < Max_Degree; ++j) { 
      edges[j] = INVALID_EDGE;
    }
  } // Node

  // TODO: Parallel unsafe  
  insert_edge (int j) {
    for (i = 0; i < Max_Degree; i++) {
      if (edges[i] == INVALID_EDGE) { 
        edges[i] = j;
        degree++;
        break;
      }
    }

    if (i = Max_Degree) {
      cout << "Error: Edge Insertion Failed\n";
    }
  }

  // TODO: Parallel unsafe
  delete_edge (int j) {
    i = 0;
    while (i < degree && edges[i] != j && edges[i] == INVALID_EDGE) { ++i; };

    if (i == degree) {
      cout <<  "Error: Edge not found\n";
    }  
    else {
      edges[i] = INVALID_EDGE;
      degree--; 
    }
  }

}; // Node


template<std::size_t Max_Degree>
class Forest {
  int max_degree = Max_Degree;
  int num_nodes;

  Node<Max_Degree> *nodes;

  Forest (int n) {
    num_nodes = n;
    nodes = new Node[n];
    
    for (int i = 0; i < n; ++i) {
      Node(i,0);
    }
  }; // Forest 

  insert_edge (int i, int j) {
    nodes[i].insert_edge(j);
  };

  delete_edge (int i, int j) {
    nodes[i].delete_edge(j);
  };


}; // Forest 


static rake (F,v) {
  delete node v from Forest (and all its edges)
}

static compress (F,v) {
  delete node v from Forest (and all its edges)
}

static seq_node_contract (forest F, int round) 
{
  fn v => {
    switch (F.degree(v)) {
      case 0: break;
    case 1: 
      {node u = F.neighbor(v);
	if (F.degree(u) > 1) {
          rake (F,v); 
          break;
        }
	else  if (F.id(u) > F.id(v)) {
          break;
        }
        else {
          rake (F,v);
          break;
        } 
      }// case 1
    case 2: 
      {pair<node,node> p = F.neighbors_2(v);
       u = p.first;
       w = p.second;
       if (F.degree(u) == 1 || F.degree(w) == 1) {
	 break;
        }
       else {
         bool c = hash(u,round) == Tail && hash(v,round) == Head &&  hash(w,round) == Tail)
       if (c)  {
         compress (F,v);
         break;
       }
       else {
         break;
        }
      }//case 2

    }//switch    

  }//return value

}//seq_node_contract



static forest seq_tree_contract (forest F, int round, finalizer)
{
  if F.numEdges () == 0 {
      F.map (finalizer);
    }
  else {
    FF = F.copy ();
    FF.map(seq_node_contract(round));
    seq_tree_contract (FF, round+1, finalizer);
  } 
}//seq_tree_contract

/*---------------------------------------------------------------------*/

static long par_fib(long n) {
  if (n <= cutoff || n < 2)
    return seq_fib(n);
  long a, b;
  par::fork2([n, &a] { a = par_fib(n-1); },
             [n, &b] { b = par_fib(n-2); });
  return a + b;
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  long result = 0;
  long n = 0;

  /* The call to `launch` creates an instance of the PASL runtime and
   * then runs a few given functions in order. Specifically, the call
   * to launch calls our local functions in order:
   *
   *          init(); run(); output(); destroy();
   *
   * Each of these functions are allowed to call internal PASL
   * functions, such as `fork2`. Note, however, that it is not safe to
   * call such PASL library functions outside of the PASL environment.
   *
   * After the calls to the local functions all complete, the PASL
   * runtime reports among other things, the execution time of the
   * call `run();`.
   */

  auto init = [&] {
    cutoff = (long)pasl::util::cmdline::parse_or_default_int("cutoff", 25);
    n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);
  };

  auto run = [&] (bool sequential) {
    // TODO: Don't we need a case here? 
    result = par_tree_contract (n);
  };

  auto output = [&] {
    std::cout << "result " << result << std::endl;
  };

  auto destroy = [&] {
    ;
  };

  pasl::sched::launch(argc, argv, init, run, output, destroy);

  return 0;
}

/***********************************************************************/
