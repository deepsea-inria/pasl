#include "rake-compress-primitives.hpp"

void initialization_construction(int n, std::vector<int>* children, int* parent) {
  lists = new Node*[n];
  for (int i = 0; i < n; i++) {
    lists[i] = new Node(i);
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
    std::cerr << round << " " << len[round % 2] << std::endl;
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

  len[1 - round % 2] = pbbs::sequence::filter(live[round % 2], live[1 - round % 2], len[round % 2], [&] (int v) {
    return !lists[v]->is_contracted() && !lists[v]->is_known_root();
  });

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    std::set<Node*> copy_children = lists[v]->get_children();
    for (auto child : copy_children) {
      if (child->is_contracted())
        delete_node(child);
    }
  });

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    lists[v]->advance();
    lists[v]->prepare();
  });
}

void construction_round_seq(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
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

