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


#ifndef TEST_CLIENT_HPP_
#define TEST_CLIENT_HPP_
#include <websocketpp/config/asio_client.hpp>

#include <future>
#include <string>
#include <vector>
#include <memory>

#include "../parameter.hpp"
#include "../websocket_client.hpp"
namespace cobridge_base
{
std::future<std::string> wait_for_kicked(std::shared_ptr<ClientInterface> client);

std::future<std::string> wait_for_login(
  std::shared_ptr<ClientInterface> client,
  std::string operate);

std::future<std::vector<uint8_t>> wait_for_channel_msg(
  ClientInterface * client,
  SubscriptionId subscription_id);

std::future<std::vector<Parameter>> wait_for_parameters(
  std::shared_ptr<ClientInterface> client,
  const std::string & request_id = std::string());

std::future<ServiceResponse> wait_for_service_response(std::shared_ptr<ClientInterface> client);

std::future<Service> wait_for_service(
  std::shared_ptr<ClientInterface> client,
  const std::string & service_name);

std::future<Channel> wait_for_channel(
  std::shared_ptr<ClientInterface> client,
  const std::string & topic_name);

std::future<FetchAssetResponse> wait_for_fetch_asset_response(
  std::shared_ptr<ClientInterface>
  client);

extern template class Client<websocketpp::config::asio_client>;

}  // namespace cobridge_base

#endif  // TEST_CLIENT_HPP_
