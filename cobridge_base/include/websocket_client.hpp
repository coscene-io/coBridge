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

#ifndef WEBSOCKET_CLIENT_HPP_
#define WEBSOCKET_CLIENT_HPP_

// #include <optional>
// #include <shared_mutex>

#include <json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>

#include <string>
#include <memory>
#include <functional>
#include <future>
#include <utility>
#include <vector>

#include "common.hpp"
#include "parameter.hpp"
#include "serialization.hpp"

namespace cobridge_base
{

inline void to_json(nlohmann::json & j, const ClientAdvertisement & p)
{
  j = nlohmann::json{
    {"id", p.channel_id},
    {"topic", p.topic},
    {"encoding", p.encoding},
    {"schemaName", p.schema_name}
  };
}

using TextMessageHandler = std::function<void (const std::string &)>;
using BinaryMessageHandler = std::function<void (const uint8_t *, size_t)>;
using OpCode = websocketpp::frame::opcode::value;

class ClientInterface
{
public:
  virtual void connect(
    const std::string & uri, std::function<void(websocketpp::connection_hdl)> on_open_handler,
    std::function<void(websocketpp::connection_hdl)> on_close_handler = nullptr) = 0;

  virtual std::future<void> connect(const std::string & uri) = 0;

  virtual void close() = 0;

  virtual void login(std::string user_name, std::string user_id) = 0;

  virtual void subscribe(
    const std::vector<std::pair<SubscriptionId, ChannelId>> & subscriptions) = 0;

  virtual void unsubscribe(const std::vector<SubscriptionId> & subscription_ids) = 0;

  virtual void advertise(const std::vector<ClientAdvertisement> & channels) = 0;

  virtual void unadvertise(const std::vector<ClientChannelId> & channel_ids) = 0;

  virtual void publish(ClientChannelId channel_id, const uint8_t * buffer, size_t size) = 0;

  virtual void send_service_request(const ServiceRequest & request) = 0;

  virtual void get_parameters(
    const std::vector<std::string> & parameter_names,
    const optional<std::string> & request_id) = 0;

  virtual void set_parameters(
    const std::vector<Parameter> & parameters,
    const optional<std::string> & request_id) = 0;

  virtual void subscribe_parameter_updates(const std::vector<std::string> & parameter_names) = 0;

  virtual void unsubscribe_parameter_updates(const std::vector<std::string> & parameter_names) = 0;

  virtual void fetch_asset(const std::string & name, uint32_t request_id) = 0;

  virtual void set_text_message_handler(TextMessageHandler handler) = 0;

  virtual void set_binary_message_handler(BinaryMessageHandler handler) = 0;
};

template<typename ClientConfiguration>
class Client : public ClientInterface
{
public:
  using ClientType = websocketpp::client<ClientConfiguration>;
  using MessagePtr = typename ClientType::message_ptr;
  using ConnectionPtr = typename ClientType::connection_ptr;

  Client()
  {
    _endpoint.clear_access_channels(websocketpp::log::alevel::all);
    _endpoint.clear_error_channels(websocketpp::log::elevel::all);

    _endpoint.init_asio();
    _endpoint.start_perpetual();

    _endpoint.set_message_handler(
      bind(&Client::message_handler, this, std::placeholders::_1, std::placeholders::_2));

    _thread.reset(new websocketpp::lib::thread(&ClientType::run, &_endpoint));
  }

  virtual ~Client()
  {
    close();

    _con.reset();
    _endpoint.stop_perpetual();
    _thread->join();
  }

  void connect(
    const std::string & uri,
    std::function<void(websocketpp::connection_hdl)> on_open_handler,
    std::function<void(websocketpp::connection_hdl)> on_close_handler = nullptr) override
  {
    std::lock_guard<std::mutex> lock(_mutex);

    websocketpp::lib::error_code ec;
    _con = _endpoint.get_connection(uri, ec);

    if (ec) {
      throw std::runtime_error("Failed to get connection from URI " + uri);
    }

    if (on_open_handler) {
      _con->set_open_handler(on_open_handler);
    }
    if (on_close_handler) {
      _con->set_close_handler(on_close_handler);
    }

    _con->add_subprotocol(SUPPORTED_SUB_PROTOCOL);
    _endpoint.connect(_con);
  }

  std::future<void> connect(const std::string & uri) override
  {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    connect(
      uri, [promise](websocketpp::connection_hdl) mutable
      {
        promise->set_value();
      });

    return future;
  }

  void close() override
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_con->get_state() != websocketpp::session::state::open) {
      return;  // Already disconnected
    }
    _endpoint.close(_con, websocketpp::close::status::going_away, "");
  }

  void message_handler(websocketpp::connection_hdl hdl, MessagePtr msg)
  {
    (void) hdl;
    const OpCode op = msg->get_opcode();

    switch (op) {
      case OpCode::TEXT: {
          std::lock_guard<std::mutex> lock(_mutex);
          if (_text_message_handler) {
            _text_message_handler(msg->get_payload());
          }
        }
        break;
      case OpCode::BINARY: {
          std::lock_guard<std::mutex> lock(_mutex);
          const auto & payload = msg->get_payload();
          if (_binary_message_handler) {
            _binary_message_handler(
              reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
          }
        }
        break;
      default: break;
    }
  }

  void login(std::string user_name, std::string user_id) override
  {
    const std::string payload =
      nlohmann::json{
      {"op", "login"},
      {"username", user_name},
      {"userId", user_id}}.dump();
    send_text(payload);
  }

  void subscribe(const std::vector<std::pair<SubscriptionId, ChannelId>> & subscriptions) override
  {
    nlohmann::json sub_json;
    for (const auto & subscription : subscriptions) {
      sub_json.push_back(
        {{"id", subscription.first},
          {"channelId", subscription.second}});
    }

    const std::string payload =
      nlohmann::json{
      {"op", "subscribe"},
      {"subscriptions", std::move(sub_json)}}.dump();
    send_text(payload);
  }

  void unsubscribe(const std::vector<SubscriptionId> & subscription_ids) override
  {
    const std::string payload =
      nlohmann::json{
      {"op", "unsubscribe"},
      {"subscriptionIds", subscription_ids}}.dump();
    send_text(payload);
  }

  void advertise(const std::vector<ClientAdvertisement> & channels) override
  {
    const std::string payload = nlohmann::json{
      {"op", "advertise"},
      {"channels", channels}}.dump();
    send_text(payload);
  }

  void unadvertise(const std::vector<ClientChannelId> & channel_ids) override
  {
    const std::string payload =
      nlohmann::json{
      {"op", "unadvertise"},
      {"channelIds", channel_ids}}.dump();
    send_text(payload);
  }

  void publish(ClientChannelId channel_id, const uint8_t * buffer, size_t size) override
  {
    std::vector<uint8_t> payload(1 + 4 + size);
    payload[0] = uint8_t(ClientBinaryOpcode::MESSAGE_DATA);
    write_uint32_LE(payload.data() + 1, channel_id);
    std::memcpy(payload.data() + 1 + 4, buffer, size);
    send_binary(payload.data(), payload.size());
  }

  void send_service_request(const ServiceRequest & request) override
  {
    std::vector<uint8_t> payload(1 + request.size());
    payload[0] = uint8_t(ClientBinaryOpcode::SERVICE_CALL_REQUEST);
    request.write(payload.data() + 1);
    send_binary(payload.data(), payload.size());
  }

  void get_parameters(
    const std::vector<std::string> & parameter_names,
    const optional<std::string> & request_id = optional<std::string>(nullopt)) override
  {
    nlohmann::json jsonPayload{{"op", "getParameters"},
      {"parameterNames", parameter_names}};
    if (request_id.has_value()) {
      jsonPayload["id"] = request_id.value();
    }
    send_text(jsonPayload.dump());
  }

  void set_parameters(
    const std::vector<Parameter> & parameters,
    const optional<std::string> & request_id = optional<std::string>(nullopt)) override
  {
    nlohmann::json jsonPayload{{"op", "setParameters"},
      {"parameters", parameters}};
    if (request_id.has_value()) {
      jsonPayload["id"] = request_id.value();
    }
    send_text(jsonPayload.dump());
  }

  void subscribe_parameter_updates(const std::vector<std::string> & parameter_names) override
  {
    nlohmann::json jsonPayload{{"op", "subscribeParameterUpdates"},
      {"parameterNames", parameter_names}};
    send_text(jsonPayload.dump());
  }

  void unsubscribe_parameter_updates(const std::vector<std::string> & parameter_names) override
  {
    nlohmann::json jsonPayload{{"op", "unsubscribeParameterUpdates"},
      {"parameterNames", parameter_names}};
    send_text(jsonPayload.dump());
  }

  void fetch_asset(const std::string & uri, uint32_t request_id) override
  {
    nlohmann::json jsonPayload{{"op", "fetchAsset"},
      {"uri", uri},
      {"requestId", request_id}};
    send_text(jsonPayload.dump());
  }

  void set_text_message_handler(TextMessageHandler handler) override
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _text_message_handler = std::move(handler);
  }

  void set_binary_message_handler(BinaryMessageHandler handler) override
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _binary_message_handler = std::move(handler);
  }

  void send_text(const std::string & payload)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _endpoint.send(_con, payload, OpCode::TEXT);
  }

  void send_binary(const uint8_t * data, size_t dataLength)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _endpoint.send(_con, data, dataLength, OpCode::BINARY);
  }

protected:
  ClientType _endpoint;
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> _thread;
  ConnectionPtr _con;
  std::mutex _mutex;
  TextMessageHandler _text_message_handler;
  BinaryMessageHandler _binary_message_handler;
};

}  // namespace cobridge_base
#endif  // WEBSOCKET_CLIENT_HPP_
