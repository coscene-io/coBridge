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

#ifndef COBRIDGE_BASE64_HPP
#define COBRIDGE_BASE64_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cobridge {
    std::string base64_encode(const std::string_view &input);

    std::vector<unsigned char> base64_decode(const std::string &input);
}

#endif //COBRIDGE_BASE64_HPP
