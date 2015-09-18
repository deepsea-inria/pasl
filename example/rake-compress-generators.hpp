#include<vector>
#include <string>
#include <unordered_set>

#ifndef _RAKE_COMPRESS_GENERATORS_
#define _RAKE_COMPRESS_GENERATORS_

void generate_binary_tree(int n, std::vector<int>* children, int* parent) {
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
}

void generate_bamboo(int n, std::vector<int>* children, int* parent)  {
  for (int i = 0; i < n; i++) {
    parent[i] = i == 0 ? 0 : i - 1;
    children[i] = std::vector<int>();
    if (i + 1 < n)
      children[i].push_back(i + 1);
  }
}

void generate_two_bamboos(int n, std::vector<int>* children, int* parent) {
  int l1 = n / 2;
  int l2 = n - l1;
  for (int i = 0; i < l1; i++) {
    parent[i] = i == 0 ? 0 : i - 1;
    children[i] = std::vector<int>();
    if (i + 1 < l1)
      children[i].push_back(i + 1);
  }
  for (int i = 0; i < l2; i++) {
    parent[i + l1] = (i == 0 ? 0 : i - 1) + l1;
    children[i + l1] = std::vector<int>();
    if (i + 1 < l2)
      children[i + l1].push_back(i + l1 + 1);
  }
}

void generate_k_bamboos(int n, std::vector<int>* children, int* parent, int k) {
  for (int i = 0; i < k; i++) {
    int l = i == k - 1 ? n - (k - 1) * (n / k) : n / k;
    int d = i * (n / k);
    for (int i = 0; i < l; i++) {
      parent[i + d] = (i == 0 ? 0 : i - 1) + d;
      children[i + d] = std::vector<int>();
      if (i + 1 < l)
        children[i + d].push_back(i + d + 1);
    }
  }
}

void generate_empty_graph(int n, std::vector<int>* children, int* parent) {
  for (int i = 0; i < n; i++) {
    parent[i] = i;
    children[i] = std::vector<int>();
  }
}

void remove_edge(std::vector<int>* children, int* parent, int u, int v) {
  parent[v] = -1;
  children[u].erase(std::remove(children[u].begin(), children[u].end(), v), children[u].end());
}

void add_edge(std::vector<int>* children, int* parent, int u, int v) {
  parent[v] = u;
  children[u].push_back(v);
}

void generate_random_graph(int n, std::vector<int>* children, int* parent, int k, int seed, int degree, double f) {
  for (int i = 0; i < n; i++) {
    parent[i] = i;
  }

  int each = 3;
  int r = std::max(n - (int)ceil(n * f), 2);

  std::cerr << n << " " << degree << " " << r << std::endl;

  srand(seed);

  for (int i = 1; i < r; i++) {
//    if (rand() % each == 1 || i < each) {
      add_edge(children, parent, i / degree, i);
//    }
  }

  for (int i = r; i < n; i++) {
    int u = rand() % i;
    while (children[u].size() == 0) {
      u = rand() % i;
    }

    int v = children[u][rand() % children[u].size()];
    remove_edge(children, parent, u, v);
    add_edge(children, parent, u, i);
    add_edge(children, parent, i, v);
  }

  int cnt = 0;
  for (int i = 0; i < n; i++) {
    if (children[i].size() == 1) {
      cnt++;
    }
  }

  std::cout << "done. " << ((double) cnt / n * 100) << "% of vertices have degree 2." << std::endl;
}

struct hasher {
  std::size_t operator()(const std::pair<int, int>& val) const {
    return val.first * 1000007 + val.second;
  }
};

void choose_edges(int n, std::vector<int>* children, int* parent, int k, int* p, int* v, int seed) {
  std::unordered_set<std::pair<int, int>, hasher> taken;
  for (int i = 0; i < k; i++) {
    int u = rand() % n;
    int w = -1;
    while (true) {
      if (children[u].size() > 0) {
        int start = rand() % children[u].size();
        for (int j = 0; j < children[u].size(); j++) {
          int c = children[u][(start + j) % children[u].size()];
          if (taken.count({ u, c }) == 0) {
            w = c;
            break;
          }
        }
        if (w != -1) {
          break;
        }
      }
      u = rand() % n;
    }
    taken.insert({ u, w });
    p[i] = u;
    v[i] = w;
  }
}

void remove_edges(int n, std::vector<int>* old_children, int* old_parent, std::vector<int>* new_children, int* new_parent,
                int k, int* p, int* v) {
  for (int i = 0; i < n; i++) {
    new_children[i] = old_children[i];
    new_parent[i] = old_parent[i];
  }

  for (int i = 0; i < k; i++) {
    new_parent[v[i]] = v[i];
    new_children[p[i]].erase(std::remove(new_children[p[i]].begin(), new_children[p[i]].end(), v[i]), new_children[p[i]].end());
  }
}

void generate_graph(std::string type, int n, std::vector<int>* children, int* parent, int k = 1, int seed = 239, int degree = 4, double f = 0) {
  if (type.compare("binary_tree") == 0) {
    // let us firstly think about binary tree
    generate_binary_tree(n, children, parent);
  } else if (type.compare("bamboo") == 0) {
    // bambooooooo
    generate_bamboo(n, children, parent);
  } else if (type.compare("empty_graph") == 0) {
    generate_empty_graph(n, children, parent);
  } else if (type.compare("two_bamboos") == 0) {
    generate_two_bamboos(n, children, parent);
  } else if (type.compare("k_bamboos") == 0) {
    generate_k_bamboos(n, children, parent, k);
  } else {
    generate_random_graph(n, children, parent, k, seed, degree, f);
  }
}

#endif