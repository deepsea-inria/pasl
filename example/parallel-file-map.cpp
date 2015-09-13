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

static int64_t seq_file_map (ifstream f, int n)
{
    for (long i=0; i < n;n+=8) {
        
    }
 
  return 0;
    
}//seq_node_contract


/*---------------------------------------------------------------------*/

long filesize(const char* file_name)
{
  ifstream file (file_name, ios::binary |  ios::ate);
  ifstream::pos_type n = file.tellg();
    file.close();
    return (long) n;
}


long create_file (const string file_name, int64_t n) 
{
  ofstream out_file;
  out_file.open (file_name, ios::binary);

  for (int64_t i=0; i<n; ++i) {
    out_file << i;  
  }
 
  out_file.close ();
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
        string file_name = "input.dat";

        create_file (file_name, (int64_t) n);
        return 0;
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
