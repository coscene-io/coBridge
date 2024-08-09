//////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////


#pragma once

#include <future>
#include <string>
#include <vector>

#include <websocketpp/config/asio_client.hpp>

#include "../parameter.hpp"
#include "../websocket_client.hpp"

namespace cobridge {

    std::future<std::vector<uint8_t>> waitForChannelMsg(ClientInterface* client,
                                                        SubscriptionId subscriptionId);

    std::future<std::vector<Parameter>> waitForParameters(std::shared_ptr<ClientInterface> client,
                                                          const std::string& requestId = std::string());

    std::future<ServiceResponse> waitForServiceResponse(std::shared_ptr<ClientInterface> client);

    std::future<Service> waitForService(std::shared_ptr<ClientInterface> client,
                                        const std::string& serviceName);

    std::future<Channel> waitForChannel(std::shared_ptr<ClientInterface> client,
                                        const std::string& topicName);

    std::future<FetchAssetResponse> waitForFetchAssetResponse(std::shared_ptr<ClientInterface> client);

    extern template class Client<websocketpp::config::asio_client>;

}  // namespace cobridge