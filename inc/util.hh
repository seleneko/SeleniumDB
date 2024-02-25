/**
 * @file util.hh
 * @author Selene
 * @brief 各种有用的杂项, 有些其实可以用现成的库来代替.
 * @version 0.2
 * @date 2021-02-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_UTIL_HH_
#define INC_UTIL_HH_

#include <fmt/core.h>

#include <array>
#include <ctime>
#include <exception>
#include <iostream>
#include <string>

namespace ndb {

struct Record {
  Record() {}
  Record(uint32_t pos, uint32_t len) : pos(pos), len(len) {}

  uint32_t pos = 0;
  uint32_t len = 0;
};

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
constexpr char MKDIR[] = "md";
constexpr char CLEAR[] = "cls";
#else
constexpr char MKDIR[] = "mkdir";
constexpr char CLEAR[] = "clear";
#endif

/**
 * 把类的成员变量封装了一层, 模拟了其他语言常用的 property.
 * 用 prop() 来充当 get 方法, 用 prop = p 来充当 set 方法.
 * ? 具体实现上有待改进.
 */
template <class T>
class Property {
 public:
  Property() {}
  explicit Property(const T &f) { val = f; }
  virtual ~Property() {}

  virtual auto operator=(const T &f) -> T & { return val = f; }
  virtual auto operator()() const -> const T & { return val; }
  virtual auto itself() -> T & { return val; }

 protected:
  T val;
};

/**
 * 把类的数组成员变量封装了一层, 模拟了其他语言常用的 property.
 * 用 prop() 来充当 get 方法, 用 prop = p 来充当 set 方法.
 * ? 具体实现上有待改进.
 */
template <class T, int S>
class ArrayProperty {
 public:
  ArrayProperty() {}
  explicit ArrayProperty(const T &f) { val.fill(f); }
  explicit ArrayProperty(const std::array<T, S> &f) { val = f; }
  virtual ~ArrayProperty() {}

  virtual auto operator[](int i) -> T & { return val[i]; }
  virtual auto operator()() const -> const std::array<T, S> & { return val; }
  virtual auto itself() -> std::array<T, S> & { return val; }
  virtual void set(int i, const T &f) { val[i] = f; }

 protected:
  std::array<T, S> val;
};

void print_msg() {
  fmt::print("tssndb version 1.5.0\n");
  fmt::print("i.e. too simple sometimes naive database\n");
}
void print_prompt() { fmt::print("MDB >>> "); }

/**
 * @brief 参数数目有误.
 *
 */
struct invalid_arguments_num : public std::exception {
  invalid_arguments_num(int ex, int got, std::string format)
      : ex(ex), got(got), format(format) {}
  std::string msg() const throw() {
    auto str = fmt::format("Expected {} argument(s), but got {}.", ex, got);
    return str;
  }
  std::string how() const throw() {
    auto str = fmt::format("Format: {}.", format);
    return str;
  }
  int ex;
  int got;
  std::string format;
};

/**
 * @brief 数据库不存在.
 *
 */
struct database_not_exist : public std::exception {
  explicit database_not_exist(std::string file_name) : file_name(file_name) {}
  const char *what() const throw() { return "Database does not exist."; }
  std::string file_name;
};

/**
 * @brief 数据库二进制文件打开有错误.
 *
 */
struct database_opening_error : public std::exception {
  explicit database_opening_error(std::string fn) : file_name(fn) {}
  const char *what() const throw() { return "File opening error: "; }
  std::string file_name;
};

/**
 * @brief 没有打开的数据库.
 *
 */
struct database_not_open : public std::exception {
  std::string msg() const throw() {
    auto str = fmt::format("No opening database.");
    return str;
  }
  std::string how() const throw() {
    auto str = fmt::format("Please open a database first.");
    return str;
  }
};

/**
 * @brief 已有名为 [] 的数据库.
 *
 */
struct database_exists : public std::exception {
  explicit database_exists(std::string db) : db_name(db) {}
  std::string msg() const throw() {
    auto str = fmt::format("Database {} already exists.", db_name);
    return str;
  }
  std::string how() const throw() {
    auto str = fmt::format("Please just open it.");
    return str;
  }
  std::string db_name;
};

/**
 * @brief 已打开一个数据库, 必须先关闭它才能打开新数据库.
 *
 */
struct another_database_opening : public std::exception {
  explicit another_database_opening(std::string db) : db_name(db) {}
  std::string msg() const throw() {
    auto str = fmt::format("Database {} is open.", db_name);
    return str;
  }
  std::string how() const throw() {
    auto str = fmt::format("Please close it first.");
    return str;
  }
  std::string db_name;
};

/**
 * @brief 试图查询一个空串.
 *
 */
struct empty_inquiry : public std::exception {
  empty_inquiry() {}
  std::string msg() const throw() {
    auto str = fmt::format("Input should not be empty.");
    return str;
  }
  std::string how() const throw() {
    auto str = fmt::format("todo:");
    return str;
  }
};

/**
 * @brief 用于测试时计时的类.
 * ? 好像 C++20 有自带的.
 *
 */
class Clock {
 public:
  /**
   * @brief Fiat iustitia, et pereat mundus. (Ferdinand I.)
   *
   */
  void verify() {
    state = State::TOCKED;
    start = clock();
    end = clock();
  }

  /**
   * @brief 开始计时.
   *
   */
  void tick() {
    assert(state == State::TOCKED);
    state = State::TICKED;
    start = clock();
  }

  /**
   * @brief 结束计时.
   *
   */
  void tock() {
    assert(state == State::TICKED);
    state = State::TOCKED;
    end = clock();
  }

  /**
   * @brief 返回计时结果.
   *
   * @return 耗时 (毫秒).
   */
  auto time_cost() -> double_t {
    assert(state == State::TOCKED);
    return double_t((end - start) * 1000 / CLOCKS_PER_SEC);
  }

  /**
   * @brief 打印计时结果.
   *
   */
  void print_time_cost() { fmt::print("({} ms)", time_cost()); }

 private:
  enum class State {
    INIT,
    TICKED,
    TOCKED,
  } state = State::TOCKED;
  clock_t start = clock();
  clock_t end = clock();
} clk;

};  // namespace ndb

#endif  // INC_UTIL_HH_
