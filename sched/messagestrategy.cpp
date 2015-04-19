/* COPYRIGHT (c) 2011 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file messagestrategy.cpp
 */

#include "atomic.hpp"
#include "messagestrategy.hpp"
#include "instrategy.hpp"
#include "outstrategy.hpp"

namespace pasl {
namespace sched {
namespace messagestrategy {

messagestrategy* the_messagestrategy = NULL;

/*---------------------------------------------------------------------*/
/* Message strategy */

void messagestrategy::handle_message(message_t msg) {
  switch (msg.tag) {
    case IN_DELTA: {
      instrategy_p in = msg.data.in_delta.in;
      thread_p t = msg.data.in_delta.t;
      int64_t d = msg.data.in_delta.d;
      instrategy::msg_delta(in, t, d);
      break;
    }
    case OUT_ADD: {
      outstrategy_p out = msg.data.out_add.out;
      thread_p td = msg.data.out_add.td;
      outstrategy::msg_add(out, td);
      break;
    }
    case OUT_FINISHED: {
      outstrategy_p out = msg.data.out_finished.out;
      outstrategy::msg_finished(out);
      break;
    }
    default:
      util::atomic::die("bogus message\n");
  }
}

/*---------------------------------------------------------------------*/
/* Messagestrategy implemented with Producer-Consumer Buffers (PCBs)  */

void pcb::init() {
  int nb_workers = util::worker::get_nb();
  for (worker_id_t id = 0; id < nb_workers; id++) {
    for (worker_id_t tid = 0; tid < nb_workers; tid++)
      channels[id][tid].init();
  }
  target.init(0);
  nb_processed_per_round = std::min(nb_workers, 8); // LATER: take 8 as argument
}

void pcb::destroy() {
  int nb_workers = util::worker::get_nb();
  for (worker_id_t id = 0; id < nb_workers; id++) {
    for (worker_id_t tid = 0; tid < nb_workers; tid++)
      channels[id][tid].destroy();
  }
}

void pcb::send(worker_id_t id_target, message_t msg) {
  worker_id_t id_source = util::worker::get_my_id();
  channels[id_target][id_source].push(msg);
  STAT_COUNT(MSG_SEND);
}

void pcb::check() {
  int nb_workers = util::worker::get_nb();
  if (nb_workers <= 1) 
    return;
  worker_id_t my_id = util::worker::get_my_id();
  int id = target[my_id];
  for (int __cnt = 0; __cnt < nb_processed_per_round; __cnt++) {
    id = (id + 1) % nb_workers;
    if (id == my_id) 
      continue; 
    message_t msg;
    while (channels[my_id][id].try_pop(msg))
      handle_message(msg);
  }
  target[my_id] = id;
}

} // end namespace
} // end namespace
} // end namespace

