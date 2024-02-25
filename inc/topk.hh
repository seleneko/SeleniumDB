/**
 * @file topk.hh
 * @author Selene
 * @brief Top K 问题的解决.
 * @version 0.2
 * @date 2021-04-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_TOPK_HH_
#define INC_TOPK_HH_

#include <fmt/core.h>

#include <algorithm>
#include <functional>
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

namespace ndb {

/**
 * @brief Top K 问题所需的键.
 *
 */
struct TkKey {
  TkKey() {}
  TkKey(size_t key, int64_t id) : key(key), id(id) {}

  bool operator<(const TkKey &t) const { return key < t.key; }
  bool operator<=(const TkKey &t) const { return key <= t.key; }
  bool operator==(const TkKey &t) const { return key == t.key; }

  size_t key;
  int64_t id = -1;
};

/**
 * @brief Top K 问题所需的值.
 *
 */
struct TkRecord {
  TkRecord() {}
  TkRecord(uint32_t c, const char *n) : count(c) {
    snprintf(tkname, sizeof(tkname), "%s", n);
  }

  bool operator<(const TkRecord &t) const { return count < t.count; }
  bool operator<=(const TkRecord &t) const { return count <= t.count; }
  bool operator==(const TkRecord &t) const { return count == t.count; }
  bool operator>(const TkRecord &t) const { return count > t.count; }
  bool operator>=(const TkRecord &t) const { return count >= t.count; }
  bool operator!=(const TkRecord &t) const { return count != t.count; }

  uint32_t count = 0;
  char tkname[64];
};

class TopK {
 public:
  /**
   * @brief 初始化.
   *
   * @param tname 文件名
   * @param new_file 是否新建文件
   */
  void init_topk(std::string tname, bool new_file);

  /**
   * @brief 插入一个键.
   *
   * @param word
   */
  void insert(std::string word);

  /**
   * @brief 解决问题.
   *
   * @param N
   *
   */
  void make_topk(int16_t N);

  /**
   * @brief
   *
   * @param K
   */
  void print(int16_t K);

 private:
  int id = 0;
  std::shared_ptr<ndb::Pager> page_manager;
  std::shared_ptr<ndb::Pager> record_manager;
  std::shared_ptr<ndb::BplusTree<TkKey, 64>> bt;
  std::hash<std::string> hash_fn;
  std::vector<TkRecord> vec;
};

TopK topk_manager{};

void TopK::init_topk(std::string tname, bool new_file) {
  auto idx = fmt::format("database/{0}/{0}_topk_idx.bin", tname);
  auto rec = fmt::format("database/{0}/{0}_topk_rec.bin", tname);
  page_manager = std::make_shared<ndb::Pager>(idx, new_file);
  record_manager = std::make_shared<ndb::Pager>(rec, new_file);
  bt = std::make_shared<ndb::BplusTree<TkKey, 64>>(page_manager);
  Record s;
  id = record_manager->get_id(&s);
}

void TopK::insert(std::string word) {
  auto pphash = hash_fn(word);
  int cnt = 1;
  auto iter = bt->find({pphash, -1});
  TkRecord r;
  record_manager->recover(iter->id, &r);
  if (word != r.tkname) {
    bt->insert({pphash, id});  // fixme:...
    TkRecord r(1, word.c_str());
    record_manager->save(id, &r);
    id++;
  } else {
    r.count = r.count + 1;
    //// printf("%s = %d\n", word.c_str(), r.count);
    record_manager->save(iter->id, &r);
  }
}

void TopK::make_topk(int16_t N) {
  TkRecord t;
  for (int i = 0; i < record_manager->get_id(&t); i++) {
    TkRecord r;
    record_manager->recover(i, &r);
    vec.push_back(r);
    std::make_heap(vec.begin(), vec.end(), std::greater<TkRecord>());
    while (vec.size() > N) {
      std::make_heap(vec.begin(), vec.end(), std::greater<TkRecord>());
      std::pop_heap(vec.begin(), vec.end());
      vec.pop_back();
    }
  }
}

void TopK::print(int16_t K) {
  sort(vec.begin(), vec.end(), std::greater<TkRecord>());
  for (auto i = 0; i < K; i++) {
    auto num = fmt::format("[{}] ", i + 1);
    fmt::print(fg(fmt::terminal_color::bright_blue), "{:>5}", num);
    fmt::print("{} ({})\n", vec[i].tkname, vec[i].count);
  }
}

}  // namespace ndb

#endif  // INC_TOPK_HH_
