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

#ifndef SERIALIZATION_HPP_
#define SERIALIZATION_HPP_

#include <stdint.h>
#include <json.hpp>

#include "common.hpp"
#include "parameter.hpp"

namespace cobridge_base
{

inline void write_uint64_LE(uint8_t * buf, uint64_t val)
{
#ifdef ARCH_IS_BIG_ENDIAN
  buf[0] = val & 0xff;
  buf[1] = (val >> 8) & 0xff;
  buf[2] = (val >> 16) & 0xff;
  buf[3] = (val >> 24) & 0xff;
  buf[4] = (val >> 32) & 0xff;
  buf[5] = (val >> 40) & 0xff;
  buf[6] = (val >> 48) & 0xff;
  buf[7] = (val >> 56) & 0xff;
#else
  reinterpret_cast<uint64_t *>(buf)[0] = val;
#endif
}

inline void write_uint32_LE(uint8_t * buf, uint32_t val)
{
#ifdef ARCH_IS_BIG_ENDIAN
  buf[0] = val & 0xff;
  buf[1] = (val >> 8) & 0xff;
  buf[2] = (val >> 16) & 0xff;
  buf[3] = (val >> 24) & 0xff;
#else
  reinterpret_cast<uint32_t *>(buf)[0] = val;
#endif
}

inline uint32_t read_uint32_LE(const uint8_t * buf)
{
#ifdef ARCH_IS_BIG_ENDIAN
  // Read little-endian bytes and convert to big-endian native format
  uint32_t val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
  return val;
#else
  return reinterpret_cast<const uint32_t *>(buf)[0];
#endif
}

void to_json(nlohmann::json & json_obj, const Channel & chan);

void from_json(const nlohmann::json & json_obj, Channel & chan);

void to_json(nlohmann::json & json_obj, const ParameterValue & param_val);

void from_json(const nlohmann::json & json_obj, ParameterValue & param_val);

void to_json(nlohmann::json & json_obj, const Parameter & param);

void from_json(const nlohmann::json & json_obj, Parameter & param);

void to_json(nlohmann::json & json_obj, const Service & service);

void from_json(const nlohmann::json & json_obj, Service & service);
}  // namespace cobridge_base

#endif  // SERIALIZATION_HPP_
