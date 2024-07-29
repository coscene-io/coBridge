//////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cobridge {

    inline std::pair<std::string, std::string> get_node_and_node_namespace(const std::string &fqn_node_name) {
        const std::size_t found = fqn_node_name.find_last_of('/');
        if (found == std::string::npos) {
            throw std::runtime_error("Invalid fully qualified node name: " + fqn_node_name);
        }
        return std::make_pair(fqn_node_name.substr(0, found), fqn_node_name.substr(found + 1));
    }

}
