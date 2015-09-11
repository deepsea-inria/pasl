#include "rake-compress-primitives.hpp"
#include <unordered_map>

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

  for (int i = 0; i < delete_no; i++) {
    Node* p = lists[delete_p[i]]->head;
    Node* v = lists[delete_v[i]]->head;

    v->set_parent(v);
    p->remove_child(v);

    make_affected(p, 0, false);
    make_affected(v, 0, false);
  }

  for (int i = 0; i < add_no; i++) {
    Node* p = lists[add_p[i]]->head;
    Node* v = lists[add_v[i]]->head;

    v->set_parent(p);
    p->add_child(v);

    make_affected(p, 0, false);
    make_affected(v, 0, false);
  }
}

int* ids;
std::unordered_set<Node*>* old_live_affected_sets;
std::unordered_set<Node*>* old_deleted_affected_sets;
// TODO: change to more parallelizable structure, as list of adjacency
void initialization_update(int n, std::unordered_map<int, std::vector<std::pair<int, bool>>> add,
                           std::unordered_map<int, std::vector<std::pair<int, bool>>> del) {
  set_number = add.size() + del.size();

  ids = new int[set_number];
  for (int i = 0; i < set_number; i++) {
    ids[i] = i;
  }

  vertex_thread = new int[n];
  for (int i = 0; i < n; i++) {
    vertex_thread[i] = -1;
  }

  live_affected_sets = new std::unordered_set<Node*>[set_number];
  deleted_affected_sets = new std::unordered_set<Node*>[set_number];
  old_live_affected_sets = new std::unordered_set<Node*>[set_number];
  old_deleted_affected_sets = new std::unordered_set<Node*>[set_number];
  for (int i = 0; i < set_number; i++) {
    live_affected_sets[i] = std::unordered_set<Node*>();
    deleted_affected_sets[i] = std::unordered_set<Node*>();
    old_live_affected_sets[i] = std::unordered_set<Node*>();
    old_deleted_affected_sets[i] = std::unordered_set<Node*>();
  }

  for (auto p : del) {
    int v = p.first;
    for (auto u : p.second) {
      if (u.second) {
        lists[v]->head->set_parent(lists[v]->head);
      } else {
        lists[v]->head->remove_child(lists[u.first]->head);
      }
    }
  }

  for (auto p : add) {
    int v = p.first;
    for (auto u : p.second) {
      if (u.second) {
        lists[v]->head->set_parent(lists[u.first]->head);
      } else {
        lists[v]->head->add_child(lists[u.first]->head);
      }
    }
  }

  int id = 0;
  for (auto p : del) {
    make_affected(lists[p.first]->head, id++, false);
  }
  for (auto p : add) {
    make_affected(lists[p.first]->head, id++, false);
  }
}

void initialization_update(int n, int add_no, int* add_p, int* add_v, int delete_no, int* delete_p, int* delete_v) {
  std::unordered_map<int, std::vector<std::pair<int, bool>>> add;
  std::unordered_map<int, std::vector<std::pair<int, bool>>> del;

  for (int i = 0; i < add_no; i++) {
    if (add.count(add_p[i]) == 0) {
      add.insert({add_p[i], std::vector<std::pair<int, bool>>()});
    }
    add[add_p[i]].push_back({add_v[i], false});
    if (add.count(add_v[i]) == 0) {
      add.insert({add_v[i], std::vector<std::pair<int, bool>>()});
    }
    add[add_v[i]].push_back({add_p[i], true});
  }

  for (int i = 0; i < delete_no; i++) {
    if (del.count(delete_p[i]) == 0) {
      del.insert({delete_p[i], std::vector<std::pair<int, bool>>()});
    }
    del[delete_p[i]].push_back({delete_v[i], false});
    if (del.count(delete_v[i]) == 0) {
      del.insert({delete_v[i], std::vector<std::pair<int, bool>>()});
    }
    del[delete_v[i]].push_back({delete_p[i], true});
  }
  initialization_update(n, add, del);
}

void update_round_seq(int round) {
  std::unordered_set<Node*> old_live_affected_set;
  std::unordered_set<Node*> old_deleted_affected_set;
  old_live_affected_set.swap(live_affected_sets[0]);
  old_deleted_affected_set.swap(deleted_affected_sets[0]);
  for (Node* v : old_live_affected_set) {
    if (is_contracted(v, round)) {
      v->set_contracted(true);
      if (on_frontier(v)) {
        make_affected(v->get_parent(), 0, true);
        for (Node* child : v->get_children()) {
          make_affected(child, 0, true);
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

void update_round(int round) {
  for (int i = 0; i < set_number; i++) {
    old_live_affected_sets[i].clear();
    live_affected_sets[i].swap(old_live_affected_sets[i]);
    old_deleted_affected_sets[i].clear();
    deleted_affected_sets[i].swap(old_deleted_affected_sets[i]);
  }

  for (int i = 0; i < set_number; i++) {
    for (Node* v : old_live_affected_sets[i]) {
      if (is_contracted(v, round)) {
        v->set_contracted(true);
        if (vertex_thread[v->get_parent()->get_vertex()] == -1)
          v->get_parent()->set_proposal(v, i);
        for (Node* c : v->get_children()) {
           if (vertex_thread[c->get_vertex()] == -1)
             c->set_proposal(v, i);
        }
      } else if (v->is_root()) {
        v->set_root(true);
      } else {
        copy_node(v->get_vertex());
        live_affected_sets[i].insert(v->next);
      }
    }
  }

  for (int i = 0; i < set_number; i++) {
    for (Node* v : old_live_affected_sets[i]) {
      if (is_contracted(v, round)) {
        if (get_thread_id(v->get_parent()) == i) {
          make_affected(v->get_parent(), i, true);
        }
        for (Node* u : v->get_children()) {
          if (get_thread_id(u) == i)
            make_affected(u, i, true);
        }
      }
    }
  }

  for (int i = 0; i < set_number; i++) {
    for (Node* v : live_affected_sets[i]) {
      if (v->get_parent()->is_contracted()) {
        delete_node_for(v->get_parent(), v);
      }
      for (Node* c : v->get_children()) {
        if (c->is_contracted()) {
          delete_node_for(c, v);
        }
      }
    }
  }

  for (int i = 0; i < set_number; i++) {
    for (Node* v : live_affected_sets[i]) {
      v->advance();
    }
  }

  for (int i = 0; i < set_number; i++) {
    for (Node* v : old_deleted_affected_sets[i]) {
      if (v->next != NULL)
        deleted_affected_sets[i].insert(v->next);
      delete v;
    }
  }
}

bool end_condition_seq() {
  return live_affected_sets[0].size() + deleted_affected_sets[0].size();
}

bool end_condition() {
  return pbbs::sequence::plusReduce(ids, set_number, [&] (int i) { return live_affected_sets[i].size() + deleted_affected_sets[i].size();});
}

template <typename Round, typename Condition>
void update(int n, Round round_function, Condition condition_function) {
  int round_no = 0;
  while (condition_function() > 0) {
    round_function(round_no);
    round_no++;
  }

  std::cerr << "Number of rounds: " << round_no << std::endl;
}