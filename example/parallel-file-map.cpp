/*!
 * \file tree-contract.cpp
 * \brief Parallel tree contraction
 * \example tree-contract.cpp
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
 * Implementation: File map
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

long filesize(const char* file_name)
{
  ifstream file (file_name, ios::binary |  ios::ate);
  ifstream::pos_type n = file.tellg();
    file.close();
    return (long) n;
}


void create_file (const string file_name, int n) 
{
  ofstream out_file;
  out_file.open (file_name, ios::binary);

  for (int i=0; i<n; ++i) {
	  out_file.write ((char *) &i, sizeof(int));
  }
 
  out_file.close ();
  return;
}


static int seq_file_map (ifstream &f, int n)
{
  int block_size = sizeof (int);  
  char block[4];
  int sum = 0;
  int m = 0;

  for (int i = 0; i < n*block_size; i += block_size) {
    f.seekg (i, ios::beg);        
    f.read (block, block_size); 
    m = (int) *block;
    sum += m;
//    cout << "i = " << i << " m = " << m << endl;
  }
 
  return sum;
    
}


static int par_file_map_rec (ifstream &f, int n, int block_size, int i, int j)
{


  if ( j-i <= 1) {
    char block[4]; 

		// this is buggy of course because the file is shared.
    f.seekg (i * block_size, ios::beg);        
    f.read (block, block_size); 
    int m = (int) *block;
    cout << "i = " << i << " j = " << j << " m = " << m << endl;

		return m; 
	}
	else {
    int mid = (i+j)/2;
    int a, b;
		par::fork2([&] // [&a,&f,n,i,j,mid,block_size]
							 { a = par_file_map_rec (f, n, block_size, i, mid); },
               [&] // [&b,&f,n,i,j,mid,block_size]
							 { b = par_file_map_rec (f, n, block_size, mid, j); });		

    return (a + b);
	}		
}//par_file_map_rec

static int par_file_map (ifstream &f, int n)
{
  int block_size = sizeof (int);  
  char block[4];
  int sum = 0;
  int m = 0;

	sum = par_file_map_rec (f, n, block_size, 0, n); 
	return sum; 
}//par_file_map

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  int result = 0;
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
    string file_name = "input.dat";
    ifstream in_file;
	
    create_file (file_name, n);

    in_file.open (file_name, ios::binary);
    int sum = par_file_map (in_file, n);
    
    result = sum;
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
