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

#include <base64.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace cobridge_base
{

std::string base64_encode(const std::string_view & input)
{
  constexpr const char ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  // Every 3 bytes of data yields 4 bytes of output
  result.reserve((input.size() + (3 - 1 /* round up */)) / 3 * 4);

  // Unsigned values are required for bit-shifts below to work properly
  const unsigned char * data = reinterpret_cast<const unsigned char *>(input.data());

  size_t i = 0;
  for (; i + 2 < input.size(); i += 3) {
    result.push_back(ALPHABET[data[i] >> 2]);
    result.push_back(ALPHABET[((data[i] & 0b11) << 4) | (data[i + 1] >> 4)]);
    result.push_back(ALPHABET[((data[i + 1] & 0b1111) << 2) | (data[i + 2] >> 6)]);
    result.push_back(ALPHABET[data[i + 2] & 0b111111]);
  }
  switch (input.size() - i) {
    case 2:
      result.push_back(ALPHABET[data[i] >> 2]);
      result.push_back(ALPHABET[((data[i] & 0b11) << 4) | (data[i + 1] >> 4)]);
      result.push_back(ALPHABET[(data[i + 1] & 0b1111) << 2]);
      result.push_back('=');
      break;
    case 1:
      result.push_back(ALPHABET[data[i] >> 2]);
      result.push_back(ALPHABET[(data[i] & 0b11) << 4]);
      result.push_back('=');
      result.push_back('=');
      break;
  }

  return result;
}

std::vector<unsigned char> base64_decode(const std::string & input)
{
  if (input.length() % 4) {
    throw std::runtime_error("Invalid base64 length!");
  }

  constexpr char kPadCharacter = '=';

  std::size_t padding{};

  if (input.length()) {
    if (input[input.length() - 1] == kPadCharacter) {padding++;}
    if (input[input.length() - 2] == kPadCharacter) {padding++;}
  }

  std::vector<unsigned char> decoded;
  decoded.reserve(((input.length() / 4) * 3) - padding);

  std::uint32_t temp{};
  auto it = input.begin();

  while (it < input.end()) {
    for (std::size_t i = 0; i < 4; ++i) {
      temp <<= 6;
      if (*it >= 0x41 && *it <= 0x5A) {
        temp |= *it - 0x41;
      } else if (*it >= 0x61 && *it <= 0x7A) {
        temp |= *it - 0x47;
      } else if (*it >= 0x30 && *it <= 0x39) {
        temp |= *it + 0x04;
      } else if (*it == 0x2B) {
        temp |= 0x3E;
      } else if (*it == 0x2F) {
        temp |= 0x3F;
      } else if (*it == kPadCharacter) {
        switch (input.end() - it) {
          case 1:
            decoded.push_back((temp >> 16) & 0x000000FF);
            decoded.push_back((temp >> 8) & 0x000000FF);
            return decoded;
          case 2:
            decoded.push_back((temp >> 10) & 0x000000FF);
            return decoded;
          default:
            throw std::runtime_error("Invalid padding in base64!");
        }
      } else {
        throw std::runtime_error("Invalid character in base64!");
      }

      ++it;
    }

    decoded.push_back((temp >> 16) & 0x000000FF);
    decoded.push_back((temp >> 8) & 0x000000FF);
    decoded.push_back((temp) & 0x000000FF);
  }
  return decoded;
}
}  // namespace cobridge_base
