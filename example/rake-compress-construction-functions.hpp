#include "rake-compress-primitives.hpp"

const int MAX_ROUND = 21;

void initialization_construction(int n, std::vector<int>* children, int* parent) {
#ifdef SPECIAL
  memory = new Node*[n * MAX_ROUND];
  for (int i = 0; i < MAX_ROUND; i++) {
    for (int j = 0; j < n; j++) {
      memory[i * n + j] = new Node(j);
      if (i > 0) {
        memory[(i - 1) * n + j]->next = memory[i * n + j];
      }
    }
  }
#endif

  lists = new Node*[n];
  for (int i = 0; i < n; i++) {
#ifdef STANDART
    lists[i] = new Node(i);
#elif SPECIAL
    lists[i] = memory[i];
#endif
    lists[i]->head = lists[i];
    lists[i]->set_parent(lists[i]);
  }

  for (int i = 0; i < n; i++) {
    lists[i]->state.parent = lists[parent[i]];
    for (int child : children[i]) {
      lists[i]->add_child(lists[child]);
    }
    lists[i]->prepare();
  }

  live[0] = new int[n];
  for (int i = 0; i < n; i++)
    live[0][i] = i;
  live[1] = new int[n];
  for (int i = 0; i < n; i++) live[1][i] = i;
  len[0] = n;
}

void construction_round(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << " " << live[round % 2][0] << std::endl;
  }

//  for (int i = 0; i < len[round % 2]; i++) {
  pasl::sched::native::parallel_for(0, len[round % 2], [&] (int i) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(lists[v], round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(lists[v]);
    }
  });

/*  len[1 - round % 2] = 0;
  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    if (!lists[v]->is_contracted() && !lists[v]->is_known_root())
        live[1 - round % 2][len[1 - round % 2]++] = v;
  }*/


  len[1 - round % 2] = pbbs::sequence::filter(live[round % 2], live[1 - round % 2], len[round % 2], [&] (int v) {
    return !lists[v]->is_contracted() && !lists[v]->is_known_root();
  });

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    std::set<Node*>& copy_children = lists[v]->prev->get_children();
//    std::set<Node*> copy_children = lists[v]->get_children();
    for (auto child : copy_children) {
//    for (auto child : memory[v][round]->get_children()) {
      if (child->is_contracted())
        delete_node(child);
    }
  });

/*  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    if (lists[v]->get_parent()->is_contracted()) {
      delete_node_for(lists[v]->get_parent(), lists[v]);
    }

    std::set<Node*>& copy_children = lists[v]->prev->get_children();
    for (auto child : copy_children) {
      if (child->is_contracted())
        delete_node_for(child, lists[v]);
    }
  });*/

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    lists[v]->advance();
    lists[v]->prepare();
  });
}

void construction_round_seq(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << " " << live[round % 2][0] << std::endl;
  }

  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(lists[v], round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(lists[v]);
    }
  }

  len[1 - round % 2] = 0;
  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    if (lists[v]->is_contracted()) {
      delete_node(lists[v]);
    } else {
      if (!lists[v]->is_known_root())
        live[1 - round % 2][len[1 - round % 2]++] = v;
    }
  }

  for (int i = 0; i < len[1 - round % 2]; i++) {
    int v = live[1 - round % 2][i];
    lists[v]->advance();
    lists[v]->prepare();
  }
}

template<typename Round>
void construction(int n, Round round_function) {
  int round_no = 0;
  while (len[round_no % 2] > 0) {
    round_function(round_no);
    round_no++;
  }
  std::cerr << "Number of rounds: " << round_no << std::endl;
}

