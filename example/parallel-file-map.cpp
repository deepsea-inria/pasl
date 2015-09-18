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
 * Given $n$ this file creates a file with $n$ integers 
 * from $0$ to $n-1$ and then reads them in parallel to compute 
 * their sum.  It uses a spin lock to ensure atomic access
 * to the file.
 *
 * The effect that I (Umut) would like to see was how the program
 * behaves when end up getting blocked for I/O.  I am not sure
 * if this program demonstrates the issue because the lock will
 * be held by only one processor, causing essentially a serialization
 * of all file accesses anyway.  More thinking is needed...
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
  // turn off file buffering
  out_file.rdbuf()->pubsetbuf(0, 0);
	
  for (int i=0; i<n; ++i) {
	  out_file.write ((char *) &i, sizeof(int));
  };
 
  out_file.flush ();
  out_file.close ();
  return;
}


static double seq_file_map (ifstream &f, int n)
{
  int block_size = sizeof (int);  
  char block[4];
  double sum = 0;
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


class spin_lock {
private:
  atomic<bool> held;

public:
  spin_lock () {
		held.store (false);
  }
	
	void spin_to_lock () {
		while (true) {
			bool current = held.load();
			if (!current) {
				if (held.compare_exchange_weak(current,true))
					break;
			}
		}
	}//spin_to_lock

	void release () {
		while (true) {
			bool current = held.load();
			assert (current==true);
			if (held.compare_exchange_weak(current,false))
					break;
		}
	}	
};//class lock


static int par_file_map_rec_locked (ifstream &f, int n, spin_lock &f_lock, int block_size, int i, int j)
{

  if ( j-i <= cutoff) {
    char block[4];

    // begin read: take the file lock, read, and release.
    f_lock.spin_to_lock ();     				
    f.seekg (i * block_size, ios::beg);        
    f.read (block, block_size);
		f_lock.release ();
    // end read.		
    int m = (int) *block;
//    cout << "i = " << i << " j = " << j << " m = " << m << endl;
		return m; 
	}
	else {
    int mid = (i+j)/2;
    int a, b;
		par::fork2([&] // [&a,&f,n,i,j,mid,block_size]
							 { a = par_file_map_rec_locked (f, n, f_lock, block_size, i, mid); },
               [&] // [&b,&f,n,i,j,mid,block_size]
							 { b = par_file_map_rec_locked (f, n, f_lock, block_size, mid, j); });		

    return (a + b);
	}		
}//par_file_map_rec

static int par_file_map_locked (string file_name, int n)
{
  ifstream in_file;
  spin_lock f_lock;
  int block_size = sizeof (int);  
  char block[4];
  double sum = 0.0;
  int m = 0;

	
  in_file.open (file_name, ios::binary);	
  // turn off file buffering
  in_file.rdbuf()->pubsetbuf(0, 0);


	sum = par_file_map_rec_locked (in_file, n, f_lock, block_size, 0, n);
	return sum; 

}//par_file_map_locked

/*---------------------------------------------------------------------*/

static long seq_fib (long n){
  if (n < 2)
    return n;
  else
    return seq_fib (n - 1) + seq_fib (n - 2);
}

static long par_fib(long n) {
  if (n <= 20 || n < 2)
    return seq_fib(n);
  long a, b;
  par::fork2([n, &a] { a = par_fib(n-1); },
             [n, &b] { b = par_fib(n-2); });
  return a + b;
}

/*---------------------------------------------------------------------*/


static double g (int* data, int start, int end)
{
  double sum = 0.0;

  if (end-start < cutoff) {
//    sum = par_fib (30);
    for (int i = start; i < end; ++i) {
      sum = sum + (int) data[i];
    }
//    cout << "x";
  }
  else {
    int mid = (start + end) / 2;
    double a = 0.0;
    double b = 0.0;

		par::fork2([&] 
							 { a = g (data, start, mid); },
               [&] 
            	 { b = g (data, mid, end); });		

//    cout << "|";
    sum = a + b ;    
  };

  return sum;

}// g



// Assume that block_size = 4 and convert it into integers for addition.
static double g_seq (int* data, int start, int end)
{
  double sum = 0.0;
	
  for (int i = start; i < end; ++i) {
    int d = (int) data[i];
//		cout << "d = " << d << endl;
    sum = sum + d;
  };

//	cout << "sum = " << sum << endl;
	return sum;
}// g


static double par_file_map_rec (string file_name, int n, int block_size, int i, int j)
{

  if ( j-i <= cutoff*cutoff) {
    int m = j-i;            // how many blocks do we have.
    char* data = new char[block_size*m]; 
    ifstream f;
		
    // begin read: open file, seek and read.
    f.open (file_name, ios::binary);
		if (f.is_open ()) {
      // successfully opened, nothing to do.
		}
		else {
      // failed to open.
			cout << "FATAL ERROR: FAILED TO OPEN FILE: " << file_name << endl;
		};

    // turn off file buffering
    f.rdbuf()->pubsetbuf(0, 0);
		
    f.seekg (i * block_size, ios::beg);        
    f.read (data, block_size*m);
		f.close ();
    // end read.
    double s = g ((int*) data, 0, m);		
  	return s;
	}
	else {
    int mid = (i+j)/2;
    double a, b, s;
		par::fork2([&] // [&a,&f,n,i,j,mid,block_size]
							 { a = par_file_map_rec (file_name, n, block_size, i, mid); },
               [&] // [&b,&f,n,i,j,mid,block_size]
							 { b = par_file_map_rec (file_name, n, block_size, mid, j); });		

    s = a + b;
//    cout << "s = " << s;
    return s;
	}		
}//par_file_map_rec

static int par_file_map (string file_name, int n)
{
  int block_size = sizeof (int);  
  char block[4];
  double sum = 0.0;
  int m = 0;
	
	sum = par_file_map_rec (file_name, n, block_size, 0, n);
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
    string file_name = "input.dat";
	
    create_file (file_name, n);
    result = par_file_map (file_name, n);

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
