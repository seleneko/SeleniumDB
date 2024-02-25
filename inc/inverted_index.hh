/**
 * @file inverted_index.hh
 * @author Selene
 * @brief 基于 B+ 树的倒排索引.
 * @version 0.2
 * @date 2021-03-02
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_INVERTED_INDEX_HH_
#define INC_INVERTED_INDEX_HH_

#include <fmt/core.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bptree.hh"
#include "util.hh"

#define ALL(x) x.begin(), x.end()
#define INSERT(x) inserter(x, x.begin())

namespace ndb {

/**
 * @brief 用于倒排索引的 Key 类
 * todo: 或许可以用模板类来实现?
 */
struct IvKey {
  IvKey() {}
  IvKey(size_t key, int64_t id) : key(key), id(id) {}

  bool operator<(const IvKey &t) const { return key < t.key; }
  bool operator<=(const IvKey &t) const { return key <= t.key; }
  bool operator==(const IvKey &t) const { return key == t.key; }

  size_t key;
  int64_t id = -1;
};

/**
 * @brief 倒排索引. 将每个单词的 Hash 值存入 B+ 树中.
 *
 */
class InvertedIndex {
  using string_list = std::vector<std::string>;
  using result_set = std::set<std::pair<u_int32_t, uint32_t>>;
  using result_set_list = std::vector<result_set>;

 public:
  /**
   * @brief InvertedIndex 的构造函数.
   *
   */
  InvertedIndex();

  void init_ii(std::string iiname, bool new_file);

  /**
   * @brief 从包含若干单词的一条记录建立倒排索引.
   *
   * @param source 待建立索引的单词列表.
   * @param pos XML 文件中的位置, 作为开始.
   * @param len XML 文件从起始位置开始的长度.
   */
  void build(string_list source, uint32_t pos, uint32_t len);

  /**
   * @brief 查询索引, 取这些单词索引指向的位置的交集.
   *
   * @param value_list 待查询的单词列表.
   */
  auto find(string_list value_list)
      -> std::vector<std::pair<Record, std::string>>;

  Property<std::string> dbname{"null"};

 private:
  /**
   * @brief 建立一个单词的倒排索引.
   *
   * @param key 这个单词.
   * @param pos XML 文件中的位置, 作为开始.
   * @param len XML 文件从起始位置开始的长度.
   */
  void insert(std::string key, uint32_t pos, uint32_t len);

  /**
   * @brief 取一组 result_set 的交集.
   *
   * @param result_list 一组 result_set.
   * @return result_list 的交集.
   */
  auto intersection(result_set_list result_list) -> result_set;

  /**
   * @brief 查询单个单词, 返回查询结果.
   *
   * @param v 待查询的单个单词.
   * @return 查询结果.
   */
  auto find_single_value(std::string v) -> result_set;

  int id = 0;
  std::shared_ptr<ndb::Pager> page_manager;
  std::shared_ptr<ndb::Pager> record_manager;
  std::shared_ptr<ndb::BplusTree<IvKey, 64>> bt;
  std::hash<std::string> hash_fn;
};

#pragma region  // InvertedIndex

InvertedIndex::InvertedIndex() {
  //
}

void InvertedIndex::init_ii(std::string iiname, bool new_file) {
  auto idx = fmt::format("database/{0}/{0}_ii_idx.bin", iiname);
  auto rec = fmt::format("database/{0}/{0}_ii_rec.bin", iiname);
  page_manager = std::make_shared<ndb::Pager>(idx, new_file);
  record_manager = std::make_shared<ndb::Pager>(rec, new_file);
  bt = std::make_shared<ndb::BplusTree<IvKey, 64>>(page_manager);
  Record s;
  id = record_manager->get_id(&s);
}

void InvertedIndex::build(string_list source, uint32_t pos, uint32_t len) {
  for (int i = 0; i < source.size(); i++) {
    insert(source[i], pos, len);
  }
}

auto InvertedIndex::find(string_list value_list)
    -> std::vector<std::pair<Record, std::string>> {
  fmt::print("Search for ");
  fmt::print(fg(fmt::terminal_color::bright_green), "{}",
             fmt::join(value_list, " + "));
  fmt::print(":\n");

  // 如需要搜索多个关键字, 应该取它们结果的交集.
  result_set_list result_list;
  for (auto v : value_list) {
    result_list.push_back(find_single_value(v));
  }
  auto result_intersection = intersection(result_list);

  std::vector<std::pair<Record, std::string>> results;
  for (auto i : result_intersection) {
    std::string key;
    results.push_back({Record{i.first, i.second}, key});
  }
  //// fmt::print("{} record(s) found.\n", cnt);
  return results;
}

void InvertedIndex::insert(std::string key, uint32_t pos, uint32_t len) {
  std::stringstream in(key);
  std::string word;
  while (in >> word) {
    auto pphash = hash_fn(word);
    Record r{pos, len};
    record_manager->save(id, &r);
    bt->insert({pphash, id});  // fixme
    id++;
  }
}

auto InvertedIndex::intersection(result_set_list result_list) -> result_set {
  result_set result_intersection;
  if (result_list.size() >= 2) {
    set_intersection(ALL(result_list[0]), ALL(result_list[1]),
                     INSERT(result_intersection));
    for (auto i = 2; i < result_list.size(); i++) {
      auto temp = result_intersection;
      result_intersection.clear();
      set_intersection(ALL(result_list[i]), ALL(temp),
                       INSERT(result_intersection));
    }
  } else {
    result_intersection = result_list[0];
  }
  return result_intersection;
}

auto InvertedIndex::find_single_value(std::string v) -> result_set {
  result_set result;
  auto hash_code = hash_fn(v);
  IvKey k(hash_code - 1, -1);
  auto iter = bt->find_geq(k);
  while (true) {
    if (iter->key == hash_code) {
      Record s;
      record_manager->recover(iter->id, &s);
      result.insert({s.pos, s.len});
      iter++;
      continue;
    }
    break;
  }
  return result;
}

InvertedIndex invidx_manager{};

#pragma endregion

};  // namespace ndb

#endif  // INC_INVERTED_INDEX_HH_
