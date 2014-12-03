/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file multishot.hpp
 * \brief Implementation of native, multi-shot threads.
 *
 */

#include <utility>
#include <functional>

#if defined(USE_CILK_RUNTIME)
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#include "thread.hpp"
#include "threaddag.hpp"
#include "control.hpp"
#include "atomic.hpp"

#ifndef _PASL_NATIVE_H_
#define _PASL_NATIVE_H_

namespace pasl {
namespace sched {
namespace native {

/***********************************************************************/

namespace ucxt = util::control;

/*---------------------------------------------------------------------*/

class multishot : public thread {
protected:

  using multishot_p = multishot*;
  using context_type = ucxt::context::context_type;
  using context = ucxt::context;

  // declaration of dummy-pointer constants
  static char dummy1;
  static char dummy2;
  static constexpr char* notaptr = &dummy1;
  /* indicates to a thread that the thread does not need to deallocate
   * the call stack on which it is running
   */
  static constexpr char* notownstackptr = &dummy2;

  //! pointer to the call stack of this thread
  char* stack;
  //! CPU context of this thread
  context_type cxt;

  void swap_with_scheduler() {
    context::swap(context::addr(cxt), ucxt::my_cxt(), notaptr);
  }

  static void exit_to_scheduler() {
    context::throw_to(ucxt::my_cxt(), notaptr);
  }

  void prepare() {
    threaddag::reuse_calling_thread();
  }

  void prepare_and_swap_with_scheduler() {
    prepare();
    swap_with_scheduler();
  }

  // point of entry from the scheduler to the body of this thread
  // the scheduler may reenter this (multi-shot) thread via this method
  void exec() {
    if (stack == nullptr)
      // initial entry by the scheduler into the body of this thread
      stack = context::spawn(context::addr(cxt), this);
    // jump into body of this thread
    context::swap(ucxt::my_cxt(), context::addr(cxt), this);
  }

  // point of entry to this thread to be called by the `context::spawn` routine
  static void enter(multishot* t) {
    assert(t != nullptr);
    assert(t != (multishot_p)notaptr);
    t->run();
    // terminate thread by exiting to scheduler
    exit_to_scheduler();
  }

public:

  multishot()
  : thread(), stack(nullptr)  { }

  ~multishot() {
    if (stack == nullptr)
      return;
    if (stack == notownstackptr)
      return;
    free(stack);
    stack = nullptr;
  }

  virtual void run() = 0;

  // schedule this thread and then return control to scheduler
  void yield() {
    threaddag::continue_with(this);
    prepare_and_swap_with_scheduler();
  }

  void async(multishot_p thread, multishot_p join) {
    threaddag::fork(thread, join);
    yield();
  }

  void finish(multishot_p thread) {
    instrategy::distributed* dist = new instrategy::distributed(this);
    threaddag::unary_fork_join(thread, this, dist);
    prepare_and_swap_with_scheduler();
  }

  void fork2(multishot_p thread0, multishot_p thread1) {
    LOG_THREAD_FORK(this, thread0, thread1);
    prepare();
    threaddag::binary_fork_join(thread0, thread1, this);
    if (context::capture<multishot*>(context::addr(cxt))) {
      //      util::atomic::aprintf("steal happened: executing join continuation\n");
      return;
    }
    scheduler_p sched = threaddag::my_sched();
    //    worker_id_t id = sched->get_id();
    // know thread0 stays on my stack
    thread0->stack = notownstackptr;
    thread0->swap_with_scheduler();
    assert(sched == threaddag::my_sched());
    // sched is popping thread0
    // run begin of sched->exec(thread0) until thread0->exec()
    thread0->run();
    sched = threaddag::my_sched();
    // if thread1 was not stolen, then it can run in the same stack as parent
    if (! sched->local_has() || sched->local_peek() != thread1) {
      //      util::atomic::aprintf("%d %d detected steal of %p\n",id,util::worker::get_my_id(),thread1);
      exit_to_scheduler();
      return; // unreachable
    }
    //    util::atomic::aprintf("%d %d ran %p; going to run thread %p\n",id,util::worker::get_my_id(),thread0,thread1);
    // prepare thread1 for local run
    assert(sched == threaddag::my_sched());
    assert(thread1->stack == nullptr);
    thread1->stack = notownstackptr;
    thread1->swap_with_scheduler();
    //    util::atomic::aprintf("%d %d this=%p thread0=%p thread1=%p\n",id,util::worker::get_my_id(),this, thread0, thread1);
    assert(sched == threaddag::my_sched());
    //    printf("ran %p and %p locally\n",thread0,thread1);
    // run end of sched->exec() starting after thread0->exec()
    // run begin of sched->exec(thread1) until thread1->exec()
    thread1->run();
    swap_with_scheduler();
    // run end of sched->exec() starting after thread1->exec()
  }

  friend class sched::scheduler::_private;
  friend class ucxt::context;
};

/*---------------------------------------------------------------------*/

template <class Function>
class multishot_by_lambda : public multishot {
private:

  Function f;

public:

  multishot_by_lambda(const Function& f) : f(f) { }

  void run() {
    f();
  }

  THREAD_COST_UNKNOWN
};

template <class Function>
multishot_by_lambda<Function>* new_multishot_by_lambda(const Function& f) {
  return new multishot_by_lambda<Function>(f);
}

static inline multishot* my_thread() {
  multishot* t = (multishot*)threaddag::my_sched()->get_current_thread();
  assert(t != nullptr);
  return t;
}

static inline int my_deque_size() {
#if defined(__PASL_CILK_EXT)
  return __cilkrts_get_deque_size(__cilkrts_get_tls_worker());
#elif defined(USE_CILK_RUNTIME)
  util::atomic::die("bogus version of cilk runtime");
  return 0;
#else
  scheduler_p sched = threaddag::my_sched();
  return (int)sched->nb_threads();
#endif
}

/*---------------------------------------------------------------------*/

template <class Exp1, class Exp2>
void fork2(const Exp1& exp1, const Exp2& exp2) {
#if defined(SEQUENTIAL_ELISION)
  exp1();
  exp2();
#elif defined(USE_CILK_RUNTIME)
  cilk_spawn exp1();
  exp2();
  cilk_sync;
#else
  my_thread()->fork2(new_multishot_by_lambda(exp1),
                     new_multishot_by_lambda(exp2));
#endif
}

template <class Body>
void async(const Body& body, multishot* join) {
  multishot* thread = new_multishot_by_lambda(body);
  my_thread()->async(thread, join);
}

template <class Body>
void finish(const Body& body) {
  multishot* join = my_thread();
  multishot* thread = new_multishot_by_lambda([&] { body(join); });
  join->finish(thread);
}

static inline void yield() {
  multishot* thread = my_thread();
  assert(thread != nullptr);
  thread->yield();
}

template <class Body, class State, class Size_input, class Fork_input, class Set_in_env>
class parallel_while_base : public multishot {
public:

  Body f;
  Size_input size_input;
  Fork_input fork_input;
  Set_in_env set_in_env;
  State state;
  multishot* join;

  using self_type = parallel_while_base<Body, State, Size_input, Fork_input, Set_in_env>;

  parallel_while_base(const Body& f,
                      const Size_input& size_input,
                      const Fork_input& fork_input,
                      const Set_in_env& set_in_env,
                      multishot* join)
  : f(f), size_input(size_input), fork_input(fork_input), set_in_env(set_in_env), join(join) {
    set_in_env(state);
  }

  parallel_while_base(const self_type& other)
  : f(other.f), size_input(other.size_input), fork_input(other.fork_input),
    set_in_env(other.set_in_env), join(other.join) {
    set_in_env(state);
  }

  void run() {
    f(state);
  }

  size_t size()  {
    return size_input(state);
  }

  thread_p split(size_t) {
    self_type* t = new self_type(*this);
    fork_input(state, t->state);
    t->set_instrategy(instrategy::ready_new());
    t->set_outstrategy(outstrategy::unary_new());
    threaddag::add_dependency(t, join);
    return t;
  }

  THREAD_COST_UNKNOWN

};

#if defined(USE_CILK_RUNTIME) && defined(__PASL_CILK_EXT)
// version of parallel while that uses lazy binary splitting a la Tzannes et al
template <class Input, class Size_input, class Fork_input, class Set_in_env, class Body>
void parallel_while_cilk_rec(Input& input, const Size_input& size_input, const Fork_input& fork_input,
                             const Set_in_env& set_in_env, const Body& body) {
  set_in_env(input);
  size_t sz = size_input(input);
  while (sz > 0) {
    if (sz > 1 && my_deque_size() == 0) {
      Input input2;
      fork_input(input, input2);
      fork2([&] { parallel_while_cilk_rec(input, size_input, fork_input, set_in_env, body); },
            [&] { parallel_while_cilk_rec(input2, size_input, fork_input, set_in_env, body); });
      return;
    }
    body(input);
    sz = size_input(input);
  }
}
#endif
  
const bool debug_print = false;
template <class Body>
void msg(const Body& b) {
  if (debug_print)
    util::atomic::msg(b);
}
  
template <class Input, class Size_input, class Fork_input, class Set_in_env, class Body>
void parallel_while(Input& input, const Size_input& size_input, const Fork_input& fork_input,
                    const Set_in_env& set_in_env, const Body& body) {
#if defined(SEQUENTIAL_ELISION)
  set_in_env(input);
  while (size_input(input) > 0)
    body(input);
#elif defined(USE_CILK_RUNTIME) && defined(__PASL_CILK_EXT)
  parallel_while_cilk_rec(input, size_input, fork_input, set_in_env, body);
#else
  auto b = [&] (Input& state) {
    Input input;
    set_in_env(input);
    input.swap(state);
    while (size_input(input) > 0) { // TODO: replace "size_input(input)" with "should_continue(input)"
      body(input);
      input.swap(state);
      yield();
      input.swap(state);
    }
  };
  using thread_type = parallel_while_base<typeof(b), Input, Size_input, Fork_input, Set_in_env>;
  multishot* join = my_thread();
  thread_type* thread = new thread_type(b, size_input, fork_input, set_in_env, join);
  set_in_env(input);
  input.swap(thread->state);
  join->finish(thread);
#endif
}
  
template <class Input, class Size_input, class Fork_input, class Body>
void parallel_while(Input& input, const Size_input& size_input, const Fork_input& fork_input,
                    const Body& body) {
  auto set_in_env = [] (Input&) { };
  parallel_while(input, size_input, fork_input, set_in_env, body);
}
  
template <class Body>
void parallel_while(const Body& body) {
  using input_type = struct { };
  auto size_fct = [] (input_type&) {
    return 2;
  };
  auto fork_fct = [] (input_type&, input_type&) {
  };
  auto set_fct = [] (input_type&) {
  };
  auto b = [&] (input_type&) {
    body();
  };
  using thread_type = parallel_while_base<typeof(b), input_type, typeof(size_fct), typeof(fork_fct), typeof(set_fct)>;
  multishot* join = my_thread();
  thread_type* thread = new thread_type(b, size_fct, fork_fct, set_fct, join);
  join->finish(thread);
}
  
//! \todo replace calls to frontier.swap with calls to new given function called input_swap
template <class Input, class Size_input, class Fork_input, class Set_in_env, class Body>
void parallel_while_cas_ri(Input& input, const Size_input& size_input, const Fork_input& fork_input,
                           const Set_in_env& set_in_env, const Body& body) {
#if defined(SEQUENTIAL_ELISION)
  parallel_while(input, size_input, fork_input, set_in_env, body);
#elif defined(USE_CILK_RUNTIME) && defined(__PASL_CILK_EXT)
  parallel_while(input, size_input, fork_input, set_in_env, body);
#else
  using request_type = worker_id_t;
  const request_type Request_blocked = -2;
  const request_type Request_waiting = -1;
  using answer_type = enum {
    Answer_waiting,
    Answer_transfered
  };
  data::perworker::array<Input> frontier;
  data::perworker::array<std::atomic<request_type>> request;
  data::perworker::array<std::atomic<answer_type>> answer;
  data::perworker::counter::carray<long> counter;
  worker_id_t leader_id = threaddag::get_my_id();
  msg([&] { std::cout << "leader_id=" << leader_id << std::endl; });
  frontier.for_each([&] (worker_id_t, Input& f) {
    set_in_env(f);
  });
  request.for_each([&] (worker_id_t i, std::atomic<request_type>& r) {
    request_type t = (i == leader_id) ? Request_waiting : Request_blocked;
    r.store(t);
  });
  answer.for_each([] (worker_id_t, std::atomic<answer_type>& a) {
    a.store(Answer_waiting);
  });
  counter.init(0);
  std::atomic<bool> is_done(false);
  auto b = [&] {
    worker_id_t my_id = threaddag::get_my_id();
    scheduler_p sched = threaddag::my_sched();
    multishot* thread = my_thread();
    int nb_workers = threaddag::get_nb_workers();
    Input my_frontier;
    set_in_env(my_frontier); // probably redundant
    if (my_id == leader_id) {
      counter++;
      my_frontier.swap(input);
    }
    msg([&] { std::cout << "entering my_id=" << my_id << std::endl; });
    long sz;
    bool init = (my_id != leader_id);
    while (true) {
      if (init) {
        init = false;
        goto acquire;
      }
      // try to perform some local work
      while (true) {
        thread->yield();
        if (is_done.load())
          return;
        sz = (long)size_input(my_frontier);
        if (sz == 0) {
          counter--;
          msg([&] { std::cout << "decr my_id=" << my_id << " sum=" << counter.sum() << std::endl; });
          break;
        } else { // have some work to do
          body(my_frontier);
          // communicate
          msg([&] { std::cout << "communicate my_id=" << my_id << std::endl; });
          request_type req = request[my_id].load();
          assert(req != Request_blocked);
          if (req != Request_waiting) {
            worker_id_t j = req;
            if (size_input(my_frontier) > 1) {
              counter++;
              msg([&] { std::cout << "transfer from my_id=" << my_id << " to " << j << " sum=" << counter.sum() << std::endl; });
              fork_input(my_frontier, frontier[j]);
            } else {
              msg([&] { std::cout << "reject from my_id=" << my_id << " to " << j << std::endl; });
            }
            answer[j].store(Answer_transfered);
            request[my_id].store(Request_waiting);
          }
        }
      }
      assert(sz == 0);
    acquire:
      sz = 0;
      // reject
      while (true) {
        request_type t = request[my_id].load();
        if (t == Request_blocked) {
          break;
        } else if (t == Request_waiting) {
          request[my_id].compare_exchange_strong(t, Request_blocked);
        } else {
          worker_id_t j = t;
          request[my_id].compare_exchange_strong(t, Request_blocked);
          answer[j].store(Answer_transfered);
        }
      }
      // acquire
      msg([&] { std::cout << "acquire my_id=" << my_id << std::endl; });
      while (true) {
        thread->yield();
        if (is_done.load())
          return;
        answer[my_id].store(Answer_waiting);
        if (my_id == leader_id && counter.sum() == 0) {
          is_done.store(true);
          continue;
        }
        util::ticks::microseconds_sleep(1.0);
        if (nb_workers > 1) {
          worker_id_t id = sched->random_other();
          if (request[id].load() == Request_blocked)
            continue;
          request_type orig = Request_waiting;
          bool s = request[id].compare_exchange_strong(orig, my_id);
          if (! s)
            continue;
          while (answer[my_id].load() == Answer_waiting) {
            thread->yield();
            util::ticks::microseconds_sleep(1.0);
            if (is_done.load())
              return;
          }
          frontier[my_id].swap(my_frontier);
          sz = (long)size_input(my_frontier);
        }
        if (sz > 0) {
          msg([&] { std::cout << "received " << sz << " items my_id=" << my_id << std::endl; });
          request[my_id].store(Request_waiting);
          break;
        }
      }
    }
    msg([&] { std::cout << "exiting my_id=" << my_id << std::endl; });
  };
  parallel_while(b);
#endif
}

template <class Input, class Output,
          class Size_input, class Fork_input, class Join_output,
          class Set_in_env, class Set_out_env,
          class Body>
void parallel_while_lazy(Input& input, Output& output,
                         const Size_input& size_input, const Fork_input& fork_input, const Join_output& join,
                         const Set_in_env& set_in_env, const Set_out_env& set_out_env,
                         const Body& body) {
  set_in_env(input);
  size_t sz = size_input(input);
  while (sz > 0) {
    if (sz > 1 && my_deque_size() == 0) {
      Input input2;
      Output output2;
      set_in_env(input2);
      set_out_env(output2);
      fork_input(input, input2);
      fork2([&] { parallel_while_cilk_rec(input, output, size_input, fork_input, set_in_env, set_out_env, body); },
            [&] { parallel_while_cilk_rec(input2, output2, size_input, fork_input, set_in_env, set_out_env, body); });
      join_output(output, output2);
      return;
    }
    body(input, output);
    sz = size_input(input);
  }
}

template <class Input, class Output,
class Cutoff, class Fork_input, class Join_output,
class Set_in_env, class Set_out_env,
class Body>
void forkjoin(Input& in, Output& out,
            const Cutoff& cutoff, const Fork_input& fork, const Join_output& join,
            const Set_in_env& set_in_env, const Set_out_env& set_out_env,
            const Body& body) {
  if (cutoff(in)) {
    body(in, out);
  } else {
    Input in2;
    Output out2;
    set_in_env(in2);
    set_out_env(out2);
    fork(in, in2);
    fork2([&] { forkjoin(in,  out,  cutoff, fork, join, set_in_env, set_out_env, body); },
          [&] { forkjoin(in2, out2, cutoff, fork, join, set_in_env, set_out_env, body); });
    join(out, out2);
  }
}

template <class Input, class Output,
class Cutoff, class Fork_input, class Join_output,
class Body>
void forkjoin(Input& in, Output& out,
            const Cutoff& cutoff, const Fork_input& fork, const Join_output& join,
            const Body& body) {
  auto set_in_env = [] (Input&) { };
  auto set_out_env = [] (Output&) { };
  forkjoin(in, out, cutoff, fork, join, set_in_env, set_out_env, body);
}

extern int loop_cutoff;

template <class Number, class Output, class Join_output, class Body, class Cutoff>
void combine(Number lo, Number hi, Output& out, const Join_output& join,
              const Body& body, const Cutoff& cutoff) {
  using range_type = std::pair<Number, Number>;
  range_type in(lo, hi);
  auto fork = [] (range_type& src, range_type& dst) {
    Number mid = (src.first + src.second) / 2;
    dst.first = mid;
    dst.second = src.second;
    src.second = mid;
  };
  auto _body = [&body] (range_type r, Output& out) {
    Number lo = r.first;
    Number hi = r.second;
    for (Number i = lo; i < hi; i++)
      body(i, out);
  };
  forkjoin(in, out, cutoff, fork, join, _body);
}

template <class Number, class Output, class Join_output, class Body>
void combine(Number lo, Number hi, Output& out, const Join_output& join,
             const Body& body, int cutoff = loop_cutoff) {
  using range_type = std::pair<Number, Number>;
  auto cutoff_fct = [cutoff] (range_type r) {
    return r.second - r.first <= cutoff;
  };
  combine(lo, hi, out, join, body, cutoff_fct);
}

template <class Number, class Body>
void parallel_for(Number lo, Number hi, const Body& body) {
#if defined(SEQUENTIAL_ELISION)
  for (Number i = lo; i < hi; i++)
    body(i);
    /* cilk_for not yet supported by mainline gcc
#elif defined(USE_CILK_RUNTIME)
  cilk_for (Number i = lo; i < hi; i++)
    body(i);
    */
#else
  struct { } output;
  using output_type = typeof(output);
  auto join = [] (output_type,output_type) { };
  auto _body = [&body] (Number i, output_type) {
    body(i);
  };
  combine(lo, hi, output, join, _body);
#endif
}

template <class Number, class Body>
void parallel_for1(Number lo, Number hi, const Body& body) {
#if defined(SEQUENTIAL_ELISION)
  for (Number i = lo; i < hi; i++)
    body(i);
    /* cilk_for not yet supported by mainline gcc
#elif defined(USE_CILK_RUNTIME)
  _Pragma("cilk grainsize = 1") cilk_for (Number i = lo; i < hi; i++)
    body(i);
    */
#else
  struct { } output;
  using range_type = std::pair<Number, Number>;
  auto cutoff = [] (range_type r) {
    return r.second - r.first <= 2;
  };
  using output_type = typeof(output);
  auto join = [] (output_type,output_type) { };
  auto _body = [&body] (Number i, output_type) {
    body(i);
  };
  combine(lo, hi, output, join, _body, cutoff);
#endif
}

/***********************************************************************/


} // end namespace
} // end namespace
} // end namespace

#endif //! _PASL_NATIVE_H_
