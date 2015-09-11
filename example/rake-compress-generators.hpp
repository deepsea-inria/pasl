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

void generate_empty_graph(int n, std::vector<int>* children, int* parent) {
  for (int i = 0; i < n; i++) {
    parent[i] = i;
    children[i] = std::vector<int>();
  }
}

void generate_graph(std::string type, int n, std::vector<int>* children, int* parent) {
  if (type.compare("binary_tree") == 0) {
    // let us firstly think about binary tree
    generate_binary_tree(n, children, parent);
  } else if (type.compare("bamboo") == 0) {
    // bambooooooo
    generate_bamboo(n, children, parent);
  } else {
    generate_empty_graph(n, children, parent);
  }
}

#endif