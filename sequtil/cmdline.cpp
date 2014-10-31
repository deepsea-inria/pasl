/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file cmdline.cpp
 * \brief Command-line parsing routines
 *
 */

#include <string.h>
#include <utility>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
// NEW: remove useless includes

#include "cmdline.hpp"

namespace pasl {
namespace util {
namespace cmdline {

/***********************************************************************/
/* Command line arguments */

int global_argc = -1;
char** global_argv;
bool print_warning_on_use_of_default_value;

void set(int argc, char** argv)
{
  global_argc = argc;
  global_argv = argv;
  print_warning_on_use_of_default_value = parse_or_default_bool("warning", false, false);
}

std::string name_of_my_executable() {
  return std::string(global_argv[0]);
}

/*---------------------------------------------------------------------*/
/* Auxiliary failure functions */

static void failure()
{
   printf("sError illegal command line\n");
   exit(-1);
}

static void check_set() 
{
  if (global_argc == -1) {
    printf("you must call cmdline::set(argc,argv) in your main.");
    exit(-1);
  }
}

static void check (std::string name, bool result) 
{
  if (! result) {
     printf("missing command line argument %s\n", name.c_str());
     exit(-1);
  }
}

void die (const char *fmt, ...) 
{
  va_list	ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end(ap);
  exit(-1);
}


/*---------------------------------------------------------------------*/
/* Supported type of arguments */

typedef enum {
  INT,
  LONG,
  INT64,
  UINT64,
  FLOAT,
  DOUBLE,
  CHARS,
  STRING,
  BOOL,
  /* could add support for:
  INT32,
  INT64,
  UINT32,
  UINT64,
  */
} type_t;


/* Parsing of one value of a given type into a given address */

static void parse_value(type_t type, void* dest, char* arg_value)
{
  switch (type)
  {
    case INT: {
      int* vi = (int*) dest;
      *vi = atoi(arg_value);
      break; }
    case LONG: {
      long* vi = (long*) dest;
      sscanf(arg_value, "%ld", vi);
      break; }
    case INT64: {
      int64_t* vi = (int64_t*) dest;
      sscanf(arg_value, "%lld", vi);
      break; }
    case UINT64: {
      uint64_t* vi = (uint64_t*) dest;
      sscanf(arg_value, "%llu", vi);
      break; }
    case BOOL: {
      bool* vb = (bool*) dest;
      *vb = (atoi(arg_value) != 0);
      break; }
    case FLOAT: {
      float* vf = (float*) dest;
      *vf = (float)atof(arg_value);
      break; }
    case DOUBLE: {
      double* vf = (double*) dest;
      *vf = atof(arg_value);
      break; }
    case CHARS: {
      char* vs = (char*) dest;
      strcpy(vs, arg_value);
      break; }
    case STRING: {
      std::string* vs = (std::string*) dest;
      *vs = std::string(arg_value);
      break; }
    default: {
      printf("not yet supported");
      exit(-1); }
  }
}

static bool parse(type_t type, std::string name, void* dest) 
{ 
  check_set();  
  for (int a = 1; a < global_argc; a++)  
  {
    if (*(global_argv[a]) != '-') 
      failure();
    char* arg_name = global_argv[a] + 1;
    if (arg_name[0] == '-') {
      if (name.compare(arg_name+1) == 0) {
        *((bool*) dest) = 1;
        return true;
      }
    } else {
      a++;
      if (a >= global_argc) 
        failure();
      char* arg_value = global_argv[a];
      //printf("%s %s\n", arg_name, arg_value);
      if (name.compare(arg_name) == 0) {
        parse_value(type, dest, arg_value);
        return true;
      }
    }
  }
  return false;
}

/*---------------------------------------------------------------------*/
/* Specific parsing functions */

bool parse_bool(std::string name) {
  bool r;
  check (name, parse(BOOL, name, &r));
  return r;
}

int parse_int(std::string name) {
  int r;
  check (name, parse(INT, name, &r));
  return r;
}
  
long parse_long(std::string name) {
  long r;
  check (name, parse(LONG, name, &r));
  return r;
}

int64_t parse_int64(std::string name) {
  int64_t r;
  check (name, parse(INT64, name, &r));
  return r;
}

uint64_t parse_uint64(std::string name) {
  uint64_t r;
  check (name, parse(UINT64, name, &r));
  return r;
}
  
float parse_float(std::string name) {
  float r;
  check (name, parse(FLOAT, name, &r));
  return r;
}

double parse_double(std::string name) {
  double r;
  check (name, parse(DOUBLE, name, &r));
  return r;
}

std::string parse_string(std::string name) {
  std::string r;
  check (name, parse(STRING, name, &r));
  return r;
}

/*---------------------------------------------------------------------*/
/* Specific parsing functions with default values */
  
template <class T>
void print_default(std::string name, T val, bool expected) {
  if (! expected || ! print_warning_on_use_of_default_value)
    return;
  std::cerr << "Warning: using default for " << name << " " << val << std::endl;
}

bool parse_or_default_bool(std::string name, bool d, bool expected) {
  bool r;
  if (parse(BOOL, name, &r)) {
    return r; 
  } else {
    print_default(name, d, expected);
    return d;
  }
}

int parse_or_default_int(std::string name, int d, bool expected) {
  int r;
  if (parse(INT, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}
  
long parse_or_default_long(std::string name, long d, bool expected) {
  long r;
  if (parse(LONG, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}

int64_t parse_or_default_int64(std::string name, int64_t d, bool expected) {
  int64_t r;
  if (parse(INT64, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}

uint64_t parse_or_default_uint64(std::string name, uint64_t d, bool expected) {
  uint64_t r;
  if (parse(UINT64, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}
  
float parse_or_default_float(std::string name, float f, bool expected) {
  float r;
  if (parse(FLOAT, name, &r)) {
    return r;
  } else {
    print_default(name, f, expected);
    return f;
  }
}

double parse_or_default_double(std::string name, double d, bool expected) {
  double r;
  if (parse(DOUBLE, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}

std::string parse_or_default_string(std::string name, std::string d, bool expected) {
  std::string r;
  if (parse(STRING, name, &r)) {
    return r;
  } else {
    print_default(name, d, expected);
    return d;
  }
}

/*---------------------------------------------------------------------*/
/* Could add support for parsing several arguments 

class arg_t {
  string name;
  type_t type;
  void* dest;
  arg_t(string name, type_t type, void* dest) 
    : name(name), type(type), dest(dest) {}
};

void parse_several(vector<arg_t*> args) 
{
  for (vector<arg_t*>::iterator it = args.begin(); it != args.end(); it++) {
    arg_t* arg = *it;
    parse(arg->name, arg->type, arg->dest);
  }
}
*/
  
/*---------------------------------------------------------------------*/

static void add_dispatch_all_as_option(argmap_dispatch& c) {
  c.add("all", [&] { // runs all tests
    c.for_each_key([&] (std::string key, const thunk_type& t){
      if (key == "all")
        return;
      t();
    });
  });
}

void dispatch_by_argmap(argmap_dispatch& c, std::string arg, std::string dflt_key) {
  c.find_by_arg_or_default_key(arg, dflt_key)();
}
  
void dispatch_by_argmap(argmap_dispatch& c, std::string arg) {
  c.find_by_arg(arg)();
}
  
void dispatch_by_argmap_with_default_all(argmap_dispatch& c, std::string arg) {
  add_dispatch_all_as_option(c);
  dispatch_by_argmap(c, arg, "all");
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace

