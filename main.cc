/**
 * @file main.cc
 * @author Selene
 * @brief 主文件.
 * @version 0.2
 * @date 2021-02-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <vector>

#include "inc/cmd.hh"

int main() {
  ndb::print_msg();
  while (true) {
    ndb::print_prompt();
    std::string str, command, args_str;
    getline(std::cin, str);
    std::stringstream in(str);
    ndb::CommandLine cmdln(str);
    cmdln.execute();
  }
}
