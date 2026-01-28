// Copyright 2024 coScene
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef REGEX_UTILS_HPP_
#define REGEX_UTILS_HPP_

#include <algorithm>
#include <regex>
#include <string>
#include <vector>

namespace cobridge_base
{

inline bool is_whitelisted(const std::string & name, const std::vector<std::regex> & regex_patterns)
{
  return std::find_if(
    regex_patterns.begin(), regex_patterns.end(), [name](const std::regex & regex) {
      return std::regex_match(name, regex);
    }) != regex_patterns.end();
}
}  // namespace cobridge_base

#endif  // REGEX_UTILS_HPP_
