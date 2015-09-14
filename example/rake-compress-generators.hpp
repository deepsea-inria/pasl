#include<vector>
#include <string>

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

void generate_k_bamboos(int n, int k, std::vector<int>* children, int* parent) {
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

void generate_graph(std::string type, int n, std::vector<int>* children, int* parent, int k = 0) {
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
  } else {
    generate_k_bamboos(n, k, children, parent);
  }
}

#endif