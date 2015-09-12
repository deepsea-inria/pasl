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
 * Implementation: File map
 *
 */


#include <math.h>
#include <fstream>
#include "benchmark.hpp"
#include "parallel-file-map.h"

/***********************************************************************/

namespace par = pasl::sched::native;

long cutoff = 0;


/*---------------------------------------------------------------------*/



static seq_file_map (ifstream f, int n)
{
    for (long i=0; i < n;n+=8) {
        
    }
    
}//seq_node_contract


/*---------------------------------------------------------------------*/

static long par_fib(long n) {
    if (n <= cutoff || n < 2)
        return seq_fib(n);
    long a, b;
    par::fork2([n, &a] { a = par_fib(n-1); },
               [n, &b] { b = par_fib(n-2); });
    return a + b;
}

long filesize(const char* filename)
{
    std::ifstream file(filename, ios::binary |  ios::ate);
    pos_type n = in.tellg();
    file.close();
    return (long) n;
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
        
        
        n = file_size (file_name)
        ifstram in_file(file_name, ios::binary);
        result = par_file_map (in_file,n);
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
