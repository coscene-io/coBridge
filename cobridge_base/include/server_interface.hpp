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

#ifndef SERVER_INTERFACE_HPP_
#define SERVER_INTERFACE_HPP_

// #include <optional>
#include <functional>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "parameter.hpp"

namespace cobridge_base
{

constexpr size_t DEFAULT_SEND_BUFFER_LIMIT_BYTES = 10000000UL;  // 10 MB

using MapOfSets = std::unordered_map<std::string, std::unordered_set<std::string>>;

template<typename IdType>
class ExceptionWithId : public std::runtime_error
{
public:
  explicit ExceptionWithId(IdType id, const std::string & what_arg)
  : std::runtime_error(what_arg), _id(id)
  {}

  IdType id() const
  {
    return _id;
  }

private:
  IdType _id;
};

class ChannelError : public ExceptionWithId<ChannelId>
{
  using ExceptionWithId::ExceptionWithId;
};

class ClientChannelError : public ExceptionWithId<ClientChannelId>
{
  using ExceptionWithId::ExceptionWithId;
};

class ServiceError : public ExceptionWithId<ServiceId>
{
  using ExceptionWithId::ExceptionWithId;
};

struct ServerOptions
{
  std::vector<std::string> capabilities;
  std::vector<std::string> supported_encodings;
  std::unordered_map<std::string, std::string> metadata;
  size_t send_buffer_limit_bytes = DEFAULT_SEND_BUFFER_LIMIT_BYTES;
  bool use_tls = false;
  std::string cert_file;
  std::string key_file;
  std::string session_id;
  bool use_compression = false;
  std::vector<std::regex> client_topic_whitelist_patterns;
  std::string mac_addr;
  std::vector<std::string> ip_addrs;
};

template<typename ConnectionHandle>
struct ServerHandlers
{
  std::function<void(ChannelId, ConnectionHandle)> subscribe_handler;
  std::function<void(ChannelId, ConnectionHandle)> unsubscribe_handler;
  std::function<void(const ClientAdvertisement &, ConnectionHandle)> client_advertise_handler;
  std::function<void(ClientChannelId, ConnectionHandle)> client_unadvertise_handler;
  std::function<void(const ClientMessage &, ConnectionHandle)> client_message_handler;
  std::function<void(const std::vector<std::string> &, const optional<std::string> &,
    ConnectionHandle)> parameter_request_handler;
  std::function<void(const std::vector<Parameter> &, const optional<std::string> &,
    ConnectionHandle)> parameter_change_handler;
  std::function<void(const std::vector<std::string> &, ParameterSubscriptionOperation,
    ConnectionHandle)> parameter_subscription_handler;
  std::function<void(const ServiceRequest &, ConnectionHandle)> service_request_handler;
  std::function<void(bool)> subscribe_connection_graph_handler;
  std::function<void(const std::string &, uint32_t, ConnectionHandle)> fetch_asset_handler;
};

template<typename ConnectionHandle>
class ServerInterface
{
public:
  virtual ~ServerInterface() = default;

  virtual void start(const std::string & host, uint16_t port) = 0;

  virtual void stop() = 0;

  virtual std::vector<ChannelId> add_channels(const std::vector<ChannelWithoutId> & channels) = 0;

  virtual void remove_channels(const std::vector<ChannelId> & channel_ids) = 0;

  virtual void publish_parameter_values(
    ConnectionHandle client_handle,
    const std::vector<Parameter> & parameters,
    const optional<std::string> & request_id) = 0;

  virtual void update_parameter_values(const std::vector<Parameter> & parameters) = 0;

  virtual std::vector<ServiceId> add_services(const std::vector<ServiceWithoutId> & services) = 0;

  virtual void remove_services(const std::vector<ServiceId> & service_ids) = 0;

  virtual void set_handlers(ServerHandlers<ConnectionHandle> && handlers) = 0;

  virtual void send_message(
    ConnectionHandle client_handle, ChannelId chan_id, uint64_t timestamp,
    const uint8_t * payload, size_t payload_size) = 0;

  virtual void broadcast_time(uint64_t timestamp) = 0;

  virtual void send_service_response(
    ConnectionHandle client_handle,
    const ServiceResponse & response) = 0;

  virtual void update_connection_graph(
    const MapOfSets & published_topics,
    const MapOfSets & subscribed_topics,
    const MapOfSets & advertised_services) = 0;

  virtual void send_fetch_asset_response(
    ConnectionHandle client_handle,
    const FetchAssetResponse & response) = 0;

  virtual uint16_t get_port() = 0;

  virtual std::string remote_endpoint_string(ConnectionHandle client_handle) = 0;
};
}  // namespace cobridge_base
#endif  // SERVER_INTERFACE_HPP_
