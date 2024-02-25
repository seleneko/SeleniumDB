/**
 * @file database.hh
 * @author Selene
 * @brief 数据库的外层抽象.
 * @version 0.2
 * @date 2021-02-20
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_DATABASE_HH_
#define INC_DATABASE_HH_

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <libxml/SAX2.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bptree.hh"
#include "inverted_index.hh"
#include "topk.hh"
#include "util.hh"

namespace ndb {

enum class DatabaseState {
  AUTHOR,
  TITLE,
};

/**
 * @brief 键.
 *
 */
struct Key {
  Key() {}
  explicit Key(int64_t id) : id(id) {}

  bool operator<(const Key &t) const { return strcmp(key, t.key) < 0; }
  bool operator<=(const Key &t) const { return strcmp(key, t.key) <= 0; }
  bool operator==(const Key &t) const { return strcmp(key, t.key) == 0; }
  std::ostream &operator<<(std::ostream &out) {
    out << key;
    return out;
  }

  char key[64];
  int64_t id = -1;
};

class Database {
  friend class CommandLine;

 public:
  /**
   * @brief 前缀匹配搜索
   * @param value 待搜索的字符串.
   * @return 搜索结果序列.
   *
   */
  auto find(std::string value, DatabaseState state)
      -> std::vector<std::pair<Record, std::string>>;

  /**
   * @brief 在搜索结果中选中一条并打印.
   * @param k 搜索返回序列中的索引.
   * @return 搜索结果序列.
   *
   */
  void select_in(std::vector<std::pair<Record, std::string>> results);

  /**
   * @brief 简单地插入一条 key-value 对
   * @param r 值.
   * @param key 键.
   * todo: 可读性需要增强.
   */
  void insert(Record r, std::string key, DatabaseState state);

  /**
   * @brief 选择所有数据并打印, 打印上限为 64 条.
   * ! 本来是测试的时候用的.
   */
  void select(DatabaseState state);

  /**
   * @brief 模糊搜索.
   * @param value_list 一个字符串序列, 即待搜索的内容.
   * @return 搜索结果序列.
   * todo: 需要加入返回值.
   */
  void search(std::vector<std::string> value_list);

  void topk(int16_t k);

  /**
   * @brief 打开一个数据库.
   * @param name 数据库名.
   * @param new_file 是否新建文件.
   * todo: 感觉用子数据库的逻辑有问题, 待修改.
   */
  void db_open(std::string name, bool new_file);

  /**
   * @brief 关闭一个数据库.
   *
   */
  void db_close();

  ndb::Property<bool> is_open{false};
  ndb::Property<std::string> name{"24601"};

 private:
  /**
   * 打印 DOM 树中的某一结点.
   * @param doc 指向 DOM 树的指针.
   * @param cur 指向 DOM 树中当前结点的指针.
   */
  void print_nodes(xmlDocPtr doc, xmlNodePtr cur);

  /**
   * 解析 XML 文件中的一部分并打印整棵 DOM 树.
   * @param file_name XML 源文件.
   * @param pos XML 文件中的位置, 作为开始.
   * @param len XML 文件从起始位置开始的长度.
   */
  void print_dom_tree(const char *file_name, int pos, int len);

  struct SubDatabase {
    int id = 0;
    std::shared_ptr<ndb::Pager> page_manager;
    std::shared_ptr<ndb::Pager> record_manager;
    std::shared_ptr<ndb::BplusTree<Key, 64>> bt;
  };
  std::shared_ptr<SubDatabase> title = std::make_shared<SubDatabase>();
  std::shared_ptr<SubDatabase> author = std::make_shared<SubDatabase>();
};

#pragma region  // # Database Implementation

auto Database::find(std::string value, DatabaseState state)
    -> std::vector<std::pair<Record, std::string>> {
  std::shared_ptr<SubDatabase> here;
  switch (state) {
    case DatabaseState::AUTHOR: {
      here = author;
      break;
    }
    case DatabaseState::TITLE: {
      here = title;
      break;
    }
    default: {
      break;
    }
  }
  Key k(-1);
  snprintf(k.key, sizeof(k.key), "%s", value.c_str());
  auto iter = here->bt->find_geq(k);
  std::vector<std::pair<Record, std::string>> results;
  while (true) {
    if (std::string(iter->key).find(value) == 0) {
      Record s;
      here->record_manager->recover(iter->id, &s);
      results.push_back({s, iter->key});
      //// fmt::print("({}, {})\n", s.pos, s.len);
      //// print_dom_tree("xml/small.xml", s.pos, s.len + 1);
      iter++;
      continue;
    }
    break;
  }
  select_in(results);
  //// fmt::print("{} record(s) found.\n", cnt);
  return results;
}

void Database::search(std::vector<std::string> value_list) {
  // todo: 需要改进?
  std::vector<std::pair<Record, std::string>> v =
      invidx_manager.find(value_list);
  select_in(v);
}

void Database::select_in(std::vector<std::pair<Record, std::string>> results) {
  for (int i = 0; i < results.size(); i++) {
    auto num = fmt::format("[{}] ", i + 1);
    fmt::print(fg(fmt::terminal_color::bright_blue), "{:>5}", num);
    auto dd = fmt::format("{:-^55}", "");
    fmt::print(fg(fmt::terminal_color::bright_blue), "{}\n", dd);
    auto r = results[i].first;
    print_dom_tree("xml/small.xml", r.pos, r.len + 1);
    auto div = fmt::format("{:-^60}", "");
    fmt::print(fg(fmt::terminal_color::bright_blue), "{}\n", div);
  }
}

void Database::insert(Record r, std::string key, DatabaseState state) {
  std::shared_ptr<SubDatabase> here;
  switch (state) {
    case DatabaseState::AUTHOR: {
      here = author;
      break;
    }
    case DatabaseState::TITLE: {
      here = title;
      break;
    }
    default: {
      break;
    }
  }
  here->record_manager->save(here->id, &r);
  Key k(here->id);
  snprintf(k.key, sizeof(k.key), "%s", key.c_str());
  here->bt->insert(k);
  here->id++;
}

void Database::select(DatabaseState state) {
  std::shared_ptr<SubDatabase> here;
  switch (state) {
    case DatabaseState::AUTHOR: {
      here = author;
      break;
    }
    case DatabaseState::TITLE: {
      here = title;
      break;
    }
    default: {
      break;
    }
  }
  here->bt->print();
}

void Database::topk(int16_t k) { topk_manager.print(k); }

void Database::db_open(std::string name, bool new_file) {
  this->name = name;
  is_open = true;
  if (new_file) {
    system(fmt::format("{} database/{}", ndb::MKDIR, name).c_str());
  }
  invidx_manager.init_ii(name, new_file);
  topk_manager.init_topk(name, new_file);
  auto idx_title = fmt::format("database/{0}/{0}_idx_title.bin", name);
  auto rec_title = fmt::format("database/{0}/{0}_rec_title.bin", name);
  title->page_manager = std::make_shared<ndb::Pager>(idx_title, new_file);
  title->record_manager = std::make_shared<ndb::Pager>(rec_title, new_file);
  title->bt = std::make_shared<ndb::BplusTree<Key, 64>>(title->page_manager);

  auto idx_author = fmt::format("database/{0}/{0}_idx_author.bin", name);
  auto rec_author = fmt::format("database/{0}/{0}_rec_author.bin", name);
  author->page_manager = std::make_shared<ndb::Pager>(idx_author, new_file);
  author->record_manager = std::make_shared<ndb::Pager>(rec_author, new_file);
  author->bt = std::make_shared<ndb::BplusTree<Key, 64>>(author->page_manager);

  Record s;
  title->id = title->record_manager->get_id(&s);
  author->id = author->record_manager->get_id(&s);
}

void Database::db_close() { is_open = false; }

void Database::print_nodes(xmlDocPtr doc, xmlNodePtr cur) {
  std::string last;
  bool has_key = true;
  while (cur != nullptr) {
    // 对 last 的检查是为了在单个标签多个元素的情况下不重复打印标签.
    // 比如一篇文章有多个 author, 只打印一个 author.
    // todo: 这里用 12 + 5 来实现对齐过于野蛮, 需要改进.
    auto name = std::string(reinterpret_cast<const char *>(cur->name));
    last == name ? fmt::print("{:<{}}", "", 12 + 5)
                 : fmt::print(fg(fmt::terminal_color::bright_cyan), "{:<{}}",
                              fmt::format("     <{}>", name).c_str(), 12 + 5);

    auto xckey = xmlNodeListGetString(doc, cur->children, 1);
    if (xckey != nullptr) {
      auto key = std::string(reinterpret_cast<const char *>(xckey));
      fmt::print("{}\n", key);
      xmlFree(xckey);
    } else {
      has_key = false;
    }
    last = name;

    // 这部分是用于打印标签的属性, 打印的时候类似元素.
    auto attr = cur->properties;
    while (attr != nullptr) {
      auto name = std::string(reinterpret_cast<const char *>(attr->name));
      auto xcvalue = xmlNodeListGetString(doc, attr->children, 1);
      auto value = std::string(reinterpret_cast<const char *>(xcvalue));
      fmt::print("{:<{}}", "", has_key == false ? 0 : 12 + 5);
      fmt::print("{} = {}\n", name, value);
      xmlFree(xcvalue);
      has_key = true;
      attr = attr->next;
    }
    cur = cur->next;
  }
}

void Database::print_dom_tree(const char *file_name, int pos, int len) {
  auto str = new char[len];
  auto p = str;
  auto file = fopen(file_name, "r");
  fseek(file, pos, 0);

  // 一个字符一个字符地读取内容, 所幸单条数据的量不是很大.
  for (int i = 0; i < len; i++) {
    *p++ = fgetc(file);
  }
  *--p = '\0';
  fclose(file);

  // 我对 Libxml 的理解不够透彻, 所以经常出现文件指针错位的情况.
  if (*str != '<') {
    return;
  }

  auto doc = xmlParseMemory(str, len);
  auto cur = xmlDocGetRootElement(doc);

  // 这是建立在确信生成的 DOM 树不超过两层的基础上的. 也最多打印两层.
  // fixme: 本来想直接遍历树的, 但是疑似有点问题.
  print_nodes(doc, cur);
  print_nodes(doc, cur->children);
  delete[] str;
}

Database db;

#pragma endregion

};  // namespace ndb

#endif  // INC_DATABASE_HH_
