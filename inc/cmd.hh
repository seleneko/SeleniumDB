/**
 * @file cmd.hh
 * @author Selene
 * @brief 简单命令行的实现.
 * @version 0.2
 * @date 2021-02-15
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_CMD_HH_
#define INC_CMD_HH_

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "bptree.hh"
#include "database.hh"
#include "read_xml.hh"
#include "util.hh"

namespace ndb {

class CommandLine {
  enum class Statement {
    INSERT,
    SELECT,
    OPEN,
    EXIT,
    FIND,
    READ,
    CLOSE,
    CREATE,
    WHOAMI,
    UNKNOWN,
    SEARCH,
    TOPK,
    HELP,
  };
  enum class ExecuteState {
    MAIN,
    FINDING,
  };
  std::map<std::string, Statement> statement_map{
      {"insert", Statement::INSERT}, {"select", Statement::SELECT},
      {"open", Statement::OPEN},     {"exit", Statement::EXIT},
      {"read", Statement::READ},     {"find", Statement::FIND},
      {"whoami", Statement::WHOAMI}, {"close", Statement::CLOSE},
      {"create", Statement::CREATE}, {"search", Statement::SEARCH},
      {"top", Statement::TOPK},      {"help", Statement::HELP},
  };
  std::map<Statement, std::function<void(void)>> execute_map = {
      {Statement::CREATE, [&]() { execute_create(); }},
      {Statement::OPEN, [&]() { execute_open(); }},
      {Statement::READ, [&]() { execute_read(); }},
      {Statement::INSERT, [&]() { execute_insert(); }},
      {Statement::SELECT, [&]() { execute_select(); }},
      {Statement::FIND, [&]() { execute_find(); }},
      {Statement::WHOAMI, [&]() { execute_whoami(); }},
      {Statement::EXIT, [&]() { execute_exit(); }},
      {Statement::CLOSE, [&]() { execute_close(); }},
      {Statement::SEARCH, [&]() { execute_search(); }},
      {Statement::HELP, [&]() { execute_help(); }},
      {Statement::TOPK, [&]() { execute_topk(); }},
      {Statement::UNKNOWN, [&]() { execute_unknown(); }},
  };

 public:
  explicit CommandLine(std::string str);

  void execute();

 private:
  void execute_create();

  void execute_read();

  void execute_open();

  void execute_insert();

  void execute_insert_test();

  void execute_select();

  void execute_find();

  void execute_search();

  void execute_whoami();

  void execute_topk();

  void execute_close();

  void execute_exit();

  void execute_unknown();

  void execute_help();

  auto tokenizer(std::string input) -> std::vector<std::string>;

  ExecuteState now;
  std::string command;
  std::vector<std::string> args;
};

CommandLine::CommandLine(std::string str) {
  // ? 这样分感觉有点太武断了.
  auto vec = tokenizer(str);
  command = vec[0];
  vec.erase(vec.begin());
  args = vec;
}

void CommandLine::execute() {
  // 这里用包装了一层 statement 是模仿 @cstack 的数据库实现,
  // 但我也不知道这样做有什么好处.
  execute_map[statement_map.find(command) != statement_map.end()
                  ? statement_map[command]
                  : Statement::UNKNOWN]();
}

void CommandLine::execute_create() {
  try {
    if (ndb::db.is_open()) {
      throw ndb::another_database_opening(ndb::db.name());
    }
    if (args.size() != 1) {
      throw ndb::invalid_arguments_num(1, args.size(), "create [name]");
    }
    auto name = args[0];
    if (access(fmt::format("database/{}", name).c_str(), 0) == 0) {
      throw ndb::database_exists(name);
    }
    ndb::db.db_open(name, true);
    fmt::print(fg(fmt::terminal_color::bright_green), "Database {} is open.\n",
               name);
  } catch (ndb::another_database_opening &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print("{}\n", e.how());
    return;
  } catch (ndb::invalid_arguments_num &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print(fg(fmt::terminal_color::bright_cyan), "{}\n", e.how());
    return;
  } catch (ndb::database_exists &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print("{}\n", e.how());
    return;
  }
}

void CommandLine::execute_read() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    ndb::read_xmlfile("xml/small.xml");
    topk_manager.make_topk(1024);  // todo:!!!
    fmt::print("READ OK");
    fmt::print("\n");
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.what());
    fmt::print("Please open a database first.\n");
  }
}

void CommandLine::execute_open() {
  try {
    if (ndb::db.is_open()) {
      throw ndb::another_database_opening(ndb::db.name());
    }
    if (args.size() != 1) {
      throw ndb::invalid_arguments_num(1, args.size(), "open [name]");
      return;
    }
    auto name = args[0];
    ndb::db.db_open(name, false);
    fmt::print(fg(fmt::terminal_color::bright_green), "Database {} is open.\n",
               name);
  } catch (ndb::invalid_arguments_num &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print(fg(fmt::terminal_color::bright_cyan), "{}\n", e.how());
    return;
  } catch (ndb::another_database_opening &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print("{}\n", e.how());
    return;
  } catch (ndb::database_not_exist &e) {  // FIXME:
    ndb::db.db_close();
    auto fn = e.file_name;
    fmt::print(fg(fmt::terminal_color::bright_cyan), "{}\n", e.what());
    fmt::print("Create it now? (y/n) ");
    std::string str;
    getline(std::cin, str);
    if (str == "y") {
      ndb::db.db_open(args[0], true);
      fmt::print(fg(fmt::terminal_color::bright_green),
                 "Database {} is open.\n", args[0]);
    }
    return;
  } catch (ndb::database_opening_error &e) {  // FIXME:
    ndb::db.db_close();
    fmt::print(fg(fmt::terminal_color::bright_red), "File corrupted.\n");
    fmt::print("Remove it now? (y/n) ");
    std::string str;
    getline(std::cin, str);
    if (str == "y") {
      auto fn = e.file_name;
      auto name = fn.substr(0, fn.find("."));
      auto res1 = remove(fmt::format("{}_rec.bin", name).c_str());
      auto res2 = remove(fmt::format("{}_idx.bin", name).c_str());
      (res1 == EOF || res2 == EOF)
          ? fmt::print("This message should not be shown...\n")
          : fmt::print(fg(fmt::terminal_color::bright_green),
                       "File removed.\n");
    }
    return;
  }
}

void CommandLine::execute_insert() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    fmt::print(fg(fmt::terminal_color::bright_magenta),
               "`insert` is only allowed when testing.\n");
    fmt::print("Use `read` instead.\n");
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_insert_test() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    ndb::db.insert({1, 0}, "key", DatabaseState::AUTHOR);
    fmt::print("INSERT OK");
    fmt::print("\n");
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_select() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    ndb::db.select(DatabaseState::AUTHOR);  // todo:!!!
    fmt::print("SELECT OK");
    fmt::print("\n");
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_find() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    if (args.size() > 2) {
      throw ndb::invalid_arguments_num(2, args.size(), "find [what] [name]");
    }
    if (args.size() < 2 || args[1] == "") {
      throw ndb::empty_inquiry();
    }
    DatabaseState s;
    if (args[0] == "title") {
      s = DatabaseState::TITLE;
    } else if (args[0] == "author") {
      s = DatabaseState::AUTHOR;
    } else {
      assert(0);
    }
    clk.tick();
    ndb::db.find(args[1], s);
    clk.tock();
    fmt::print("FIND OK");
    fmt::print(" ({}ms)\n", clk.time_cost());
    now = ExecuteState::MAIN;
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  } catch (ndb::invalid_arguments_num &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print("Do you mean ");
    auto what = args[0];
    args.erase(args.begin());
    fmt::print(fg(fmt::terminal_color::bright_cyan), "find {0} \"{1}\"", what,
               fmt::join(args, " "));
    fmt::print("?\n");
  } catch (ndb::empty_inquiry &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_search() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    clk.tick();
    ndb::db.search(args);
    clk.tock();
    fmt::print("SEARCH OK");
    fmt::print(" ({}ms)\n", clk.time_cost());
    now = ExecuteState::MAIN;
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  } catch (ndb::empty_inquiry &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_whoami() {
  try {
    if (!ndb::db.is_open()) {
      throw ndb::database_not_open();
    }
    fmt::print("Who am I? ");
    fmt::print(fg(fmt::terminal_color::bright_blue), "Database {}!\n",
               ndb::db.name());
  } catch (ndb::database_not_open &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    return;
  }
}

void CommandLine::execute_topk() {
  try {
    if (args.size() != 1) {
      throw ndb::invalid_arguments_num(1, args.size(), "top [number]");
      return;
    }
    auto k = stoi(args[0]);
    ndb::db.topk(k);
  } catch (ndb::invalid_arguments_num &e) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", e.msg());
    fmt::print(fg(fmt::terminal_color::bright_cyan), "{}\n", e.how());
    return;
  }
}

void CommandLine::execute_close() {
  ndb::db.db_close();
  fmt::print(fg(fmt::terminal_color::bright_magenta),
             "Database {} is closed.\n", db.name());
}

void CommandLine::execute_exit() {
  ndb::db.db_close();
  fmt::print("So long...\n");
  exit(EXIT_SUCCESS);
}

void CommandLine::execute_unknown() {
  fmt::print(fg(fmt::terminal_color::bright_red), "Command not found: ");
  fmt::print("{}\n", command);
}

void CommandLine::execute_help() {
  fmt::print("create a database: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "create [database_name]\n");
  fmt::print("open a database: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "open [database_name]\n");
  fmt::print("read from xml file: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "read\n");
  fmt::print("select from table: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "select [title|author]\n");
  fmt::print("find (prefix) in table: ");
  fmt::print(fg(fmt::terminal_color::bright_green),
             "find [title|author] [keyword]\n");
  fmt::print("search (fuzzy) in table: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "search [keyword]\n");
  fmt::print("get authors with top article counts: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "top [number]\n");
  fmt::print("get the name of current opening database: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "whoami\n");
  fmt::print("close a database: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "close\n");
  fmt::print("end the program: ");
  fmt::print(fg(fmt::terminal_color::bright_green), "exit\n");
}

auto CommandLine::tokenizer(std::string input) -> std::vector<std::string> {
  enum class State {
    STRING_ARG,
    ARG,
    EMPTY,
  };
  State state = State::EMPTY;
  std::vector<std::string> vec;
  int begin = 0;
  for (int i = 0; i < input.size(); i++) {
    if (input[i] == '"' && state == State::EMPTY) {
      state = State::STRING_ARG;
      begin = i++;
    }
    if (input[i] == '"' && state == State::STRING_ARG) {
      state = State::EMPTY;
      vec.push_back(input.substr(begin + 1, i - begin - 1));
    }
    if (input[i] != ' ' && input[i] != '"' && state == State::EMPTY) {
      state = State::ARG;
      begin = i;
    }
    if (input[i] == ' ' && state == State::ARG) {
      state = State::EMPTY;
      vec.push_back(input.substr(begin, i - begin));
    }
    if (i + 1 == input.size() && state == State::ARG) {
      vec.push_back(input.substr(begin, i - begin + 1));
    }
  }
  return vec;
}

};  // namespace ndb

#endif  // INC_CMD_HH_
