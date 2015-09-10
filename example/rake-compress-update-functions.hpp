#include "rake-compress-primitives.hpp"

void initialization_update_seq(int n, int add_no, int* add_p, int* add_v, int delete_no, int* delete_p, int* delete_v) {
  set_number = 1;
  vertex_thread = new int[n];
  for (int i = 0; i < n; i++) {
    vertex_thread[i] = -1;
  }

  live_affected_sets = new std::unordered_set<Node*>[set_number];
  deleted_affected_sets = new std::unordered_set<Node*>[set_number];
  live_affected_sets[0] = std::unordered_set<Node*>();
  deleted_affected_sets[0] = std::unordered_set<Node*>();

  if (seq) {
    for (int i = 0; i < delete_no; i++) {
      Node* p = lists[delete_p[i]]->head;
      Node* v = lists[delete_v[i]]->head;

      v->set_parent(v);
      p->remove_children(v);

      make_affected(p, 0, false);
      make_affected(v, 0, false);
    }

    for (int i = 0; i < add_no; i++) {
      Node* p = lists[add_p[i]]->head;
      Node* v = lists[add_v[i]]->head;

      v->set_parent(p);
      p->add_children(v);

      make_affected(p, 0, false);
      make_affected(v, 0, false);
    }
  }
}

void update_round_seq(int round) {
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }

  std::unordered_set<Node*> old_live_affected_set();
  std::unordered_set<Node*> old_deleted_affected_set();
  old_live_affected_set.swap(live_affected_sets[0]);
  old_deleted_affected_set.swap(deleted_affected_sets[0]);
  for (Node* v : old_live_affected_set) {
    if (is_contracted(u, round)) {
      v->set_contracted(true);
      if (on_frontier(v)) {
        if (v->get_parent()->next != NULL) {
          make_affected(v->get_parent(), 0, true);
        }
        for (Node* child : v->get_children()) {
          if (child->next != NULL) {
            make_affected(child, 0, true);
          }
        }
      }
    } else if (v->is_root()) {
      v->set_root(true);
    } else {
      copy_node(v->get_vertex());
      live_affected_sets[0].insert(v->next);
    }
  }

  for (Node* v : live_affected_sets[0]) {
    Node* p = v->get_parent();
    if (p->is_contracted()) {
      delete_node(p);
    }
    for (Node* u : v->get_children()) {
      if (u->is_contracted()) {
        delete_node(u);
      }
    }
  }

  for (Node* v : live_affected_sets[0]) {
    v->advance();
  }

  for (Node* v : old_deleted_affected_set) {
    if (v->next != NULL)
      deleted_affected_sets[0].insert(v->next);
    delete v;
  }
}
