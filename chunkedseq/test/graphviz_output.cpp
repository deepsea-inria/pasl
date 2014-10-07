/*!
 * \author Umut A. Acar
 * \author Arthur Chargueraud
 * \author Mike Rainey
 * \date 2013-2018
 * \copyright 2014 Umut A. Acar, Arthur Chargueraud, Mike Rainey
 *
 * \brief Unit tests for our chunked sequence
 * \file quickcheck_chunkedseq.cpp
 *
 */

#include <string>
#include <set>

#include "cmdline.hpp"
#include "chunkedseq.hpp"

int main(int argc, char** argv) {
  pasl::util::cmdline::set(argc, argv);

  using string_type = pasl::data::chunkedseq::bootstrapped::deque<char,4>;
  using vertex_set_type = std::set<const void*>;

  string_type c;

  std::string str = "Functions delay binding; data structures induce binding. Moral: Structure data late in the programming process.";

  for (int i = 0; i < str.size(); i++)
    c.push_back(str[i]);

  for (int i = 0; i < 500; i++) {
    string_type d;
    size_t k = rand() % c.size();
    c.split(k, d);
    c.concat(d);
  }

  /*
  for (int i = 0; i < c.size(); i++)
    std::cout << c[i];
  std::cout << std::endl;
  */

  vertex_set_type vtxset;

  auto add_edge = [&] (const void* src, const void* dst) {
    vtxset.insert(src);
    vtxset.insert(dst);
    std::cout << (long)dst << " -- " << (long)src << std::endl;
  };

  auto process_chunk = [&] <typename Chunk> (Chunk* c) {
    std::string str;
    c->for_each([&] (char c) {
      str += c;
    });
    auto it = vtxset.find(c);
    vtxset.erase(it);
    std::cout << (long)c << "[shape=record,label=\"" << str << "\"]" << std::endl;
  };

  std::cout << "graph g{" << std::endl;
  std::cout << "rankdir=LR" << std::endl;
  std::cout << "ratio=auto" << std::endl;
  c.reveal_internal_structure(add_edge, process_chunk);
  for (auto it = vtxset.begin(); it != vtxset.end(); it++)
    std::cout << (long)*it << "[shape=record,label=\"\"]" << std::endl;
  std::cout << "}" << std::endl;

  return 0;
}
