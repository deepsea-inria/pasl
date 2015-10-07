/*!
 * \file parallel-array-map
 * \brief Parallel Array Map
 * \example parallel-array-map
 * \date 2014
 * \copyright COPYRIGHT (c) 2012 Umut Acar, Arthur Chargueraud,
 * Michael Rainey. All rights reserved.
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
 * Implementation: Array map
 *
 * Given $n$ this file creates an array with $n$ integers from $0$ to
 * $n-1$ and then reads them in parallel to compute their sum.
 *
 * This code is used to assess the effectiveness of file map
 * implemented in parallel-file-map.cpp
 *
 */


#include <math.h>
#include <fstream>
#include "benchmark.hpp"

using namespace std;

/***********************************************************************/

namespace par = pasl::sched::native;

long cutoff = 0;


/*---------------------------------------------------------------------*/
void fill_array (int* &in_array, int n) 
{
  for (int i=0; i<n; ++i) {
	  in_array[i] = i;
  };
  return;
}


static double seq_array_map (int*  in_array, int n)
{
  double sum = 0;
  int m = 0;

  for (int i = 0; i < n; ++i) {
    m = in_array[i];
    sum += m;
    cout << "i = " << i << " m = " << m << endl;
  }
 
  return sum;
    
}


static double par_array_map_rec (int* &in_array, int n, int i, int j)
{

  double sum = 0.0;
	
  if ( j-i < cutoff) {
    for (; i < j; ++i) {
      sum += in_array[i];
//      cout << "i = " << i << " m = " << in_array[i] << endl;
		}
  	return sum;

	}
	else {
    int mid = (i+j)/2;
    double a, b, s;
		par::fork2([&] // [&a,&f,n,i,j,mid,block_size]
							 { a = par_array_map_rec (in_array, n, i, mid); },
               [&] // [&b,&f,n,i,j,mid,block_size]
							 { b = par_array_map_rec (in_array, n, mid, j); });		

    sum = a + b;
//    cout << "sum = " << s;
    return sum;
	}		
}//par_array_map_rec

static int par_array_map (int* &in_array, int n)
{
	double sum = par_array_map_rec (in_array, n, 0, n);
	return sum; 

}//par_file_map

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  double result = 0.0;
  int n = 0;
    
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
    int* in_array = new int[n];
	
    fill_array (in_array, n);
    result = par_array_map (in_array, n);

  };    

  auto output = [&] {
      cout << "result " << result << endl;
  };

  auto destroy = [&] {
        ;
  };
    
  pasl::sched::launch(argc, argv, init, run, output, destroy);
    
  return 0;
}

/***********************************************************************/
