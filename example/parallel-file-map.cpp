/*!
 * \file parallel-file-map.cpp
 * \brief Parallel File Map
 * \example parallel-file-map.cpp
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
 * Given $n$ this file creates a file with $n$ integers from $0$ to
 * $n-1$ and then reads them in parallel to compute their sum.  For
 * the parallel reduction, the algorithm divides the file into blocks
 * of size "cutoff*cutoff" and reads and reduces them in parallel.
 *
 * Note that the reduction is also done in parallel with the specified
 * cutoff.
 *
 * One factor here is the "thrashing effect" though work-stealing
 * should approximate quite well the order in which the file is read.
 *
 * The other factor is that when a thread is waiting for a chunk to be
 * served, it does not release the processor.  This is the real effect
 * that we are after, because it will prevent parallel reductions to
 * be done effectively.  This effect should be most noticable when the
 * file size is large.
 *
 *
 * Here is a run wuth buffering
 ../pbench/prun speedup -baseline "./parallel-file-map.opt -proc 1" -parallel "./parallel-file-map.opt -proc 1,2,4,8" -n 100000000 -cutoff 100
[1/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 4.377
[2/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 4.197
[3/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 2
exectime 3.633
[4/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 4
exectime 3.332
[5/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 8
exectime 5.119
 *
 *
 * Here is the same run without buffering.
 
 ../pbench/prun speedup -baseline "./parallel-file-map.opt -proc 1" -parallel "./parallel-file-map.opt -proc 1,2,4,8" -n 100000000 -cutoff 100
./_build/opt/compile.sh -o _build/opt/parallel-file-map.o -c ./parallel-file-map.cpp
=================
g++ -std=c++11 -DNDEBUG -O2 -fno-optimize-sibling-calls -DDISABLE_INTERRUPTS -DSTATS_IDLE -D_GNU_SOURCE -Wfatal-errors -m64 -DTARGET_X86_64 -DTARGET_MAC_OS -pthread -DHAVE_GCC_TLS -I . -I ../tools/build//../../sequtil -I ../tools/build//../../parutil -I ../tools/build//../../sched -I ../tools/build//../../tools/pbbs -I ../tools/build//../../tools/malloc_count -I _build/opt $* _build/opt/parallel-file-map.o _build/opt/cmdline.o _build/opt/threaddag.o _build/opt/callback.o _build/opt/atomic.o _build/opt/machine.o _build/opt/worker.o _build/opt/logging.o _build/opt/stats.o _build/opt/microtime.o _build/opt/ticks.o _build/opt/scheduler.o _build/opt/messagestrategy.o _build/opt/estimator.o _build/opt/workstealing.o _build/opt/native.o -o parallel-file-map.opt
=================
clang: warning: argument unused during compilation: '-pthread'
bash-3.2$ make: `parallel-file-map.opt' is up to date.
bash-3.2$ [1/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 38.323
[2/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 38.504
[3/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 2
exectime 36.417
[4/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 4
exectime 44.774
[5/5]
./parallel-file-map.opt -n 100000000 -cutoff 100 -proc 8
exectime 69.791

 * What is interesting here is that buffering changes a lot but speedup
 * seems very similar.
 *
 *
 * Here is the same experiment using arrays.  Note that the speedups
 * are a lot better.  We see the memory bottleneck kicking in at 4
 * procossors. but there is a marked impvoment at 2.  In the file
 * example, there is no improvement.

bash-3.2$ ../pbench/prun speedup -baseline "./parallel-array-map.opt -proc 1" -parallel "./parallel-array-map.opt -proc 1,2,4,8" -n 100000000 -cutoff 100
[1/5]
./parallel-array-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 0.921
[2/5]
./parallel-array-map.opt -n 100000000 -cutoff 100 -proc 1
exectime 0.907
[3/5]
./parallel-array-map.opt -n 100000000 -cutoff 100 -proc 2
exectime 0.577
[4/5]
./parallel-array-map.opt -n 100000000 -cutoff 100 -proc 4
exectime 0.454
[5/5]
./parallel-array-map.opt -n 100000000 -cutoff 100 -proc 8
exectime 0.487

*
* 
*
* TODO: Asses the situation with cilk.
 */


#include <math.h>
#include <fstream>
#include "benchmark.hpp"

using namespace std;

/***********************************************************************/

namespace par = pasl::sched::native;

long cutoff = 0;

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


static double sum_data (int* data, int start, int end)
{
  double sum = 0.0;

  if (end-start < cutoff) {
//    sum = par_fib (30);
    for (int i = start; i < end; ++i) {
//      cout << "i = " << i << " data[i] = " << data[i] << endl ;
      sum = sum + (int) data[i];
    }

  }
  else {
    int mid = (start + end) / 2;
    double a = 0.0;
    double b = 0.0;

		par::fork2([&] 
							 { a = sum_data (data, start, mid); },
               [&] 
            	 { b = sum_data (data, mid, end); });		

//    cout << "|";
    sum = a + b ;    
  };

  return sum;

}// sum_data

// Assume that block_size = 4 and convert it into integers for addition.
static double sum_data_seq (int* data, int start, int end)
{
  double sum = 0.0;
	
  for (int i = start; i < end; ++i) {
    int d = (int) data[i];
//		cout << "d = " << d << endl;
    sum = sum + d;
  };

//	cout << "sum = " << sum << endl;
	return sum;
}// sum_data_seq

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
    int m = j-i;            // how many blocks do we have.
    char* data = new char[block_size*m]; 

    // take lock.
    f_lock.spin_to_lock ();     				

    // turn off file buffering
    f.rdbuf()->pubsetbuf(0, 0);
		
    f.seekg (i * block_size, ios::beg);        
    f.read (data, block_size*m);
		f.close ();
    // end read.

		// release lock.
		f_lock.release ();

		// reduce over the block.
    double s = sum_data ((int*) data, 0, m);		
  	return s;
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

		// reduce over the block.
    double s = sum_data ((int*) data, 0, m);		
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
