#pragma once

#include <string>
#include <vector>

#include <ros/serialization.h>
#include <ros/service_traits.h>

namespace cos_bridge {

struct GenericService {
  std::string type;
  std::string md5sum;
  std::vector<uint8_t> data;

  template <typename Stream>
  inline void write(Stream& stream) const {
    std::memcpy(stream.getData(), data.data(), data.size());
  }

  template <typename Stream>
  inline void read(Stream& stream) {
    data.resize(stream.getLength());
    std::memcpy(data.data(), stream.getData(), stream.getLength());
  }
};

}

namespace ros::service_traits {
template <>
struct MD5Sum<cos_bridge::GenericService> {
  static const char* value(const cos_bridge::GenericService& m) {
    return m.md5sum.c_str();
  }

  static const char* value() {
    return "*";
  }
};

template <>
struct DataType<cos_bridge::GenericService> {
  static const char* value(const cos_bridge::GenericService& m) {
    return m.type.c_str();
  }

  static const char* value() {
    return "*";
  }
};
}  // namespace ros::service_traits

namespace ros::serialization {

template <>
struct Serializer<cos_bridge::GenericService> {
  template <typename Stream>
  inline static void write(Stream& stream, const cos_bridge::GenericService& m) {
    m.write(stream);
  }

  template <typename Stream>
  inline static void read(Stream& stream, cos_bridge::GenericService& m) {
    m.read(stream);
  }

  inline static uint32_t serializedLength(const cos_bridge::GenericService& m) {
    return m.data.size();
  }
};
}  // namespace ros::serialization
