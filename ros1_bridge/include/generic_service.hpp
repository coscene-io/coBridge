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
#ifndef GENERIC_SERVICE_HPP_
#define GENERIC_SERVICE_HPP_

#include <ros/serialization.h>
#include <ros/service_traits.h>

#include <string>
#include <vector>


namespace cobridge
{

struct GenericService
{
  std::string type;
  std::string md5sum;
  std::vector<uint8_t> data;

  template<typename Stream>
  inline void write(Stream & stream) const
  {
    std::memcpy(stream.getData(), data.data(), data.size());
  }

  template<typename Stream>
  inline void read(Stream & stream)
  {
    data.resize(stream.getLength());
    std::memcpy(data.data(), stream.getData(), stream.getLength());
  }
};
}  // namespace cobridge
namespace ros
{
namespace service_traits
{
template<>
struct MD5Sum<cobridge::GenericService>
{
  static const char * value(const cobridge::GenericService & m)
  {
    return m.md5sum.c_str();
  }

  static const char * value()
  {
    return "*";
  }
};

template<>
struct DataType<cobridge::GenericService>
{
  static const char * value(const cobridge::GenericService & m)
  {
    return m.type.c_str();
  }

  static const char * value()
  {
    return "*";
  }
};
}  // namespace service_traits


namespace serialization
{

template<>
struct Serializer<cobridge::GenericService>
{
  template<typename Stream>
  inline static void write(Stream & stream, const cobridge::GenericService & m)
  {
    m.write(stream);
  }

  template<typename Stream>
  inline static void read(Stream & stream, cobridge::GenericService & m)
  {
    m.read(stream);
  }

  inline static uint32_t serializedLength(const cobridge::GenericService & m)
  {
    return m.data.size();
  }
};
}  // namespace serialization

}  // namespace ros

#endif  // GENERIC_SERVICE_HPP_
