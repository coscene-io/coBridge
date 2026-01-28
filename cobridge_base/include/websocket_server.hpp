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

#ifndef WEBSOCKET_SERVER_HPP_
#define WEBSOCKET_SERVER_HPP_

#include <json.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

// #include <optional>
// #include <shared_mutex>
// #include <string_view>
#include <mutex>

#include <string>
#include <queue>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common.hpp"
#include "callback_queue.hpp"
#include "parameter.hpp"
#include "serialization.hpp"
#include "regex_utils.hpp"
#include "server_interface.hpp"
#include "websocket_logging.hpp"

#define COS_DEBOUNCE(f, ms) \
  { \
    static auto last_call = std::chrono::system_clock::now(); \
    const auto now = std::chrono::system_clock::now(); \
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_call).count() > ms) { \
      last_call = now; \
      f(); \
    } \
  }

#define MAX_CLIENT_COUNT 1
#define KIB 1024

namespace cobridge_base
{

constexpr uint32_t
string_hash_impl(const char * str, std::size_t len, std::size_t i, uint32_t result)
{
  return i >= len ? result :
         string_hash_impl(str, len, i + 1, (static_cast<uint32_t>(str[i]) ^ result) * 0x01000193);
}

constexpr uint32_t
string_hash(const string_view str)
{
  return string_hash_impl(str.data(), str.size(), 0, 0x811C9DC5);
}

constexpr auto LOGIN = string_hash("login");
constexpr auto SUBSCRIBE = string_hash("subscribe");
constexpr auto UNSUBSCRIBE = string_hash("unsubscribe");
constexpr auto ADVERTISE = string_hash("advertise");
constexpr auto UNADVERTISE = string_hash("unadvertise");
constexpr auto GET_PARAMETERS = string_hash("getParameters");
constexpr auto SET_PARAMETERS = string_hash("setParameters");
constexpr auto SUBSCRIBE_PARAMETER_UPDATES = string_hash("subscribeParameterUpdates");
constexpr auto UNSUBSCRIBE_PARAMETER_UPDATES = string_hash("unsubscribeParameterUpdates");
constexpr auto SUBSCRIBE_CONNECTION_GRAPH = string_hash("subscribeConnectionGraph");
constexpr auto UNSUBSCRIBE_CONNECTION_GRAPH = string_hash("unsubscribeConnectionGraph");
constexpr auto FETCH_ASSET = string_hash("fetchAsset");
}  // namespace cobridge_base

namespace cobridge_base
{
using Json = nlohmann::json;
using ConnHandle = websocketpp::connection_hdl;
using OpCode = websocketpp::frame::opcode::value;

constexpr websocketpp::log::level APP = websocketpp::log::alevel::app;
constexpr websocketpp::log::level WARNING = websocketpp::log::elevel::warn;
constexpr websocketpp::log::level RECOVERABLE = websocketpp::log::elevel::rerror;
constexpr websocketpp::log::level FATAL = websocketpp::log::elevel::fatal;

/// Map of required capability by client operation (text).
const std::unordered_map<std::string, std::string> CAPABILITY_BY_CLIENT_OPERATION = {
  // {"subscribe", },   // No required capability.
  // {"unsubscribe", }, // No required capability.
  {"advertise", CAPABILITY_CLIENT_PUBLISH},
  {"unadvertise", CAPABILITY_CLIENT_PUBLISH},
  {"getParameters", CAPABILITY_PARAMETERS},
  {"setParameters", CAPABILITY_PARAMETERS},
  {"subscribeParameterUpdates", CAPABILITY_PARAMETERS_SUBSCRIBE},
  {"unsubscribeParameterUpdates", CAPABILITY_PARAMETERS_SUBSCRIBE},
  {"subscribeConnectionGraph", CAPABILITY_CONNECTION_GRAPH},
  {"unsubscribeConnectionGraph", CAPABILITY_CONNECTION_GRAPH},
  {"fetchAsset", CAPABILITY_ASSETS},
};

/// Map of required capability by client operation (binary).
const std::unordered_map<ClientBinaryOpcode, std::string> CAPABILITY_BY_CLIENT_BINARY_OPERATION = {
  {ClientBinaryOpcode::MESSAGE_DATA, CAPABILITY_CLIENT_PUBLISH},
  {ClientBinaryOpcode::SERVICE_CALL_REQUEST, CAPABILITY_SERVICES},
};

enum class StatusLevel : uint8_t
{
  Info = 0,
  Warning = 1,
  Error = 2,
};

constexpr websocketpp::log::level
status_level_to_log_level(StatusLevel level)
{
  return level == StatusLevel::Info ? APP :
         level == StatusLevel::Warning ? WARNING :
         level == StatusLevel::Error ? RECOVERABLE :
         FATAL;
}

template<typename ServerConfiguration>
class Server final : public ServerInterface<ConnHandle>
{
public:
  using ServerType = websocketpp::server<ServerConfiguration>;
  using ConnectionType = websocketpp::connection<ServerConfiguration>;
  using MessagePtr = typename ServerType::message_ptr;
  using Tcp = websocketpp::lib::asio::ip::tcp;

  explicit Server(std::string name, LogCallback logger, ServerOptions options);

  ~Server() override;

  Server(const Server &) = delete;

  Server(Server &&) = delete;

  Server & operator=(const Server &) = delete;

  Server & operator=(Server &&) = delete;

  void start(const std::string & host, uint16_t port) override;

  void stop() override;

  std::vector<ChannelId> add_channels(const std::vector<ChannelWithoutId> & channels) override;

  void remove_channels(const std::vector<ChannelId> & channel_ids) override;

  void publish_parameter_values(
    ConnHandle client_handle, const std::vector<Parameter> & parameters,
    const optional<std::string> & request_id) override;

  void update_parameter_values(const std::vector<Parameter> & parameters) override;

  std::vector<ServiceId> add_services(const std::vector<ServiceWithoutId> & services) override;

  void remove_services(const std::vector<ServiceId> & service_ids) override;

  void set_handlers(ServerHandlers<ConnHandle> && handlers) override;

  void send_message(
    ConnHandle client_handle, ChannelId chan_id, uint64_t timestamp,
    const uint8_t * payload, size_t payload_size) override;

  void broadcast_time(uint64_t timestamp) override;

  void send_service_response(ConnHandle client_handle, const ServiceResponse & response) override;

  void update_connection_graph(
    const MapOfSets & published_topics, const MapOfSets & subscribed_topics,
    const MapOfSets & advertised_services) override;

  void
  send_fetch_asset_response(ConnHandle client_handle, const FetchAssetResponse & response) override;

  uint16_t get_port() override;

  std::string remote_endpoint_string(ConnHandle client_handle) override;

private:
  void socket_init(ConnHandle hdl);

  void setup_tls_handler();

  bool validate_connection(ConnHandle hdl);

  void handle_connection_opened(ConnHandle hdl);

  void handle_connection_closed(ConnHandle hdl);

  void handle_message(ConnHandle hdl, MessagePtr msg);

  void handle_text_message(ConnHandle hdl, MessagePtr msg);

  void handle_binary_message(ConnHandle hdl, MessagePtr msg);

  void send_json(ConnHandle hdl, Json && payload, bool reliable = true);

  void send_raw_json(ConnHandle hdl, const std::string & payload, bool reliable = true);

  void send_binary(
    ConnHandle hdl, const uint8_t * payload, size_t payload_size, bool reliable = true);

  void send_status_and_log_msg(
    ConnHandle client_handle, const StatusLevel level,
    const std::string & message);

  void unsubscribe_params_without_subscriptions(
    ConnHandle hdl,
    const std::unordered_set<std::string> & param_names);

  bool is_parameter_subscribed(const std::string & param_name) const;

  bool has_capability(const std::string & capability) const;

  bool has_handler(uint32_t op) const;

  void handle_login(const Json & payload, ConnHandle hdl);

  void handle_subscribe(const Json & payload, ConnHandle hdl);

  void handle_unsubscribe(const Json & payload, ConnHandle hdl);

  void handle_advertise(const Json & payload, ConnHandle hdl);

  void handle_unadvertise(const Json & payload, ConnHandle hdl);

  void handle_get_parameters(const Json & payload, ConnHandle hdl);

  void handle_set_parameters(const Json & payload, ConnHandle hdl);

  void handle_subscribe_parameter_updates(const Json & payload, ConnHandle hdl);

  void handle_unsubscribe_parameter_updates(const Json & payload, ConnHandle hdl);

  void handle_subscribe_connection_graph(ConnHandle hdl);

  void handle_unsubscribe_connection_graph(ConnHandle hdl);

  void handle_fetch_asset(const Json & payload, ConnHandle hdl);

private:
  struct ClientInfo
  {
    std::string endpoint_name;
    std::string user_name;
    std::string user_id;
    ConnHandle handle;
    std::unordered_map<ChannelId, SubscriptionId> subscriptions_by_channel;
    std::unordered_set<ClientChannelId> advertised_channels;
    bool subscribed_to_connection_graph = false;
    bool login = true;

    explicit ClientInfo(
      std::string endpoint_name, std::string user_name, std::string user_id,
      ConnHandle handle)
    : endpoint_name(std::move(endpoint_name)),
      user_name(std::move(user_name)),
      user_id(std::move(user_id)),
      handle(std::move(handle))
    {}

    ClientInfo(const ClientInfo &) = delete;

    ClientInfo & operator=(const ClientInfo &) = delete;

    ClientInfo(ClientInfo &&) = default;

    ClientInfo & operator=(ClientInfo &&) = default;

    std::string get_user_info() const
    {
      return "'" + this->user_name + " (" + this->user_id + ")' ";
    }
  };

  struct Message
  {
  };

  std::string _name;
  LogCallback _logger;
  ServerOptions _options;
  ServerType _server;
  std::unique_ptr<std::thread> _server_thread;
  std::unique_ptr<CallbackQueue> _handler_callback_queue;

  uint32_t _next_channel_id = 0;
  std::map<ConnHandle, ClientInfo, std::owner_less<ConnHandle>> _clients;
  std::unordered_map<ChannelId, Channel> _channels;
  std::map<ConnHandle, std::unordered_map<ClientChannelId, ClientAdvertisement>,
    std::owner_less<ConnHandle>>
  _client_channels;
  std::map<ConnHandle, std::unordered_set<std::string>, std::owner_less<ConnHandle>>
  _client_param_subscriptions;
  ServiceId _nextService_id = 0;
  std::unordered_map<ServiceId, ServiceWithoutId> _services;
  ServerHandlers<ConnHandle> _handlers;
  std::mutex _clients_mutex;
  std::mutex _channels_mutex;
  std::mutex _client_channels_mutex;
  std::mutex _services_mutex;
  std::mutex _client_param_subscriptions_mutex;

  struct
  {
    int subscription_count = 0;
    MapOfSets published_topics;
    MapOfSets subscribed_topics;
    MapOfSets advertised_services;
  } _connection_graph;
  std::mutex _connection_graph_mutex;
};

//-----------------------------------------------------------------------------------------------------------------
// public functions
template<typename ServerConfiguration>
inline Server<ServerConfiguration>::Server(
  std::string name, LogCallback logger, ServerOptions options)
: _name(std::move(name)), _logger(std::move(logger)), _options(std::move(options))
{
  _server.get_alog().set_callback(_logger);
  _server.get_elog().set_callback(_logger);

  websocketpp::lib::error_code ec;
  _server.init_asio(ec);
  if (ec) {
    throw std::runtime_error("Failed to initialize websocket server: " + ec.message());
  }

  _server.clear_access_channels(websocketpp::log::alevel::all);
  _server.set_access_channels(APP);
  _server.set_tcp_pre_init_handler(std::bind(&Server::socket_init, this, std::placeholders::_1));
  this->setup_tls_handler();
  _server.set_validate_handler(
    std::bind(&Server::validate_connection, this, std::placeholders::_1));
  _server.set_open_handler(
    std::bind(&Server::handle_connection_opened, this, std::placeholders::_1));
  _server.set_close_handler(
    [this](ConnHandle hdl) {
      _handler_callback_queue->add_callback(
        [this, hdl]() {
          this->handle_connection_closed(hdl);
        });
    });
  _server.set_message_handler(
    [this](ConnHandle hdl, MessagePtr msg) {
      _handler_callback_queue->add_callback(
        [this, hdl, msg]() {
          this->handle_message(hdl, msg);
        });
    });
  _server.set_reuse_addr(true);
  _server.set_listen_backlog(128);

  // Callback queue for handling client requests and disconnections.
  // _handler_callback_queue = std::make_unique<CallbackQueue>(_logger, /*numThreads=*/ 1ul);
  _handler_callback_queue =
    std::unique_ptr<CallbackQueue>(new CallbackQueue(_logger, /*numThreads=*/ 1ul));
}

template<typename ServerConfiguration>
inline Server<ServerConfiguration>::~Server() = default;

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::start(const std::string & host, uint16_t port)
{
  if (_server_thread) {
    throw std::runtime_error("Server already started");
  }

  websocketpp::lib::error_code ec;

  _server.listen(host, std::to_string(port), ec);
  if (ec) {
    throw std::runtime_error(
            "Failed to listen on port " + std::to_string(port) + ": " +
            ec.message());
  }

  _server.start_accept(ec);
  if (ec) {
    throw std::runtime_error("Failed to start accepting connections: " + ec.message());
  }

  _server_thread = std::unique_ptr<std::thread>(
    new std::thread(
      [this]() {
        _server.get_alog().write(APP, "WebSocket server run loop started");
        _server.run();
        _server.get_alog().write(APP, "WebSocket server run loop stopped");
      }));

  if (!_server.is_listening()) {
    throw std::runtime_error("WebSocket server failed to listen on port " + std::to_string(port));
  }

  websocketpp::lib::asio::error_code asioEc;
  auto endpoint = _server.get_local_endpoint(asioEc);
  if (asioEc) {
    throw std::runtime_error("Failed to resolve the local endpoint: " + ec.message());
  }

  const std::string protocol = _options.use_tls ? "wss" : "ws";
  auto address = endpoint.address();
  _server.get_alog().write(
    APP, "WebSocket server listening at " + protocol + "://" +
    ip_address_to_string(address) + ":" + std::to_string(endpoint.port()));
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::stop()
{
  if (_server.stopped()) {
    return;
  }

  _server.get_alog().write(APP, "Stopping WebSocket server");
  websocketpp::lib::error_code ec;

  _server.stop_perpetual();

  if (_server.is_listening()) {
    _server.stop_listening(ec);
    if (ec) {
      _server.get_elog().write(RECOVERABLE, "Failed to stop listening: " + ec.message());
    }
  }

  std::vector<std::shared_ptr<ConnectionType>> connections;
  {
    std::lock_guard<std::mutex> lock(_clients_mutex);
    connections.reserve(_clients.size());
    for (const auto & client : _clients) {
      if (auto connection = _server.get_con_from_hdl(client.first, ec)) {
        connections.push_back(connection);
      }
    }
  }

  if (!connections.empty()) {
    _server.get_alog().write(
      APP, "Closing " + std::to_string(connections.size()) + " client connection(s)");

    // Iterate over all client connections and start the close connection handshake
    for (const auto & connection : connections) {
      connection->close(websocketpp::close::status::going_away, "server shutdown", ec);
      if (ec) {
        _server.get_elog().write(RECOVERABLE, "Failed to close connection: " + ec.message());
      }
    }

    // Wait for all connections to close
    constexpr size_t MAX_SHUTDOWN_MS = 1000;
    constexpr size_t SLEEP_MS = 10;
    size_t duration_ms = 0;
    while (!_server.stopped() && duration_ms < MAX_SHUTDOWN_MS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
      _server.poll_one();
      duration_ms += SLEEP_MS;
    }

    if (!_server.stopped()) {
      _server.get_elog().write(
        RECOVERABLE, "Failed to close all connections, forcefully stopping");
      for (const auto & hdl : connections) {
        if (auto con = _server.get_con_from_hdl(hdl, ec)) {
          _server.get_elog().write(
            RECOVERABLE,
            "Terminating connection to " + remote_endpoint_string(hdl));
          con->terminate(ec);
        }
      }
      _server.stop();
    }
  }

  _server.get_alog().write(APP, "All WebSocket connections closed");

  if (_server_thread) {
    _server.get_alog().write(APP, "Waiting for WebSocket server run loop to terminate");
    _server_thread->join();
    _server_thread.reset();
    _server.get_alog().write(APP, "WebSocket server run loop terminated");
  }

  std::lock_guard<std::mutex> lock(_clients_mutex);
  _clients.clear();
}

template<typename ServerConfiguration>
inline std::vector<ChannelId> Server<ServerConfiguration>::add_channels(
  const std::vector<ChannelWithoutId> & channels)
{
  if (channels.empty()) {
    return {};
  }

  std::vector<ChannelId> channel_ids;
  channel_ids.reserve(channels.size());
  Json::array_t channels_json;

  {
    std::lock_guard<std::mutex> lock(_channels_mutex);
    for (const auto & channel_without_id : channels) {
      const auto new_id = ++_next_channel_id;
      channel_ids.push_back(new_id);
      Channel new_channel{new_id, channel_without_id};
      Json j = new_channel;
      channels_json.emplace_back(new_channel);
      _channels.emplace(new_id, std::move(new_channel));
    }
  }

  const auto msg =
    Json{{"op", "advertise"}, {"channels", channels_json}}.dump();
  std::lock_guard<std::mutex> clients_lock(_clients_mutex);
  for (const auto & client : _clients) {
    send_raw_json(client.first, msg);
  }

  return channel_ids;
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::remove_channels(const std::vector<ChannelId> & channel_ids)
{
  if (channel_ids.empty()) {
    return;
  }

  {
    std::lock_guard<std::mutex> channels_lock(_channels_mutex);
    for (auto channel_id : channel_ids) {
      _channels.erase(channel_id);
    }
  }

  const auto msg =
    Json{{"op", "unadvertise"}, {"channelIds", channel_ids}}.dump();

  std::lock_guard<std::mutex> clients_lock(_clients_mutex);
  for (auto & client : _clients) {
    for (auto channel_id : channel_ids) {
      const auto it = client.second.subscriptions_by_channel.find(channel_id);
      if (it != client.second.subscriptions_by_channel.end()) {
        client.second.subscriptions_by_channel.erase(it);
      }
    }
    send_raw_json(client.first, msg);
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::publish_parameter_values(
  ConnHandle client_handle, const std::vector<Parameter> & parameters,
  const optional<std::string> & request_id)
{
  // Filter out parameters which are not set.
  std::vector<Parameter> non_empty_parameters;
  std::copy_if(
    parameters.begin(), parameters.end(), std::back_inserter(non_empty_parameters),
    [](const Parameter & p) {
      return p.get_type() != ParameterType::PARAMETER_NOT_SET;
    });

  Json json_payload{{"op", "parameterValues"},
    {"parameters", non_empty_parameters}};
  if (request_id.has_value()) {
    json_payload["id"] = request_id.value();
  }
  send_raw_json(client_handle, json_payload.dump());
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::update_parameter_values(
  const std::vector<Parameter> & parameters)
{
  std::lock_guard<std::mutex> lock(_client_param_subscriptions_mutex);
  for (const auto & client_param_subscriptions : _client_param_subscriptions) {
    std::vector<Parameter> params_to_send_to_client;

    // Only consider parameters that are subscribed by the client
    std::copy_if(
      parameters.begin(), parameters.end(), std::back_inserter(params_to_send_to_client),
      [client_param_subscriptions](const Parameter & param) {
        return client_param_subscriptions.second.find(param.get_name()) !=
        client_param_subscriptions.second.end();
      });

    if (!params_to_send_to_client.empty()) {
      publish_parameter_values(
        client_param_subscriptions.first, params_to_send_to_client, optional<std::string>(nullopt));
    }
  }
}

template<typename ServerConfiguration>
inline std::vector<ServiceId> Server<ServerConfiguration>::add_services(
  const std::vector<ServiceWithoutId> & services)
{
  if (services.empty()) {
    return {};
  }

  std::lock_guard<std::mutex> lock(_services_mutex);
  std::vector<ServiceId> service_ids;
  Json new_services;
  for (const auto & service : services) {
    const ServiceId service_id = ++_nextService_id;
    _services.emplace(service_id, service);
    service_ids.push_back(service_id);
    new_services.push_back(Service(service, service_id));
  }

  const auto msg =
    Json{{"op", "advertiseServices"}, {"services", std::move(new_services)}}
  .dump();
  std::lock_guard<std::mutex> clients_lock(_clients_mutex);
  for (const auto & client : _clients) {
    send_raw_json(client.first, msg);
  }
  return service_ids;
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::remove_services(const std::vector<ServiceId> & service_ids)
{
  std::lock_guard<std::mutex> lock(_services_mutex);
  std::vector<ServiceId> removed_services;
  for (const auto & service_id : service_ids) {
    const auto iter = _services.find(service_id);
    if (iter != _services.end()) {
      _services.erase(iter);
      removed_services.push_back(service_id);
    }
  }

  if (!removed_services.empty()) {
    const auto msg =
      Json {
      {"op", "unadvertiseServices"},
      {"serviceIds", std::move(removed_services)}
    }
    .dump();
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    for (const auto & client : _clients) {
      send_raw_json(client.first, msg);
    }
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::set_handlers(ServerHandlers<ConnHandle> && handlers)
{
  _handlers = handlers;
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_message(
  ConnHandle client_handle, ChannelId chan_id,
  uint64_t timestamp, const uint8_t * payload,
  size_t payload_size)
{
  websocketpp::lib::error_code ec;
  const auto con = _server.get_con_from_hdl(client_handle, ec);
  if (ec || !con) {
    return;
  }

  SubscriptionId sub_id;
  {
    std::lock_guard<std::mutex> lock(_clients_mutex);
    const auto client_handle_and_info_iter = _clients.find(client_handle);
    if (client_handle_and_info_iter == _clients.end()) {
      return;  // Client got removed in the meantime.
    }

    const auto & client = client_handle_and_info_iter->second;
    const auto & subs = client.subscriptions_by_channel.find(chan_id);
    if (subs == client.subscriptions_by_channel.end()) {
      return;  // Client not subscribed to this channel.
    }
    sub_id = subs->second;
  }

  std::array<uint8_t, 1 + 4 + 8> msg_header;
  msg_header[0] = uint8_t(BinaryOpcode::MESSAGE_DATA);
  write_uint32_LE(msg_header.data() + 1, sub_id);
  write_uint64_LE(msg_header.data() + 5, timestamp);

  const size_t message_size = msg_header.size() + payload_size;
  auto message = con->get_message(OpCode::BINARY, message_size);
  message->set_compressed(_options.use_compression);

  message->set_payload(msg_header.data(), msg_header.size());
  message->append_payload(payload, payload_size);
  con->send(message, false);
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::broadcast_time(uint64_t timestamp)
{
  std::array<uint8_t, 1 + 8> message;
  message[0] = uint8_t(BinaryOpcode::TIME_DATA);
  write_uint64_LE(message.data() + 1, timestamp);

  std::lock_guard<std::mutex> lock(_clients_mutex);
  for (const auto & client : _clients) {
    send_binary(client.first, message.data(), message.size());
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_service_response(
  ConnHandle client_handle, const ServiceResponse & response)
{
  std::vector<uint8_t> payload(1 + response.size());
  payload[0] = uint8_t(BinaryOpcode::SERVICE_CALL_RESPONSE);
  response.write(payload.data() + 1);
  send_binary(client_handle, payload.data(), payload.size());
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::update_connection_graph(
  const MapOfSets & published_topics, const MapOfSets & subscribed_topics,
  const MapOfSets & advertised_services)
{
  Json::array_t publisher_diff, subscriber_diff, services_diff;
  std::unordered_set<std::string> topic_names, service_names;
  std::unordered_set<std::string> known_topic_names, known_service_names;
  {
    std::lock_guard<std::mutex> lock(_connection_graph_mutex);
    for (const auto & topic : published_topics) {
      const auto name = topic.first;
      const auto publisher_ids = topic.second;
      const auto iter = _connection_graph.published_topics.find(name);
      if (iter == _connection_graph.published_topics.end() ||
        _connection_graph.published_topics[name] != publisher_ids)
      {
        publisher_diff.push_back(
          Json{{"name", name}, {"publisherIds", publisher_ids}});
      }
      topic_names.insert(name);
    }
    for (const auto & topic : subscribed_topics) {
      const auto name = topic.first;
      const auto subscriber_ids = topic.second;
      const auto iter = _connection_graph.subscribed_topics.find(name);
      if (iter == _connection_graph.subscribed_topics.end() ||
        _connection_graph.subscribed_topics[name] != subscriber_ids)
      {
        subscriber_diff.push_back(
          Json{{"name", name}, {"subscriberIds", subscriber_ids}});
      }
      topic_names.insert(name);
    }
    for (const auto & service : advertised_services) {
      const auto name = service.first;
      const auto provider_ids = service.second;
      const auto iter = _connection_graph.advertised_services.find(name);
      if (iter == _connection_graph.advertised_services.end() ||
        _connection_graph.advertised_services[name] != provider_ids)
      {
        services_diff.push_back(
          Json{{"name", name}, {"providerIds", provider_ids}});
      }
      service_names.insert(name);
    }

    for (const auto & name_with_ids : _connection_graph.published_topics) {
      known_topic_names.insert(name_with_ids.first);
    }
    for (const auto & name_with_ids : _connection_graph.subscribed_topics) {
      known_topic_names.insert(name_with_ids.first);
    }
    for (const auto & name_with_ids : _connection_graph.advertised_services) {
      known_service_names.insert(name_with_ids.first);
    }

    _connection_graph.published_topics = published_topics;
    _connection_graph.subscribed_topics = subscribed_topics;
    _connection_graph.advertised_services = advertised_services;
  }

  std::vector<std::string> removed_topics, removed_services;
  std::copy_if(
    known_topic_names.begin(), known_topic_names.end(), std::back_inserter(removed_topics),
    [&topic_names](const std::string & topic) {
      return topic_names.find(topic) == topic_names.end();
    });
  std::copy_if(
    known_service_names.begin(), known_service_names.end(),
    std::back_inserter(removed_services),
    [&service_names](const std::string & service) {
      return service_names.find(service) == service_names.end();
    });

  if (publisher_diff.empty() && subscriber_diff.empty() && services_diff.empty() &&
    removed_topics.empty() && removed_services.empty())
  {
    return;
  }

  const Json msg = {
    {"op", "connectionGraphUpdate"},
    {"publishedTopics", publisher_diff},
    {"subscribedTopics", subscriber_diff},
    {"advertisedServices", services_diff},
    {"removedTopics", removed_topics},
    {"removedServices", removed_services},
  };
  const auto payload = msg.dump();

  std::lock_guard<std::mutex> clients_lock(_clients_mutex);
  for (const auto & client : _clients) {
    if (client.second.subscribed_to_connection_graph) {
      _server.send(client.first, payload, OpCode::TEXT);
    }
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_fetch_asset_response(
  ConnHandle client_handle, const FetchAssetResponse & response)
{
  websocketpp::lib::error_code ec;
  const auto con = _server.get_con_from_hdl(client_handle, ec);
  if (ec || !con) {
    return;
  }

  const size_t err_msg_size =
    response.status == FetchAssetStatus::Error ? response.error_message.size() : 0ul;
  const size_t data_size =
    response.status == FetchAssetStatus::Success ? response.data.size() : 0ul;
  const size_t message_size = 1 + 4 + 1 + 4 + err_msg_size + data_size;

  auto message = con->get_message(OpCode::BINARY, message_size);

  const auto op = BinaryOpcode::FETCH_ASSET_RESPONSE;
  message->append_payload(&op, 1);

  std::array<uint8_t, 4> uint32_data;
  write_uint32_LE(uint32_data.data(), response.request_id);

  message->append_payload(uint32_data.data(), uint32_data.size());

  const auto status = static_cast<uint8_t>(response.status);
  message->append_payload(&status, 1);

  write_uint32_LE(uint32_data.data(), response.error_message.size());
  message->append_payload(uint32_data.data(), uint32_data.size());
  message->append_payload(response.error_message.data(), err_msg_size);

  message->append_payload(response.data.data(), data_size);
  con->send(message, true);
}

template<typename ServerConfiguration>
inline uint16_t Server<ServerConfiguration>::get_port()
{
  websocketpp::lib::asio::error_code ec;
  auto endpoint = _server.get_local_endpoint(ec);
  if (ec) {
    throw std::runtime_error("Server not listening on any port. Has it been started before?");
  }
  return endpoint.port();
}

template<typename ServerConfiguration>
inline std::string
Server<ServerConfiguration>::remote_endpoint_string(cobridge_base::ConnHandle client_handle)
{
  websocketpp::lib::error_code ec;
  const auto con = _server.get_con_from_hdl(client_handle, ec);
  return con ? con->get_remote_endpoint() : "(unknown)";
}


//-----------------------------------------------------------------------------------------------------------------
// private functions
template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::socket_init(ConnHandle hdl)
{
  websocketpp::lib::asio::error_code ec;
  _server.get_con_from_hdl(hdl)->get_raw_socket().set_option(Tcp::no_delay(true), ec);
  if (ec) {
    _server.get_elog().write(RECOVERABLE, "Failed to set TCP_NODELAY: " + ec.message());
  }
}

template<typename ServerConfiguration>
inline bool Server<ServerConfiguration>::validate_connection(ConnHandle hdl)
{
  auto con = _server.get_con_from_hdl(hdl);

  const auto & sub_protocols = con->get_requested_subprotocols();

  auto it = std::find(sub_protocols.begin(), sub_protocols.end(), SUPPORTED_SUB_PROTOCOL);
  if (it != sub_protocols.end()) {
    con->select_subprotocol(SUPPORTED_SUB_PROTOCOL);
    return true;
  }

  it = std::find(sub_protocols.begin(), sub_protocols.end(), FOXGLOVE_SUB_PROTOCOL);
  if (it != sub_protocols.end()) {
    con->select_subprotocol(FOXGLOVE_SUB_PROTOCOL);
    return true;
  }
  _server.get_alog().write(
    APP, "Rejecting client " + remote_endpoint_string(hdl) +
    " which did not declare support for subprotocol " +
    SUPPORTED_SUB_PROTOCOL);
  return false;
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_login(const Json & payload, ConnHandle hdl)
{
  const auto user_name = payload.at("username").get<std::string>();
  const auto user_id = payload.at("userId").get<std::string>();
  const auto endpoint = remote_endpoint_string(hdl);
  _server.get_alog().write(APP, "'" + user_name + " (" + user_id + ")' is logging in.");

  {
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    if (_clients.size() != 0) {
      for (auto it = _clients.begin(); it != _clients.end(); ) {
        auto con = _server.get_con_from_hdl(it->first);
        con->send(
          Json(
          {
            {"op", "kicked"},
            {"message", "The client was forcibly disconnected by the server."},
            {"userId", user_id},
            {"username", user_name}
          })
          .dump(), true);
        it->second.login = false;

        _server.get_alog().write(
          APP, it->second.get_user_info() + "was kicked by '" + user_name + " (" + user_id + ")'");
        ++it;
      }
    }
    _clients.emplace(hdl, ClientInfo(endpoint, user_name, user_id, hdl));
  }
  const auto con = _server.get_con_from_hdl(hdl);
  con->send(
    Json(
    {
      {"op", "serverInfo"},
      {"name", _name},
      {"capabilities", _options.capabilities},
      {"supportedEncodings", _options.supported_encodings},
      {"metadata", _options.metadata},
      {"sessionId", _options.session_id},
    })
    .dump(), true);

  std::vector<Channel> channels;
  {
    std::lock_guard<std::mutex> lock(_channels_mutex);
    for (const auto & channel : _channels) {
      channels.push_back(channel.second);
    }
  }

  send_json(
    hdl, {
      {"op", "advertise"},
      {"channels", std::move(channels)},
    });

  std::vector<Service> services;
  {
    std::lock_guard<std::mutex> lock(_services_mutex);
    for (const auto & service : _services) {
      services.emplace_back(service.second, service.first);
    }
  }
  send_json(
    hdl, {
      {"op", "advertiseServices"},
      {"services", std::move(services)},
    });
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::handle_connection_opened(cobridge_base::ConnHandle hdl)
{
  auto con = _server.get_con_from_hdl(hdl);
  const auto endpoint = remote_endpoint_string(hdl);
  _server.get_alog().write(
    APP, "websocket connection  " + endpoint + " connected via " +
    con->get_resource());
  std::string link_type = "other";
  std::string colink_ip = _options.metadata["COLINK"];

  if (!colink_ip.empty()) {
    std::string endpoint_ip = endpoint;
    size_t colon_pos = endpoint_ip.find(":");
    if (colon_pos != std::string::npos) {
      endpoint_ip = endpoint_ip.substr(0, colon_pos);
    }
    if (endpoint_ip == colink_ip) {
      link_type = "colink";
    }
  }

  if (_clients.size() != 0) {
    std::string login_user_id;
    std::string login_user_name;
    for (auto it = _clients.begin(); it != _clients.end(); ) {
      if (it->second.login) {
        login_user_id = it->second.user_id;
        login_user_name = it->second.user_name;
        break;
      }
      it++;
    }
    send_json(
      hdl, {
        {"op", "login"},
        {"userId", login_user_id},
        {"username", login_user_name},
        {"infoPort", "21275"},
        {"macAddr", _options.mac_addr},
        {"lanCandidates", _options.ip_addrs},
        {"linkType", link_type}
      });
  } else {
    send_json(
      hdl, {{"op", "login"}, {"userId", ""}, {"username", ""}, {"infoPort", "21275"},
        {"macAddr", _options.mac_addr}, {"lanCandidates", _options.ip_addrs},
        {"linkType", link_type}});
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::handle_connection_closed(ConnHandle hdl)
{
  const auto endpoint = remote_endpoint_string(hdl);
  std::unordered_map<ChannelId, SubscriptionId> old_subscriptions_by_channel;
  std::unordered_set<ClientChannelId> old_advertised_channels;
  std::string client_name;
  bool was_subscribed_to_connection_graph;
  {
    std::lock_guard<std::mutex> lock(_clients_mutex);
    const auto client_iter = _clients.find(hdl);
    if (client_iter == _clients.end()) {
      _server.get_elog().write(
        RECOVERABLE, "Client " + remote_endpoint_string(hdl) +
        " disconnected without login");
      return;
    }

    const auto & client = client_iter->second;
    client_name = client.get_user_info();

    old_subscriptions_by_channel = std::move(client.subscriptions_by_channel);
    old_advertised_channels = std::move(client.advertised_channels);
    was_subscribed_to_connection_graph = client.subscribed_to_connection_graph;
    _server.get_alog().write(APP, client_name + " was logged out");
    _clients.erase(client_iter);
  }

  // Unadvertise all channels this client advertised
  for (const auto client_channel_id : old_advertised_channels) {
    _server.get_alog().write(
      APP, "Client " + client_name + " unadvertising channel " +
      std::to_string(client_channel_id) + " due to disconnect");
    if (_handlers.client_unadvertise_handler) {
      try {
        _handlers.client_unadvertise_handler(client_channel_id, hdl);
      } catch (const std::exception & ex) {
        _server.get_elog().write(
          RECOVERABLE, "Exception caught when closing connection: " + std::string(ex.what()));
      } catch (...) {
        _server.get_elog().write(RECOVERABLE, "Exception caught when closing connection");
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(_client_channels_mutex);
    _client_channels.erase(hdl);
  }

  // Unsubscribe all channels this client subscribed to
  if (_handlers.unsubscribe_handler) {
    for (const auto & subs : old_subscriptions_by_channel) {
      try {
        _handlers.unsubscribe_handler(subs.first, hdl);
      } catch (const std::exception & ex) {
        _server.get_elog().write(
          RECOVERABLE, "Exception caught when closing connection: " + std::string(ex.what()));
      } catch (...) {
        _server.get_elog().write(RECOVERABLE, "Exception caught when closing connection");
      }
    }
  }

  // Unsubscribe from parameters this client subscribed to
  std::unordered_set<std::string> client_subscribed_parameters;
  {
    std::lock_guard<std::mutex> lock(_client_param_subscriptions_mutex);
    client_subscribed_parameters = _client_param_subscriptions[hdl];
    _client_param_subscriptions.erase(hdl);
  }
  this->unsubscribe_params_without_subscriptions(hdl, client_subscribed_parameters);

  if (was_subscribed_to_connection_graph) {
    std::lock_guard<std::mutex> lock(_connection_graph_mutex);
    _connection_graph.subscription_count--;
    if (_connection_graph.subscription_count == 0 && _handlers.subscribe_connection_graph_handler) {
      _server.get_alog().write(APP, "Unsubscribing from connection graph updates.");
      try {
        _handlers.subscribe_connection_graph_handler(false);
      } catch (const std::exception & ex) {
        _server.get_elog().write(
          RECOVERABLE, "Exception caught when closing connection: " + std::string(ex.what()));
      } catch (...) {
        _server.get_elog().write(
          RECOVERABLE, "Exception caught when closing connection");
      }
    }
  }
  _server.get_alog().write(APP, "Client " + client_name + " disconnected");
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::unsubscribe_params_without_subscriptions(
  ConnHandle hdl, const std::unordered_set<std::string> & param_names)
{
  std::vector<std::string> params_to_unsubscribe;
  {
    std::lock_guard<std::mutex> lock(_client_param_subscriptions_mutex);
    std::copy_if(
      param_names.begin(), param_names.end(), std::back_inserter(params_to_unsubscribe),
      [this](const std::string & param_name) {
        return !is_parameter_subscribed(param_name);
      });
  }

  if (_handlers.parameter_subscription_handler && !params_to_unsubscribe.empty()) {
    for (const auto & param : params_to_unsubscribe) {
      _server.get_alog().write(APP, "Unsubscribing from parameter '" + param + "'.");
    }

    try {
      _handlers.parameter_subscription_handler(
        params_to_unsubscribe,
        ParameterSubscriptionOperation::UNSUBSCRIBE, hdl);
    } catch (const std::exception & e) {
      send_status_and_log_msg(hdl, StatusLevel::Error, e.what());
    } catch (...) {
      send_status_and_log_msg(
        hdl, StatusLevel::Error,
        "Failed to unsubscribe from one more more parameters");
    }
  }
}

template<typename ServerConfiguration>
inline bool Server<ServerConfiguration>::is_parameter_subscribed(
  const std::string & param_name) const
{
  return std::find_if(
    _client_param_subscriptions.begin(), _client_param_subscriptions.end(),
    [param_name](const std::pair<ConnHandle,
    std::unordered_set<std::string>> & param_subscriptions) {
      return param_subscriptions.second.find(param_name) !=
      param_subscriptions.second.end();
    }) != _client_param_subscriptions.end();
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::handle_message(ConnHandle hdl, MessagePtr msg)
{
  const OpCode op = msg->get_opcode();
  try {
    if (op == OpCode::TEXT) {
      handle_text_message(hdl, msg);
    } else if (op == OpCode::BINARY) {
      handle_binary_message(hdl, msg);
    }
  } catch (const std::exception & e) {
    send_status_and_log_msg(hdl, StatusLevel::Error, e.what());
  } catch (...) {
    send_status_and_log_msg(
      hdl, StatusLevel::Error,
      "Exception occurred when executing message handler");
  }
}

template<typename ServerConfiguration>
inline bool Server<ServerConfiguration>::has_capability(const std::string & capability) const
{
  return std::find(
    _options.capabilities.begin(), _options.capabilities.end(), capability
  ) != _options.capabilities.end();
}

template<typename ServerConfiguration>
inline bool Server<ServerConfiguration>::has_handler(uint32_t op) const
{
  switch (op) {
    case LOGIN:
      // `login` must be the first request after websocket connected,
      // so, here return true forever
      return true;
    case SUBSCRIBE:
      return static_cast<bool>(_handlers.subscribe_handler);
    case UNSUBSCRIBE:
      return static_cast<bool>(_handlers.unsubscribe_handler);
    case ADVERTISE:
      return static_cast<bool>(_handlers.client_advertise_handler);
    case UNADVERTISE:
      return static_cast<bool>(_handlers.client_unadvertise_handler);
    case GET_PARAMETERS:
      return static_cast<bool>(_handlers.parameter_request_handler);
    case SET_PARAMETERS:
      return static_cast<bool>(_handlers.parameter_change_handler);
    case SUBSCRIBE_PARAMETER_UPDATES:
    case UNSUBSCRIBE_PARAMETER_UPDATES:
      return static_cast<bool>(_handlers.parameter_subscription_handler);
    case SUBSCRIBE_CONNECTION_GRAPH:
    case UNSUBSCRIBE_CONNECTION_GRAPH:
      return static_cast<bool>(_handlers.subscribe_connection_graph_handler);
    case FETCH_ASSET:
      return static_cast<bool>(_handlers.fetch_asset_handler);
    default:
      throw std::runtime_error("Unknown operation: " + std::to_string(op));
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_subscribe(const Json & payload, ConnHandle hdl)
{
  std::unordered_map<ChannelId, SubscriptionId> client_subscriptions_by_channel;
  {
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    client_subscriptions_by_channel = _clients.at(hdl).subscriptions_by_channel;
  }

  const auto find_subscription_by_sub_id =
    [](const std::unordered_map<ChannelId, SubscriptionId> & subscriptions_by_channel,
      SubscriptionId sub_id) {
      return std::find_if(
        subscriptions_by_channel.begin(), subscriptions_by_channel.end(),
        [&sub_id](const std::pair<ChannelId, SubscriptionId> & mo) {return mo.second == sub_id;});
    };

  for (const auto & sub : payload.at("subscriptions")) {
    SubscriptionId sub_id = sub.at("id");
    ChannelId channel_id = sub.at("channelId");
    if (find_subscription_by_sub_id(client_subscriptions_by_channel, sub_id) !=
      client_subscriptions_by_channel.end())
    {
      send_status_and_log_msg(
        hdl, StatusLevel::Error, "Client subscription id " +
        std::to_string(sub_id) + " was already used; ignoring subscription");
      continue;
    }
    const auto & channel_iter = _channels.find(channel_id);
    if (channel_iter == _channels.end()) {
      send_status_and_log_msg(
        hdl, StatusLevel::Warning, "Channel " + std::to_string(channel_id) +
        " is not available; ignoring subscription");
      continue;
    }

    _handlers.subscribe_handler(channel_id, hdl);
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    _clients.at(hdl).subscriptions_by_channel.emplace(channel_id, sub_id);
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_unsubscribe(const Json & payload, ConnHandle hdl)
{
  std::unordered_map<ChannelId, SubscriptionId> client_subscriptions_by_channel;
  {
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    client_subscriptions_by_channel = _clients.at(hdl).subscriptions_by_channel;
  }

  const auto find_subscription_by_sub_id =
    [](const std::unordered_map<ChannelId, SubscriptionId>
      & subscriptions_by_channel,
      SubscriptionId sub_id) {
      return std::find_if(
        subscriptions_by_channel.begin(), subscriptions_by_channel.end(),
        [&sub_id](const std::pair<ChannelId, SubscriptionId> & mo) {return mo.second == sub_id;});
    };

  for (const auto & sub_id_json : payload.at("subscriptionIds")) {
    SubscriptionId sub_id = sub_id_json;
    const auto & sub = find_subscription_by_sub_id(client_subscriptions_by_channel, sub_id);
    if (sub == client_subscriptions_by_channel.end()) {
      send_status_and_log_msg(
        hdl, StatusLevel::Warning, "Client subscription id " +
        std::to_string(sub_id) + " did not exist; ignoring unsubscription");
      continue;
    }

    ChannelId chan_id = sub->first;
    _handlers.unsubscribe_handler(chan_id, hdl);
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    _clients.at(hdl).subscriptions_by_channel.erase(chan_id);
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_advertise(const Json & payload, ConnHandle hdl)
{
  std::lock_guard<std::mutex> client_channels_lock(_client_channels_mutex);
  auto client_channel =
    _client_channels.emplace(hdl, std::unordered_map<ClientChannelId, ClientAdvertisement>());

  auto client_publications_iter = client_channel.first;
  auto is_first_publication = client_channel.second;

  auto & client_publications = client_publications_iter->second;

  for (const auto & chan : payload.at("channels")) {
    ClientChannelId channel_id = chan.at("id");
    if (!is_first_publication &&
      client_publications.find(channel_id) != client_publications.end())
    {
      send_status_and_log_msg(
        hdl, StatusLevel::Error,
        "Channel " + std::to_string(channel_id) + " was already advertised");
      continue;
    }

    const auto topic = chan.at("topic").get<std::string>();
    if (!is_whitelisted(topic, _options.client_topic_whitelist_patterns)) {
      send_status_and_log_msg(
        hdl, StatusLevel::Error, "Can't advertise channel " +
        std::to_string(channel_id) + ", topic '" + topic + "' not whitelisted");
      continue;
    }
    ClientAdvertisement advertisement{
      channel_id,
      topic,
      chan.at("encoding").get<std::string>(),
      chan.at("schemaName").get<std::string>(),
      std::vector<unsigned char>()
    };

    _handlers.client_advertise_handler(advertisement, hdl);
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    _clients.at(hdl).advertised_channels.emplace(channel_id);
    client_publications.emplace(channel_id, advertisement);
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_unadvertise(const Json & payload, ConnHandle hdl)
{
  std::lock_guard<std::mutex> clientChannelsLock(_client_channels_mutex);
  auto client_publications_iter = _client_channels.find(hdl);
  if (client_publications_iter == _client_channels.end()) {
    send_status_and_log_msg(
      hdl, StatusLevel::Error, "Client has no advertised channels");
    return;
  }

  auto & client_publications = client_publications_iter->second;
  for (const auto & chan_id_json : payload.at("channelIds")) {
    auto channel_id = chan_id_json.get<ClientChannelId>();
    const auto & channel_iter = client_publications.find(channel_id);
    if (channel_iter == client_publications.end()) {
      continue;
    }

    _handlers.client_unadvertise_handler(channel_id, hdl);
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    auto & client_info = _clients.at(hdl);
    client_publications.erase(channel_iter);
    const auto advertised_channel_iter = client_info.advertised_channels.find(channel_id);
    if (advertised_channel_iter != client_info.advertised_channels.end()) {
      client_info.advertised_channels.erase(advertised_channel_iter);
    }
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_get_parameters(const Json & payload, ConnHandle hdl)
{
  const auto param_names = payload.at("parameterNames").get<std::vector<std::string>>();
  const auto request_id = payload.find("id") == payload.end() ?
    optional<std::string>(nullopt) : optional<std::string>(payload["id"].get<std::string>());
  _handlers.parameter_request_handler(param_names, request_id, std::move(hdl));
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_set_parameters(const Json & payload, ConnHandle hdl)
{
  const auto parameters = payload.at("parameters").get<std::vector<Parameter>>();
  const auto request_id = payload.find("id") == payload.end() ?
    optional<std::string>(nullopt) : optional<std::string>(payload["id"].get<std::string>());
  _handlers.parameter_change_handler(parameters, request_id, std::move(hdl));
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_subscribe_parameter_updates(
  const Json & payload, ConnHandle hdl)
{
  const auto param_names =
    payload.at("parameterNames").get<std::unordered_set<std::string>>();
  std::vector<std::string> params_to_subscribe;
  {
    // Only consider parameters that are not subscribed yet (by this or by other
    // clients)
    std::lock_guard<std::mutex> lock(_client_param_subscriptions_mutex);
    std::copy_if(
      param_names.begin(), param_names.end(), std::back_inserter(params_to_subscribe),
      [this](const std::string & paramName) {
        return !is_parameter_subscribed(paramName);
      });

    // Update the client's parameter subscriptions.
    auto & clientSubscribedParams = _client_param_subscriptions[hdl];
    clientSubscribedParams.insert(param_names.begin(), param_names.end());
  }

  if (!params_to_subscribe.empty()) {
    _handlers.parameter_subscription_handler(
      params_to_subscribe, ParameterSubscriptionOperation::SUBSCRIBE, hdl);
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_unsubscribe_parameter_updates(
  const Json & payload, ConnHandle hdl)
{
  const auto param_names = payload.at("parameterNames").get<std::unordered_set<std::string>>();
  {
    std::lock_guard<std::mutex> lock(_client_param_subscriptions_mutex);
    auto & client_subscribed_params = _client_param_subscriptions[hdl];
    for (const auto & param_name : param_names) {
      client_subscribed_params.erase(param_name);
    }
  }

  unsubscribe_params_without_subscriptions(hdl, param_names);
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_subscribe_connection_graph(ConnHandle hdl)
{
  bool subscribe_to_connection_graph = false;
  {
    std::lock_guard<std::mutex> lock(_connection_graph_mutex);
    _connection_graph.subscription_count++;
    subscribe_to_connection_graph = _connection_graph.subscription_count == 1;
  }

  if (subscribe_to_connection_graph) {
    // First subscriber, let the handler know that we are interested in updates.
    _server.get_alog().write(APP, "Subscribing to connection graph updates.");
    _handlers.subscribe_connection_graph_handler(true);
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    _clients.at(hdl).subscribed_to_connection_graph = true;
  }

  Json::array_t published_topics_json, subscribed_topics_json, advertised_services_json;
  {
    std::lock_guard<std::mutex> lock(_connection_graph_mutex);
    for (const auto & topic : _connection_graph.published_topics) {
      published_topics_json.push_back(
        Json{{"name", topic.first}, {"publisherIds", topic.second}});
    }
    for (const auto & topic : _connection_graph.subscribed_topics) {
      subscribed_topics_json.push_back(
        Json{{"name", topic.first}, {"subscriberIds", topic.second}});
    }
    for (const auto & service : _connection_graph.advertised_services) {
      advertised_services_json.push_back(
        Json{{"name", service.first}, {"providerIds", service.second}});
    }
  }

  const Json jsonMsg = {
    {"op", "connectionGraphUpdate"},
    {"publishedTopics", published_topics_json},
    {"subscribedTopics", subscribed_topics_json},
    {"advertisedServices", advertised_services_json},
    {"removedTopics", Json::array()},
    {"removedServices", Json::array()},
  };

  send_raw_json(hdl, jsonMsg.dump());
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_unsubscribe_connection_graph(ConnHandle hdl)
{
  bool client_was_subscribed = false;
  {
    std::lock_guard<std::mutex> clients_lock(_clients_mutex);
    auto & client_info = _clients.at(hdl);
    if (client_info.subscribed_to_connection_graph) {
      client_was_subscribed = true;
      client_info.subscribed_to_connection_graph = false;
    }
  }

  if (client_was_subscribed) {
    bool unsubscribe_from_connection_graph = false;
    {
      std::lock_guard<std::mutex> lock(_connection_graph_mutex);
      _connection_graph.subscription_count--;
      unsubscribe_from_connection_graph = _connection_graph.subscription_count == 0;
    }
    if (unsubscribe_from_connection_graph) {
      _server.get_alog().write(APP, "Unsubscribing from connection graph updates.");
      _handlers.subscribe_connection_graph_handler(false);
    }
  } else {
    send_status_and_log_msg(
      hdl, StatusLevel::Error,
      "Client was not subscribed to connection graph updates");
  }
}

template<typename ServerConfiguration>
void Server<ServerConfiguration>::handle_fetch_asset(const Json & payload, ConnHandle hdl)
{
  const auto uri = payload.at("uri").get<std::string>();
  const auto request_id = payload.at("requestId").get<uint32_t>();
  _handlers.fetch_asset_handler(uri, request_id, hdl);
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::handle_text_message(ConnHandle hdl, MessagePtr msg)
{
  const Json payload = Json::parse(msg->get_payload());
  const std::string & op = payload.at("op").get<std::string>();

  const auto required_capability_iter = CAPABILITY_BY_CLIENT_OPERATION.find(op);
  if (required_capability_iter != CAPABILITY_BY_CLIENT_OPERATION.end() &&
    !has_capability(required_capability_iter->second))
  {
    send_status_and_log_msg(
      hdl, StatusLevel::Error,
      "Operation '" + op + "' not supported as server capability '" +
      required_capability_iter->second + "' is missing");
    return;
  }

  auto op_string_view = string_view(op);
  if (!has_handler(string_hash(op_string_view))) {
    send_status_and_log_msg(
      hdl, StatusLevel::Error,
      "Operation '" + op + "' not supported as server handler function is missing");
    return;
  }

  try {
    switch (string_hash(op_string_view)) {
      case LOGIN:
        handle_login(payload, hdl);
        break;
      case SUBSCRIBE:
        handle_subscribe(payload, hdl);
        break;
      case UNSUBSCRIBE:
        handle_unsubscribe(payload, hdl);
        break;
      case ADVERTISE:
        handle_advertise(payload, hdl);
        break;
      case UNADVERTISE:
        handle_unadvertise(payload, hdl);
        break;
      case GET_PARAMETERS:
        handle_get_parameters(payload, hdl);
        break;
      case SET_PARAMETERS:
        handle_set_parameters(payload, hdl);
        break;
      case SUBSCRIBE_PARAMETER_UPDATES:
        handle_subscribe_parameter_updates(payload, hdl);
        break;
      case UNSUBSCRIBE_PARAMETER_UPDATES:
        handle_unsubscribe_parameter_updates(payload, hdl);
        break;
      case SUBSCRIBE_CONNECTION_GRAPH:
        handle_subscribe_connection_graph(hdl);
        break;
      case UNSUBSCRIBE_CONNECTION_GRAPH:
        handle_unsubscribe_connection_graph(hdl);
        break;
      case FETCH_ASSET:
        handle_fetch_asset(payload, hdl);
        break;
      default:
        send_status_and_log_msg(
          hdl, StatusLevel::Error, "Unrecognized client opcode \"" + op + "\"");
        break;
    }
  } catch (const ExceptionWithId<uint32_t> & e) {
    const std::string postfix = " (op: " + op + ", id: " + std::to_string(e.id()) + ")";
    send_status_and_log_msg(hdl, StatusLevel::Error, e.what() + postfix);
  } catch (const std::exception & e) {
    const std::string postfix = " (op: " + op + ")";
    send_status_and_log_msg(hdl, StatusLevel::Error, e.what() + postfix);
  } catch (...) {
    const std::string postfix = " (op: " + op + ")";
    send_status_and_log_msg(hdl, StatusLevel::Error, "Failed to execute handler" + postfix);
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::handle_binary_message(ConnHandle hdl, MessagePtr msg)
{
  const auto & payload = msg->get_payload();
  const auto * data = reinterpret_cast<const uint8_t *>(payload.data());
  const size_t length = payload.size();

  if (length < 1) {
    send_status_and_log_msg(hdl, StatusLevel::Error, "Received an empty binary message");
    return;
  }

  const auto op = static_cast<ClientBinaryOpcode>(data[0]);

  const auto required_capability_iter = CAPABILITY_BY_CLIENT_BINARY_OPERATION.find(op);
  if (required_capability_iter != CAPABILITY_BY_CLIENT_BINARY_OPERATION.end() &&
    !has_capability(required_capability_iter->second))
  {
    send_status_and_log_msg(
      hdl, StatusLevel::Error,
      "Binary operation '" + std::to_string(static_cast<int>(op)) +
      "' not supported as server capability '" + required_capability_iter->second +
      "' is missing");
    return;
  }

  switch (op) {
    case ClientBinaryOpcode::MESSAGE_DATA: {
        if (!_handlers.client_message_handler) {
          return;
        }

        if (length < 5) {
          send_status_and_log_msg(
            hdl, StatusLevel::Error,
            "Invalid message length " + std::to_string(length));
          return;
        }
        const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
        const ClientChannelId channel_id = *reinterpret_cast<const ClientChannelId *>(data + 1);
        std::lock_guard<std::mutex> lock(_client_channels_mutex);

        auto client_publications_iter = _client_channels.find(hdl);
        if (client_publications_iter == _client_channels.end()) {
          send_status_and_log_msg(hdl, StatusLevel::Error, "Client has no advertised channels");
          return;
        }

        auto & client_publications = client_publications_iter->second;
        const auto & channel_iter = client_publications.find(channel_id);
        if (channel_iter == client_publications.end()) {
          send_status_and_log_msg(
            hdl, StatusLevel::Error,
            "Channel " + std::to_string(channel_id) + " is not advertised");
          return;
        }

        try {
          const auto & advertisement = channel_iter->second;
          const uint32_t sequence = 0;
          const ClientMessage client_message{static_cast<uint64_t>(timestamp),
            static_cast<uint64_t>(timestamp),
            sequence,
            advertisement,
            length,
            data};
          _handlers.client_message_handler(client_message, hdl);
        } catch (const ServiceError & e) {
          send_status_and_log_msg(hdl, StatusLevel::Error, e.what());
        } catch (...) {
          send_status_and_log_msg(
            hdl, StatusLevel::Error,
            "callService: Failed to execute handler");
        }
      }
      break;
    case ClientBinaryOpcode::SERVICE_CALL_REQUEST: {
        ServiceRequest request;
        if (length < request.size()) {
          send_status_and_log_msg(
            hdl, StatusLevel::Error,
            "Invalid service call request length " + std::to_string(length));
          return;
        }

        request.read(data + 1, length - 1);

        {
          std::lock_guard<std::mutex> lock(_services_mutex);
          if (_services.find(request.service_id) == _services.end()) {
            send_status_and_log_msg(
              hdl, StatusLevel::Error,
              "Service " + std::to_string(request.service_id) + " is not advertised");
            return;
          }
        }

        if (_handlers.service_request_handler) {
          _handlers.service_request_handler(request, hdl);
        }
      }
      break;
    default: {
        send_status_and_log_msg(
          hdl, StatusLevel::Error,
          "Unrecognized client opcode " + std::to_string(uint8_t(op)));
      }
      break;
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_status_and_log_msg(
  ConnHandle client_handle,
  const StatusLevel level,
  const std::string & message)
{
  const std::string endpoint = remote_endpoint_string(client_handle);
  const std::string logMessage = endpoint + ": " + message;
  const auto logLevel = status_level_to_log_level(level);
  auto logger = level == StatusLevel::Info ? _server.get_alog() : _server.get_elog();
  logger.write(logLevel, logMessage);

  send_json(
    client_handle, Json{
      {"op", "status"},
      {"level", static_cast<uint8_t>(level)},
      {"message", message},
    });
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_json(ConnHandle hdl, Json && payload, bool reliable)
{
  try {
    const auto con = _server.get_con_from_hdl(hdl);
    con->send(std::move(payload).dump(), reliable, OpCode::text);
  } catch (std::exception const & e) {
    _server.get_elog().write(RECOVERABLE, e.what());
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_raw_json(
  ConnHandle hdl, const std::string & payload,
  bool reliable)
{
  try {
    const auto con = _server.get_con_from_hdl(hdl);
    con->send(payload, reliable, OpCode::TEXT);
  } catch (std::exception const & e) {
    _server.get_elog().write(RECOVERABLE, e.what());
  }
}

template<typename ServerConfiguration>
inline void Server<ServerConfiguration>::send_binary(
  ConnHandle hdl, const uint8_t * payload,
  size_t payload_size, bool reliable)
{
  try {
    const auto con = _server.get_con_from_hdl(hdl);
    con->send(payload, payload_size, reliable, OpCode::BINARY);
  } catch (std::exception const & e) {
    _server.get_elog().write(RECOVERABLE, e.what());
  }
}
}  // namespace cobridge_base

#endif  // WEBSOCKET_SERVER_HPP_
