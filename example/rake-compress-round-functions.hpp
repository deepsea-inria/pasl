#include "rake-compress-primitives.hpp"

void construction_round(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }

  pasl::sched::native::parallel_for(0, len[round % 2], [&] (int i) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(v, round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(v);
    }
  });

  len[1 - round % 2] = pbbs::sequence::filter(live[round % 2], live[1 - round % 2], len[round % 2], [&] (int v) {
    return !lists[v]->is_contracted() && !lists[v]->is_known_root();
  });

/*  len[1 - round % 2] = 0;
  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    if (!lists[v]->is_contracted() && !lists[v]->is_known_root())
        live[1 - round % 2][len[1 - round % 2]++] = v;
  }*/

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    std::set<Node*> copy_children = lists[v]->get_children();
    for (auto child : copy_children) {
      if (child->is_contracted())
        delete_node(child->get_vertex());
    }
  });

  pasl::sched::native::parallel_for(0, len[1 - round % 2], [&] (int i) {
    int v = live[1 - round % 2][i];
    lists[v]->set_parent(lists[v]->get_parent()->next);
    std::set<Node*> new_children;
    for (Node* child : lists[v]->get_children()) {
      new_children.insert(child->next);
    }
    lists[v]->set_children(new_children);
    lists[v]->prepare();
  });
}

void construction_round_seq(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }

  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    bool is_contr = is_contracted(v, round);
    bool is_root = lists[v]->is_root();
    if (!is_contr && !is_root) {
      copy_node(v);
    }
  }

  len[1 - round % 2] = 0;
  for (int i = 0; i < len[round % 2]; i++) {
    int v = live[round % 2][i];
    if (lists[v]->is_contracted()) {
      delete_node(v);
    } else {
      if (!lists[v]->is_known_root())
        live[1 - round % 2][len[1 - round % 2]++] = v;
    }
  }

  for (int i = 0; i < len[1 - round % 2]; i++) {
    int v = live[1 - round % 2][i];
    lists[v]->set_parent(lists[v]->get_parent()->next);
    std::set<Node*> new_children;
    for (Node* child : lists[v]->get_children()) {
      new_children.insert(child->next);
    }
    lists[v]->set_children(new_children);
  }
}
