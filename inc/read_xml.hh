/**
 * @file read_xml.hh
 * @author Selene
 * @brief 直接读取 XML 文件并直接将其内容保存到 B+ 树.
 * @version 0.2
 * @date 2021-02-02
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef INC_READ_XML_HH_
#define INC_READ_XML_HH_

#include <libxml/SAX2.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "database.hh"
#include "util.hh"

namespace ndb {

using xstr = const xmlChar *;
using poslen = std::pair<uint32_t, uint32_t>;

// 之所以初始值是 6 是因为 <dblp> 无需考虑.
poslen pos = {6, 0};

std::vector<char> partial_key;

enum class ParserState {
  AUTHOR,
  TITLE,
  OTHER,
};

// 一个状态机, 用于表明当前需要插入数据的归属 (是属于 author 或是属于
// title).
ParserState state = ParserState::OTHER;

// 这个是根据状态机的状态来决定选用哪个 vector.
// 例如, 想要添加日期就在 ParserState 里加 DATE 状态, 再在下面加一行:
// vector<string> &time_key_list = key_list[ParserState::TIME];

std::map<ParserState, std::vector<std::string>> key_list;
std::vector<std::string> &title_key_list = key_list[ParserState::TITLE];
std::vector<std::string> &author_key_list = key_list[ParserState::AUTHOR];

int layer_count;
int t_cnt = 0;
/**
 * @brief SAX 分析起始时调用.
 * @param ctx XML 正文.
 * @param name 元素名称.
 * @param attrs 元素参数对.
 */
static void on_start_element(void *ctx, xstr name, xstr *attrs) {
  // 这里是读取<name>...</name> 然后看 name 是作者还是标题.
  // 如果要存其他的 (比如日期) 就在 ParserState 里面添加状态,
  // 然后添加一个 std::vector<std::string>, 再在这里修改代码.
  // ? 似乎可以改成用 switch 的.
  partial_key.clear();
  layer_count++;
  state = strcmp((const char *)name, "author") == 0  ? ParserState::AUTHOR
          : strcmp((const char *)name, "title") == 0 ? ParserState::TITLE
                                                     : ParserState::OTHER;
}

/**
 * @brief SAX 分析结束时调用.
 * @param ctx XML 正文.
 * @param name 元素名称.
 */
static void on_end_element(void *ctx, xstr name) {
  // 这里是读到最后, 把数据插入到数据库的表中.
  // 如果想插入其他数据, 直接添加代码就可以.
  state = strcmp((const char *)name, "author") == 0  ? ParserState::AUTHOR
          : strcmp((const char *)name, "title") == 0 ? ParserState::TITLE
                                                     : ParserState::OTHER;
  layer_count--;
  t_cnt++;
  if (t_cnt % 100000 == 0) {
    fmt::print("{}", t_cnt / 100000);
  }
  if (state != ParserState::OTHER) {
    auto push_back_helper = [](std::string k) {
      // 这里保证插入的 key 最大长度为 KEY_LENGTH, 现在设置为 64.
      // ? 是否存在更好的方法?
      if (k.size() > 64) {
        k = k.substr(0, 64 - 3) + "...";
      }
      k.resize(64, ' ');
      assert(k.size() == 64);
      key_list[state].push_back(k);
    };
    std::string key;
    key.insert(key.begin(), partial_key.begin(), partial_key.end());
    while (key.find(" - ") != key.npos || key.find("; ") != key.npos) {
      bool flag = key.find(" - ") < key.find("; ");
      auto p = key.find(flag ? " - " : "; ");
      auto temp = key.substr(0, p);
      push_back_helper(temp);
      key.erase(0, p + (flag ? 3 : 2));
    }
    push_back_helper(key);
  }
  if (layer_count == 1) {
    pos.second = xmlSAX2GetColumnNumber(ctx) - 1;
    for (auto it : author_key_list) {
      // author_map.insert({it, pos});
      assert(db.is_open());
      Record k(pos.first, pos.second - pos.first);
      it = it.substr(0, it.find("  "));
      db.insert(k, it.c_str(), DatabaseState::AUTHOR);

      std::stringstream in(it);
      std::string word;
      std::vector<std::string> word_list;
      while (in >> word) {
        word_list.push_back(word);
      }
      invidx_manager.build(word_list, pos.first, pos.second - pos.first);
      topk_manager.insert(it);
    }
    for (auto it : title_key_list) {
      assert(db.is_open());
      Record k(pos.first, pos.second - pos.first);
      it = it.substr(0, it.find("  "));
      db.insert(k, it.c_str(), DatabaseState::TITLE);

      std::stringstream in(it);
      std::string word;
      std::vector<std::string> word_list;
      while (in >> word) {
        word_list.push_back(word);
      }
      invidx_manager.build(word_list, pos.first, pos.second - pos.first);
    }
    pos.first = pos.second;
    author_key_list.clear();
    title_key_list.clear();
  }
}

/**
 * @brief 读取正文, 根据 ParserState 的状态来决定往哪个 vector 里插入数据.
 * @param ctx XML 正文.
 * @param ch XML 字符串.
 * @param len XML 字符数.
 */
static void on_characters(void *ctx, xstr ch, int len) {
  if (state != ParserState::OTHER) {
    std::string key((const char *)ch, len);
    std::copy(key.begin(), key.end(), std::back_inserter(partial_key));
  }
}

/**
 * @brief 利用 LibXml 读取 XML 文件.
 * @param file_name 文件名.
 *
 */

void read_xmlfile(const char *file_name) {
  FILE *file = fopen(file_name, "r");
  layer_count = 0;
  char chars[1024];
  try {
    if (file == nullptr) {
      // todo:
      // throw file_opening_error();
    }
    auto res = fread(chars, 1, 4, file);
    if (res <= 0) {
      // todo:
      // throw file_reading_error();
    }
    auto sax_hander = [&] {
      xmlSAXHandler sax_hander;
      memset(&sax_hander, 0, sizeof(xmlSAXHandler));
      sax_hander.initialized = XML_SAX2_MAGIC;  // 听说 XML_SAX2_MAGIC 比 0 好.
      sax_hander.startElement = on_start_element;
      sax_hander.endElement = on_end_element;
      sax_hander.characters = on_characters;
      return sax_hander;
    }();
    auto ctxt =
        xmlCreatePushParserCtxt(&sax_hander, nullptr, chars, res, nullptr);
    while ((res = fread(chars, 1, sizeof(chars), file)) > 0) {
      if (xmlParseChunk(ctxt, chars, res, 0)) {
        xmlParserError(ctxt, "xmlParseChunk");
        // todo:
        // throw xml_parsing_error();
      }
    }
    xmlParseChunk(ctxt, chars, 0, 1);
    xmlFreeParserCtxt(ctxt);
    xmlCleanupParser();
  } catch (const std::exception &e) {
    fmt::print("{}\n", e.what());
  }
  fclose(file);
}

};  // namespace ndb

#endif  // INC_READ_XML_HH_
