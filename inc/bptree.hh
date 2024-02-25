/**
 * @file bptree.hh
 * @author Selene
 * @brief B+ 树的本体实现, 用 Pager 作为中介实现磁盘 I/O.
 * @version 0.2
 * @date 2021-02-23
 * todo: B+ 树的注释没写好...
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_BPTREE_HH_
#define INC_BPTREE_HH_

#include <fmt/color.h>
#include <fmt/core.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "util.hh"

namespace ndb {

/**
 * @brief B+ 树和硬盘读写的中间层, 借助此类来完成对磁盘上某条数据的增删查操作.
 *
 */
class Pager : protected std::fstream {
 public:
  ndb::Property<bool> empty;

  /**
   * @brief Pager 的构造函数.
   *
   * @param file_name 待保存或读取的文件名.
   * @param create 是否新建文件.
   * todo: create 可以改成 new_file.
   */
  explicit Pager(std::string file_name, bool create = false);

  /**
   * @brief Pager 的析构函数.
   *
   */
  ~Pager();

  /**
   * @brief 获得一条数据的 ID.
   *
   * @tparam Register Pager 读写的类.
   * @param reg 一条数据.
   * @return int64_t 一条数据的 ID.
   */
  template <class Register>
  inline auto get_id(Register* reg) -> int64_t;

  /**
   * @brief 把一条数据保存进文件.
   *
   * @tparam Register Pager 读写的类.
   * @param n 在位置为 n 处保存.
   * @param reg 一条数据.
   */
  template <class Register>
  inline void save(const int64_t& n, Register* reg);

  /**
   * @brief 从文件中读取一条数据.
   *
   * @tparam Register Pager 读写的类.
   * @param n 在位置为 n 处获取.
   * @param reg 一条数据.
   * @return true 如果读取成功.
   * @return false 如果读取失败.
   */
  template <class Register>
  inline bool recover(const int64_t& n, Register* reg);

  /**
   * @brief 从文件中删除一条数据. 其实是在 n 处打上文件已删除的标记.
   *
   * @tparam Register
   * @param n 在位置为 n 处删除.
   */
  template <class Register>
  inline void erase(const int64_t& n);
};

/**
 * @brief B+ 树中的一个结点. 提供了各种结点内部基本操作.
 *
 * @tparam T Node 存储的类型.
 * @tparam ORDER B+ 树的序.
 */
template <class T, int16_t ORDER>
class Node {
  template <class X, int16_t Y>
  friend class Iterator;
  template <class X, int16_t Y>
  friend class BplusTree;

 public:
  /**
   * @brief Construct a new Node object
   *
   */
  Node();

  /**
   * @brief Construct a new Node object
   *
   * @param page_id
   */
  explicit Node(int64_t page_id);

 protected:
  Property<int64_t> page_id{-1};
  Property<int64_t> count{0};
  Property<int64_t> right{0};
  ArrayProperty<T, ORDER + 1> data{};
  ArrayProperty<int64_t, ORDER + 2> children{0};

  /**
   * @brief
   *
   * @param pos
   * @param value
   */
  void insert_in_node(int64_t pos, const T& value);

  /**
   * @brief
   *
   * @param pos
   */
  void delete_in_node(int64_t pos);

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  bool is_overflow() const;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  bool is_underflow() const;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  bool is_leaf() const;
};

/**
 * @brief B+ 树的迭代器.
 *
 * @tparam T
 * @tparam ORDER
 */
template <class T, int16_t ORDER>
class Iterator {
  using node = Node<T, ORDER>;
  template <class X, int16_t Y>
  friend class BplusTree;

 public:
  /**
   * @brief Construct a new Iterator object
   *
   */
  Iterator();

  /**
   * @brief Construct a new Iterator object
   *
   * @param that
   */
  Iterator(const Iterator& that);

  /**
   * @brief Construct a new Iterator object
   *
   * @param pager
   */
  explicit Iterator(std::shared_ptr<Pager> pager);

  auto operator->() -> const T*;
  auto operator*() -> T;
  auto operator++() -> Iterator&;
  auto operator++(int) -> Iterator;
  auto operator=(const Iterator& that) -> Iterator&;
  bool operator!=(const Iterator& that) const;

 protected:
  Property<int64_t> index{0};
  Property<std::shared_ptr<node>> current_pos;

 private:
  std::shared_ptr<Pager> pager;
};

template <class T, int16_t ORDER = 3>
class BplusTree {
  using node = Node<T, ORDER>;
  using iterator = Iterator<T, ORDER>;
  using nodeptr = std::shared_ptr<node>;

  /**
   * @brief B+ 树子结点的状态.
   *
   */
  enum class State {
    BT_OVERFLOW,   // 元素数目过多
    BT_UNDERFLOW,  // 元素数目过少
    OK,            // OK
  };
  enum class InNode {
    LEFT,
    RIGHT,
  };
  struct Header {
    int64_t root_id = 1;
    int64_t count = 0;
  };

 public:
  /**
   * @brief B+ 树的构造函数.
   *
   * @param pager
   */
  explicit BplusTree(std::shared_ptr<Pager> pager);

  /**
   * @brief begin()
   *
   * @return 返回 B+ 树的 begin() 迭代器.
   */
  auto begin() -> iterator;

  /**
   * @brief 在 B+ 树中查找一个值.
   *
   * @param value 欲查找的值.
   * @return 返回的迭代器指向欲查找的值. 如未查找到, 返回 end().
   */
  auto find(const T& value) -> iterator;

  /**
   * @brief 在 B+ 树中查找一个偏序大于等于 value 的值.
   *
   * @param value 欲查找的值.
   * @return 返回指向偏序大于等于 value 的第一个值的迭代器.
   */
  auto find_geq(const T& value) -> iterator;

  /**
   * @brief end()
   *
   * @return B+ 树的 end() 迭代器.
   */
  auto end() -> iterator;

  /**
   * @brief 在 B+ 树中插入一个值.
   *
   * @param value 待插入的值.
   */
  void insert(const T& value);

  /**
   * @brief 打印 B+ 树. (应该只用于测试.)
   *
   */
  void print();

 private:
  int16_t print_count = 1;
  std::shared_ptr<Pager> pager;
  std::shared_ptr<Header> header = std::make_shared<Header>();

  /**
   * @brief 把 B+ 树的一个结点写进 record.
   *
   * @param id 在 record 中的编号.
   * @param n_ptr 指向结点的指针.、‘
   *
   */
  void write_node(int64_t id, nodeptr n_ptr);

  /**
   * @brief
   *
   * @param id
   * @return nodeptr
   */
  auto read_node(int64_t id) -> nodeptr;

  /**
   * @brief
   *
   * @return nodeptr
   */
  auto new_node() -> nodeptr;

  /**
   * @brief
   *
   * @param value
   * @param root
   * @return iterator
   */
  auto find_helper(const T& value, nodeptr root) -> iterator;

  /**
   * @brief
   *
   * @param ptr
   */
  void print_helper(nodeptr ptr);

  /**
   * @brief
   *
   * @param n
   * @param value
   * @return State
   */
  auto insert_helper(nodeptr n, const T& value) -> State;

  /**
   * @brief
   *
   * @param parent
   * @param child
   * @param p
   * @param iter
   */
  void reset_children(nodeptr parent, nodeptr child, InNode p, int64_t* iter);

  /**
   * @brief
   *
   * @param n1
   * @param n2
   * @param n3
   * @param pos
   */
  void write_these_nodes(nodeptr n1, nodeptr n2, nodeptr n3, int64_t pos);
};

#pragma region  // # Pager Implementation

Pager::Pager(std::string file_name, bool create)
    : std::fstream(file_name.data(),
                   std::ios::in | std::ios::out | std::ios::binary) {
  if (!create && !is_open()) {
    throw ndb::database_not_exist(file_name);
  }
  if (!create && fail()) {
    throw ndb::database_opening_error(file_name);
  }
  empty = false;
  if (create) {
    empty = true;
    open(file_name.data(),
         std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
  }
}

Pager::~Pager() { close(); }

template <class Register>
auto Pager::get_id(Register* reg) -> int64_t {
  seekg(0, std::ios::end);
  auto id = tellg() / sizeof(Register);
  return id;
}

template <class Register>
void Pager::save(const int64_t& n, Register* reg) {
  clear();
  seekp(n * sizeof(Register), std::ios::beg);
  write(reinterpret_cast<char*>(reg), sizeof(*reg));
}

template <class Register>
bool Pager::recover(const int64_t& n, Register* reg) {
  clear();
  seekg(n * sizeof(Register), std::ios::beg);
  read(reinterpret_cast<char*>(reg), sizeof(*reg));
  return gcount() > 0;
}

template <class Register>
void Pager::erase(const int64_t& n) {
  clear();
  char mark = 'X';
  seekg(n * sizeof(Register), std::ios::beg);
  write(&mark, 1);
}

#pragma endregion

#pragma region  // # Node Implementation

template <class T, int16_t ORDER>
Node<T, ORDER>::Node() {
  this->page_id = 0;
  this->count = 0;
  this->right = 0;
}

template <class T, int16_t ORDER>
Node<T, ORDER>::Node(int64_t page_id) {
  this->page_id = page_id;
  this->count = 0;
  this->right = 0;
}

template <class T, int16_t ORDER>
void Node<T, ORDER>::insert_in_node(int64_t pos, const T& value) {
  auto j = count();
  while (j > pos) {
    data[j] = data[j - 1];
    children[j + 1] = children[j];
    j--;
  }
  data[j] = value;
  children[j + 1] = children[j];
  count = count() + 1;
}

template <class T, int16_t ORDER>
void Node<T, ORDER>::delete_in_node(int64_t pos) {
  for (auto i = pos; i < count(); i++) {
    data[i] = data[i + 1];
    children[i + 1] = children[i + 2];
  }
  count--;
}

template <class T, int16_t ORDER>
bool Node<T, ORDER>::is_overflow() const {
  return count() > ORDER;
}

template <class T, int16_t ORDER>
bool Node<T, ORDER>::is_underflow() const {
  return count() < floor(ORDER / 2.0);
}

template <class T, int16_t ORDER>
bool Node<T, ORDER>::is_leaf() const {
  return children()[0] == 0;
}

#pragma endregion

#pragma region  // # Iterator Implementation

template <class T, int16_t ORDER>
Iterator<T, ORDER>::Iterator() {}

template <class T, int16_t ORDER>
Iterator<T, ORDER>::Iterator(const Iterator& that) {
  this->current_pos = that.current_pos();
  this->index = that.index();
  this->pager = that.pager;
}

template <class T, int16_t ORDER>
Iterator<T, ORDER>::Iterator(std::shared_ptr<Pager> pager) : pager{pager} {}

template <class T, int16_t ORDER>
auto Iterator<T, ORDER>::operator->() -> const T* {
  return &(current_pos.itself().get()->data()[index()]);
}

template <class T, int16_t ORDER>
auto Iterator<T, ORDER>::operator*() -> T {
  return current_pos.itself().get()->data()[index()];
}

template <class T, int16_t ORDER>
auto Iterator<T, ORDER>::operator++() -> Iterator& {
  if (index() < current_pos()->count() - 1) {
    index = index() + 1;
  } else {
    index = 0;
    auto that = std::make_shared<node>(-1);
    if (current_pos()->right() == 0) {
      current_pos = that;
    } else {
      this->pager->recover(current_pos()->right(), current_pos.itself().get());
    }
  }
  return *this;
}

template <class T, int16_t ORDER>
auto Iterator<T, ORDER>::operator++(int) -> Iterator {
  auto temp = *this;
  ++*this;
  return temp;
}

template <class T, int16_t ORDER>
auto Iterator<T, ORDER>::operator=(const Iterator& that) -> Iterator& {
  this->current_pos = that.current_pos();
  this->index = that.index();
  this->pager = that.pager;
  return *this;
}

template <class T, int16_t ORDER>
bool Iterator<T, ORDER>::operator!=(const Iterator& that) const {
  if (this->current_pos()->page_id() == that.current_pos()->page_id()) {
    auto this_n = this->current_pos();
    auto that_n = that.current_pos();
    return !(this_n->data()[index()] == that_n->data()[that.index()]);
  }
  return true;
}

#pragma endregion

#pragma region  // # BplusTree Implementation

template <class T, int16_t ORDER>
BplusTree<T, ORDER>::BplusTree(std::shared_ptr<Pager> pager) {
  this->pager = pager;
  if (pager->empty()) {
    node root(header->root_id);
    pager->save(root.page_id(), &root);
    header->count++;
    pager->save(0, header.get());
  } else {
    pager->recover(0, header.get());
  }
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::begin() -> iterator {
  auto root = read_node(header->root_id);
  while (!root->is_leaf()) {
    auto id = root->children()[0];
    root = read_node(id);
  }
  iterator it(pager);
  it.current_pos = root;
  return it;
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::find(const T& value) -> iterator {
  auto root = read_node(header->root_id);
  auto it = find_helper(value, root);
  return *it == value ? it : end();
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::find_geq(const T& value) -> iterator {
  auto root = read_node(header->root_id);
  auto it = find_helper(value, root);
  return it;
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::end() -> iterator {
  auto end = std::make_shared<node>(-1);
  iterator it(pager);
  it.current_pos = end;
  return it;
}

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::insert(const T& value) {
  auto root = read_node(header->root_id);
  auto state = insert_helper(root, value);
  if (state == State::BT_OVERFLOW) {
    auto overflow = read_node(header->root_id);
    auto left_child = new_node();
    auto right_child = new_node();
    int64_t iter = 0;
    reset_children(overflow, left_child, InNode::LEFT, &iter);
    overflow->data.set(0, overflow->data()[iter]);
    left_child->right = right_child->page_id();
    if (!overflow->is_leaf()) {
      iter++;
    }
    reset_children(overflow, right_child, InNode::RIGHT, &iter);
    overflow->count = 1;
    write_these_nodes(overflow, left_child, right_child, 0);
  }
}

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::print() {
  print_count = 1;
  auto root = read_node(header->root_id);
  if (print_count > 64) {
    return;
  }
  print_helper(root);
}

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::write_node(int64_t id, nodeptr n_ptr) {
  pager->save(id, n_ptr.get());
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::read_node(int64_t id) -> nodeptr {
  auto ret = std::make_shared<node>(-1);
  pager->recover(id, ret.get());
  return ret;
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::new_node() -> nodeptr {
  header->count++;
  auto ret = std::make_shared<node>(header->count);
  pager->save(0, header.get());
  return ret;
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::find_helper(const T& value, nodeptr root)
    -> iterator {
  auto pos = 0;
  if (!root->is_leaf()) {
    while (pos < root->count() && root->data()[pos] <= value) {
      pos++;
    }
    auto child = read_node(root->children()[pos]);
    return find_helper(value, child);
  } else {
    while (pos < root->count() && root->data()[pos] < value) {
      pos++;
    }
    iterator it(pager);
    it.current_pos = root;
    it.index = pos;
    if (pos == root->count()) {
      it++;
    }
    return it;
  }
};

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::print_helper(nodeptr ptr) {
  auto i = 0;
  for (i = 0; i < ptr->count(); i++) {
    if (ptr->children()[i]) {
      auto child = read_node(ptr->children[i]);
      print_helper(child);
    }
    if (ptr->is_leaf()) {
      if (print_count <= 64) {
        auto num = fmt::format("[{}] ", print_count);
        fmt::print(fg(fmt::terminal_color::bright_blue), "{:>5}", num);
        fmt::print("{}\n", ptr->data()[i].key);
      } else if (print_count == 65) {
        fmt::print("...\n");
        fmt::print("There is more than 64 records, ");
        fmt::print("please use `find` command.\n");
      } else {
        return;
      }
      print_count++;
    }
  }
  if (ptr->children()[i]) {
    auto child = read_node(ptr->children()[i]);
    print_helper(child);
  }
}

template <class T, int16_t ORDER>
auto BplusTree<T, ORDER>::insert_helper(nodeptr n, const T& value) -> State {
  auto pos = 0;
  while (pos < n->count() && n->data()[pos] < value) {
    pos++;
  }
  if (n->children()[pos] != 0) {
    auto id = n->children()[pos];
    auto child = read_node(id);
    auto state = insert_helper(child, value);
    if (state == State::BT_OVERFLOW) {
      auto overflow = read_node(n->children()[pos]);
      auto left_child = overflow;
      left_child->count = 0;
      auto right_child = new_node();
      int64_t iter = 0;
      reset_children(overflow, left_child, InNode::LEFT, &iter);
      n->insert_in_node(pos, overflow->data()[iter]);
      if (!overflow->is_leaf()) {
        iter++;
      } else {
        right_child->right = left_child->right();
        left_child->right = right_child->page_id();
        n->children.set(pos + 1, right_child->page_id());
      }
      reset_children(overflow, right_child, InNode::RIGHT, &iter);
      write_these_nodes(n, left_child, right_child, pos);
    }
  } else {
    n->insert_in_node(pos, value);
    write_node(n->page_id(), n);
  }
  return n->is_overflow() ? State::BT_OVERFLOW : State::OK;
}

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::reset_children(nodeptr parent, nodeptr child,
                                         InNode p, int64_t* iter) {
  auto i = 0;
  auto flag = p == InNode::LEFT;
  for (i = 0; *iter < ceil(flag ? ORDER / 2.0 : ORDER + 1); i++) {
    child->children.set(i, parent->children()[*iter]);
    child->data.set(i, parent->data()[*iter]);
    child->count = child->count() + 1;
    (*iter)++;
  }
  child->children.set(i, parent->children()[*iter]);
}

template <class T, int16_t ORDER>
void BplusTree<T, ORDER>::write_these_nodes(nodeptr n1, nodeptr n2, nodeptr n3,
                                            int64_t pos) {
  n1->children.set(pos, n2->page_id());
  n1->children.set(pos + 1, n3->page_id());
  write_node(n1->page_id(), n1);
  write_node(n2->page_id(), n2);
  write_node(n3->page_id(), n3);
}

#pragma endregion

};  // namespace ndb

#endif  // INC_BPTREE_HH_
