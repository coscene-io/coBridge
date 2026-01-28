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

#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <array>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "standard.hpp"

namespace cobridge_base
{

using ChannelId = uint32_t;
using ClientChannelId = uint32_t;
using SubscriptionId = uint32_t;
using ServiceId = uint32_t;

// coBridge Websocket SubProtocol
// for more information, see `coBridge Websocket SubProtocol.md` file
constexpr char SUPPORTED_SUB_PROTOCOL[] = "coBridge.websocket.v1";
// and of course, we also support foxglove subprotocol
constexpr char FOXGLOVE_SUB_PROTOCOL[] = "foxglove.websocket.v1";
constexpr char CAPABILITY_CLIENT_PUBLISH[] = "clientPublish";
constexpr char CAPABILITY_TIME[] = "time";
constexpr char CAPABILITY_MESSAGE_TIME[] = "messageTime";
constexpr char CAPABILITY_PARAMETERS[] = "parameters";
constexpr char CAPABILITY_PARAMETERS_SUBSCRIBE[] = "parametersSubscribe";
constexpr char CAPABILITY_SERVICES[] = "services";
constexpr char CAPABILITY_CONNECTION_GRAPH[] = "connectionGraph";
constexpr char CAPABILITY_ASSETS[] = "assets";

constexpr std::array<const char *, 6> DEFAULT_CAPABILITIES = {
  CAPABILITY_CLIENT_PUBLISH,
  CAPABILITY_CONNECTION_GRAPH,
  CAPABILITY_PARAMETERS_SUBSCRIBE,
  CAPABILITY_PARAMETERS,
  CAPABILITY_SERVICES,
  CAPABILITY_ASSETS,
};

enum class BinaryOpcode : uint8_t
{
  MESSAGE_DATA = 1,
  TIME_DATA = 2,
  SERVICE_CALL_RESPONSE = 3,
  FETCH_ASSET_RESPONSE = 4,
};

enum class ClientBinaryOpcode : uint8_t
{
  MESSAGE_DATA = 1,
  SERVICE_CALL_REQUEST = 2,
};

enum class WebSocketLogLevel
{
  Debug,
  Info,
  Warn,
  Error,
  Critical,
};

struct ChannelWithoutId
{
  std::string topic;
  std::string encoding;
  std::string schema_name;
  std::string schema;
  optional<std::string> schema_encoding;

  bool operator==(const ChannelWithoutId & other) const
  {
    return topic == other.topic && encoding == other.encoding && schema_name == other.schema_name &&
           schema == other.schema && schema_encoding == other.schema_encoding;
  }
};

struct Channel : ChannelWithoutId
{
  ChannelId id = 0;

  Channel() = default;  // requirement for json conversions.
  Channel(ChannelId id, ChannelWithoutId ch)
  : ChannelWithoutId(std::move(ch)), id(id)
  {}

  bool operator==(const Channel & other) const
  {
    return id == other.id && ChannelWithoutId::operator==(other);
  }
};

struct ClientAdvertisement
{
  ClientChannelId channel_id;
  std::string topic;
  std::string encoding;
  std::string schema_name;
  std::vector<uint8_t> schema;
};

struct ClientMessage
{
  uint64_t log_time;
  uint64_t publish_time;
  uint32_t sequence;
  ClientAdvertisement advertisement;
  size_t data_length;
  std::vector<uint8_t> data;

  ClientMessage(
    uint64_t log_time, uint64_t publish_time, uint32_t sequence,
    const ClientAdvertisement & advertisement, size_t data_length,
    const uint8_t * raw_data)
  : log_time(log_time), publish_time(publish_time), sequence(sequence),
    advertisement(advertisement),
    data_length(data_length), data(data_length)
  {
    std::memcpy(data.data(), raw_data, data_length);
  }

  static const size_t MSG_PAYLOAD_OFFSET = 5;

  const uint8_t * getData() const
  {
    return data.data() + MSG_PAYLOAD_OFFSET;
  }

  std::size_t getLength() const
  {
    return data.size() - MSG_PAYLOAD_OFFSET;
  }
};

struct ServiceWithoutId
{
  std::string name;
  std::string type;
  std::string request_schema;
  std::string response_schema;
};

struct Service : ServiceWithoutId
{
  ServiceId id = 0;

  Service() = default;

  Service(const ServiceWithoutId & s, const ServiceId & id)
  : ServiceWithoutId(s), id(id)
  {}
};

struct ServiceResponse
{
  ServiceId service_id;
  uint32_t call_id;
  std::string encoding;
  std::vector<uint8_t> serv_data;

  size_t size() const
  {
    return 4 + 4 + 4 + encoding.size() + serv_data.size();
  }

  void read(const uint8_t * data, size_t size);

  void write(uint8_t * data) const;

  bool operator==(const ServiceResponse & other) const
  {
    return service_id == other.service_id && call_id == other.call_id &&
           encoding == other.encoding && serv_data == other.serv_data;
  }
};

using ServiceRequest = ServiceResponse;

enum class FetchAssetStatus : uint8_t
{
  Success = 0,
  Error = 1,
};

struct FetchAssetResponse
{
  uint32_t request_id;
  FetchAssetStatus status;
  std::string error_message;
  std::vector<uint8_t> data;
};
}  // namespace cobridge_base

namespace std
{
template<>
struct hash<cobridge_base::ClientBinaryOpcode>
{
  std::size_t operator()(const cobridge_base::ClientBinaryOpcode & opcode) const
  {
    return std::hash<uint8_t>()(static_cast<uint8_t>(opcode));
  }
};
}  // namespace std

#endif  // COMMON_HPP_
