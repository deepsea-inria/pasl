#include "cmdline.hpp"
#include "benchmark.hpp"
#include "rake-compress-primitives.hpp"

void round(int round) {
//  std::cerr << live.size() << " " << live << std::endl;
//  for (int i  =0; i < live.size(); i++) {
//    std::cerr << "link from " << i << " : ";
//    for (Node* node : lists[i]->get_children()) {
//      std::cerr << node << " ";
//    }
//    std::cerr << "\nparent " << lists[i]->get_parent() << std::endl;
//  }
  if (round % 100 == 0) {
    std::cerr << round << " " << len[round % 2] << std::endl;
  }
//  print_array(live[round % 2], len[round % 2]);

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

//  print_array(live[1 - round % 2], len[1 - round % 2]);

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
  });
}

void round_seq(int round) {
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

template<typename Round>
void construction(int n, Round round_function) {
  int round_no = 0;
  while (len[round_no % 2] > 0) {
    round_function(round_no);
    round_no++;
  }

  int* roots = new int[n];
  for (int i = 0; i < n; i++) {
    roots[i] = i;
  }

  int* result = new int[n];
  int roots_number = pbbs::sequence::filter(roots, result, n, [&] (int v) {
    return lists[v]->is_known_root();
  });
  std::cout << "Number of rounds: " << round_no << std::endl;
  std::cout << "number of roots: " << roots_number << std::endl;
  for (int i = 0; i < roots_number; i++)
    std::cout << result[i] << " ";
  std::cout << std::endl;
}

int main(int argc, char** argv) {
   bool seq;
   int n;
   auto init = [&] {
     n = (long)pasl::util::cmdline::parse_or_default_int("n", 24);
     std::string graph = pasl::util::cmdline::parse_or_default_string("graph", std::string("bamboo"));
     seq = pasl::util::cmdline::parse_or_default_int("seq", 1) == 1;

     std::vector<int>* children = new std::vector<int>[n];
     int* parent = new int[n];
     if (graph.compare("binary_tree") == 0) {
       // let us firstly think about binary tree
       for (int i = 0; i < n; i++) {
         parent[i] = i == 0 ? 0 : (i - 1) / 2;
         children[i] = std::vector<int>();
         if (2 * i + 1 < n) {
           children[i].push_back(2 * i + 1);
         }
         if (2 * i + 2 < n) {
           children[i].push_back(2 * i + 2);
         }
       }
     } else {
       // bambooooooo
       for (int i = 0; i < n; i++) {
         parent[i] = i == 0 ? 0 : i - 1;
         children[i] = std::vector<int>();
         if (i + 1 < n)
           children[i].push_back(i + 1);
       }
     }
  
     initialization(n, children, parent);
   };

   auto run = [&] (bool sequential) {
     if (seq) {
       std::cerr << "Sequential run" << std::endl;
       construction(n, [&] (int round_no) {round_seq(round_no);});
     } else {
       std::cerr << "Parallel run" << std::endl;
       construction(n, [&] (int round_no) {round(round_no);});
     }
   };

   auto output = [&] {
     std::cout << "the construction has finished." << std::endl;
   };

   auto destroy = [&] {
     ;
   };

   pasl::sched::launch(argc, argv, init, run, output, destroy);

   return 0;
}                