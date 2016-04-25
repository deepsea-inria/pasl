/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file cmdline.hpp
 * \brief Command-line parsing routines
 *
 */

#ifndef _CMDLINE_H_
#define _CMDLINE_H_

#include <string>
#include <stdint.h>
#include <map>
#include <iostream>
#include <functional>

/**
 * \defgroup commandline_parameters Command-line parameters
 * \brief Functions for parsing of command line arguments.
 * @{
 *
 * Usage:
 * - call `set(argc, argv)` on the first line of the main
 * - call, e.g.:
 *     `int size = cmdline::parse_or_default_int("size", 10000);`
 *
 * Convention: arguments take the form `-key value`
 * or `--key`, where the later is equivalent to `-key 1`.
 */

namespace pasl {
namespace util {
namespace cmdline {
  
/***********************************************************************/

// if true, print to stderr any key/value pair that was selected as default
extern bool print_warning_on_use_of_default_value;

/*---------------------------------------------------------------------*/

/*! \fn set
 * \brief Call this function on the first line of your main */
void set(int argc, char** argv);

//! Returns a path to the calling executable
std::string name_of_my_executable();

//! Terminates the program with a printf style error message
void die(const char *fmt, ...);


/*---------------------------------------------------------------------*/


/**
 * \defgroup commandline_parameters_basic Basic command-line parsing
 * \ingroup commandline_parameters
 * @{
 * Return the value associated with a given key, for a given type,
 * or raises an error if the key is not bound */

bool parse_bool(std::string name);

int parse_int(std::string name);
  
long parse_long(std::string name);

int64_t parse_int64(std::string name);

uint64_t parse_uint64(std::string name);

double parse_double(std::string name);
  
float parse_float(std::string name);

std::string parse_string(std::string name);
  
/** @} */

/*---------------------------------------------------------------------*/

/**
 * \defgroup commandline_parameters_with_defaults Command-line parsing with default values
 * \ingroup commandline_parameters
 * @{
 * Return the value associated with a given key, for a given type,
 * or return the default value provided if the key is missing */

bool parse_or_default_bool(std::string name, bool d, bool expected=true);

int parse_or_default_int(std::string name, int d, bool expected=true);
  
long parse_or_default_long(std::string name, long d, bool expected=true);

int64_t parse_or_default_int64(std::string name, int64_t d, bool expected=true);

uint64_t parse_or_default_uint64(std::string name, uint64_t d, bool expected=true);

double parse_or_default_double(std::string name, double d, bool expected=true);
  
float parse_or_default_float(std::string name, float f, bool expected=true);

std::string parse_or_default_string(std::string name, std::string d, bool expected=true);

/** @} */
  
/*---------------------------------------------------------------------*/

/*!
 * \defgroup argmap Argument map
 * \ingroup commandline_parameters
 * @{
 * \class argmap
 * \brief A finite map for pairing command-line arguments with values.
 *
 * This data structure is useful for dealing with the following
 * scenario. Suppose we need to parse a command line parameter `param`
 * along with one or more keys `k1`, `k2`, etc.
 * 
 *      -param k1 | k2 | ...
 *
 * Moreover, suppose we wish to associate each key `ki` with a value
 * `vi`. This code snippet shows how, for example, we can associate
 * each key with an integer and then look up the corresponding value
 * via the command line.
 *
 *      argmap<int> m;
 *      m.add("k1", 1);
 *      m.add("k2", 2);
 *      int n = m.find_by_arg("param")
 *      printf("n=%d\n", n);
 *
 * If we pass on the command line `-param k2`, this program
 * prints `2`.
 */
template <class Value>
class argmap {
private:
  std::map<std::string, Value> m;
  
  void failwith(std::string arg, std::string key) {
    std::cout << "Not found: -" << arg << " " << key << std::endl;
    std::cout << "Valid arguments are: ";
    for (auto it = m.begin(); it != m.end(); it++)
      std::cout << it->first << " ";
    std::cout << std::endl;
    exit(1);
  }
  
public:
  
  void add(std::string key, Value val) {
    m.insert(std::pair<std::string, Value>(key, val));
  }
  
  Value& find(std::string arg, std::string key) {
    auto it = m.find(key);
    if (it == m.end())
      failwith(arg, key);
    return (*it).second;
  }

  Value& find_by_arg(std::string arg) {
    return find(arg, parse_or_default_string(arg, ""));
  }
  
  Value find_by_arg_or_default(std::string arg, const Value& def) {
    std::string key = parse_or_default_string(arg, "");
    auto it = m.find(key);
    if (it == m.end())
      return Value(def);
    return (*it).second;
  }
  
  Value find_by_arg_or_default_key(std::string arg, std::string dflt) {
    std::string key = parse_or_default_string(arg, dflt);
    return find(arg, key);
  }
  
  template <class Body>
  void for_each_key(const Body& f) const {
    for (auto it = m.begin(); it != m.end(); it++)
      f(it->first, it->second);
  }
  
};
  
using thunk_type = std::function<void ()>;
using argmap_dispatch = util::cmdline::argmap<thunk_type>;

/* takes a thunk argmap and a default key and dispatches on the
 * command-line-selected value (or the default key if there is
 * no specified command-line value)
 */
void dispatch_by_argmap(argmap_dispatch& c, std::string arg);
void dispatch_by_argmap(argmap_dispatch& c, std::string arg, std::string dflt_key);
/* same as above, except that when the argument is not provided by the user,
 * all the keys are selected and their corresponding thunks are called
 */
void dispatch_by_argmap_with_default_all(argmap_dispatch& c, std::string arg);
/** @} */

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

/*! @} */

#endif // _CMDLINE_H_

