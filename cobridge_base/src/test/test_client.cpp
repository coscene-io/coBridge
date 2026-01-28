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

#include <websocketpp/config/asio_client.hpp>

#include <serialization.hpp>
#include <test/test_client.hpp>
#include <websocket_client.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cobridge_base
{

std::future<std::string> wait_for_kicked(std::shared_ptr<ClientInterface> client)
{
  auto promise = std::make_shared<std::promise<std::string>>();
  auto future = promise->get_future();

  client->set_text_message_handler(
    [promise](const std::string & payload) {
      const auto msg = nlohmann::json::parse(payload);
      const auto & op = msg["op"].get<std::string>();

      if (op == "kicked") {
        promise->set_value(msg.dump());
      }
    });

  return future;
}

std::future<std::string> wait_for_login(
  std::shared_ptr<ClientInterface> client,
  std::string operate)
{
  auto promise = std::make_shared<std::promise<std::string>>();
  auto future = promise->get_future();

  client->set_text_message_handler(
    [promise, operate](const std::string & payload) {
      const auto msg = nlohmann::json::parse(payload);
      std::string op = msg["op"].get<std::string>();

      if (op == operate) {
        promise->set_value(msg.dump());
      }
    });

  return future;
}

std::future<std::vector<uint8_t>> wait_for_channel_msg(
  ClientInterface * client,
  SubscriptionId subscription_id)
{
  auto promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
  auto future = promise->get_future();

  client->set_binary_message_handler(
    [promise, subscription_id](const uint8_t * data, size_t dataLength) {
      if (read_uint32_LE(data + 1) != subscription_id) {
        return;
      }
      const size_t offset = 1 + 4 + 8;
      std::vector<uint8_t> dataCopy(dataLength - offset);
      std::memcpy(dataCopy.data(), data + offset, dataLength - offset);
      promise->set_value(std::move(dataCopy));
    });

  return future;
}

std::future<std::vector<Parameter>> wait_for_parameters(
  std::shared_ptr<ClientInterface> client,
  const std::string & request_id)
{
  auto promise = std::make_shared<std::promise<std::vector<Parameter>>>();
  auto future = promise->get_future();

  client->set_text_message_handler(
    [promise, request_id](const std::string & payload) {
      const auto msg = nlohmann::json::parse(payload);
      const auto & op = msg["op"].get<std::string>();
      const auto id = msg.value("id", "");

      if (op == "parameterValues" && (request_id.empty() || request_id == id)) {
        const auto parameters = msg["parameters"].get<std::vector<Parameter>>();
        promise->set_value(std::move(parameters));
      }
    });

  return future;
}

std::future<ServiceResponse> wait_for_service_response(std::shared_ptr<ClientInterface> client)
{
  auto promise = std::make_shared<std::promise<ServiceResponse>>();
  auto future = promise->get_future();

  client->set_binary_message_handler(
    [promise](const uint8_t * data, size_t data_length) mutable {
      if (static_cast<BinaryOpcode>(data[0]) != BinaryOpcode::SERVICE_CALL_RESPONSE) {
        return;
      }

      cobridge_base::ServiceResponse response;
      response.read(data + 1, data_length - 1);
      promise->set_value(response);
    });
  return future;
}

std::future<Service> wait_for_service(
  std::shared_ptr<ClientInterface> client,
  const std::string & service_name)
{
  auto promise = std::make_shared<std::promise<Service>>();
  auto future = promise->get_future();

  client->set_text_message_handler(
    [promise, service_name](const std::string & payload) mutable {
      const auto msg = nlohmann::json::parse(payload);
      const auto & op = msg["op"].get<std::string>();

      if (op == "advertiseServices") {
        const auto services = msg["services"].get<std::vector<Service>>();
        for (const auto & service : services) {
          if (service.name == service_name) {
            promise->set_value(service);
            break;
          }
        }
      }
    });

  return future;
}

std::future<Channel> wait_for_channel(
  std::shared_ptr<ClientInterface> client,
  const std::string & topic_name)
{
  auto promise = std::make_shared<std::promise<Channel>>();
  auto future = promise->get_future();

  client->set_text_message_handler(
    [promise, topic_name](const std::string & payload) mutable {
      const auto msg = nlohmann::json::parse(payload);
      const auto & op = msg["op"].get<std::string>();

      if (op == "advertise") {
        const auto channels = msg["channels"].get<std::vector<Channel>>();
        for (const auto & channel : channels) {
          if (channel.topic == topic_name) {
            promise->set_value(channel);
            break;
          }
        }
      }
    });
  return future;
}

std::future<FetchAssetResponse> wait_for_fetch_asset_response(
  std::shared_ptr<ClientInterface>
  client)
{
  auto promise = std::make_shared<std::promise<FetchAssetResponse>>();
  auto future = promise->get_future();

  client->set_binary_message_handler(
    [promise](const uint8_t * data, size_t data_length) mutable {
      if (static_cast<BinaryOpcode>(data[0]) != BinaryOpcode::FETCH_ASSET_RESPONSE) {
        return;
      }

      cobridge_base::FetchAssetResponse response;
      size_t offset = 1;
      response.request_id = read_uint32_LE(data + offset);
      offset += 4;
      response.status = static_cast<cobridge_base::FetchAssetStatus>(data[offset]);
      offset += 1;
      const size_t errorMsgLength = static_cast<size_t>(read_uint32_LE(data + offset));
      offset += 4;
      response.error_message =
      std::string(reinterpret_cast<const char *>(data + offset), errorMsgLength);
      offset += errorMsgLength;
      const auto payloadLength = data_length - offset;
      response.data.resize(payloadLength);
      std::memcpy(response.data.data(), data + offset, payloadLength);
      promise->set_value(response);
    });
  return future;
}

template class Client<websocketpp::config::asio_client>;

}  // namespace cobridge_base
