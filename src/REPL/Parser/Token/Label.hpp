//
// Copyright (C) 2018-2019 NCC Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef XENDBG_TOKEN_LABEL_HPP
#define XENDBG_TOKEN_LABEL_HPP

#include <optional>
#include <regex>
#include <string>
#include <utility>

#include "TokenMatchResult.hpp"

namespace xd::parser::token {

  class Label {
  public:
    explicit Label(std::string name)
        : _name(std::move(name)) {}

    std::string name() const { return _name; };

  private:
    std::string _name;

  public:
    static TokenMatchResult<Label> match(std::string::const_iterator begin, std::string::const_iterator end) {
      std::regex r("^\\&[A-Za-z_][A-Za-z0-9_]*");
      std::smatch m;

      if (!std::regex_search(begin, end, m, r))
        return std::make_pair(std::nullopt, begin);

      const auto label_end = begin + m.position() + m.length();
      const auto label_name = std::string(begin+1, label_end);

      return std::make_pair(Label(label_name), label_end);
    }
  };
}

#endif //XENDBG_TOKEN_LABEL_HPP
