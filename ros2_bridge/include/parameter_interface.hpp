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

#include <chrono>
#include <functional>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/parameter_map.hpp>

#include <parameter.hpp>

namespace cos_bridge {

    using ParameterList = std::vector<cos_bridge_base::Parameter>;
    using ParamUpdateFunc = std::function<void(const ParameterList &)>;

    class ParameterInterface {
    public:
        ParameterInterface(rclcpp::Node *node, std::vector<std::regex> param_whitelist_patterns);

        ParameterList get_params(const std::vector<std::string> &param_names,
                                 const std::chrono::duration<double> &timeout);

        void set_params(const ParameterList &parameters, const std::chrono::duration<double> &timeout);

        void subscribe_params(const std::vector<std::string> &param_names);

        void unsubscribe_params(const std::vector<std::string> &param_names);

        void set_param_update_callback(ParamUpdateFunc param_update_func);

    private:
        rclcpp::Node *_node;
        std::vector<std::regex> _param_whitelist_patterns;
        rclcpp::CallbackGroup::SharedPtr _callback_group;
        std::mutex _mutex;
        std::unordered_map<std::string, rclcpp::AsyncParametersClient::SharedPtr> _param_clients_by_node;
        std::unordered_map<std::string, std::unordered_set<std::string>> _subscribed_params_by_node;
        std::unordered_map<std::string, rclcpp::SubscriptionBase::SharedPtr> _param_subscriptions_by_node;
        ParamUpdateFunc _param_update_func;

        ParameterList get_node_parameters(const rclcpp::AsyncParametersClient::SharedPtr param_client,
                                          const std::string &node_name,
                                          const std::vector<std::string> &param_names,
                                          const std::chrono::duration<double> &timeout);

        void set_node_parameters(rclcpp::AsyncParametersClient::SharedPtr param_client,
                                 const std::string &node_name, const std::vector<rclcpp::Parameter> &params,
                                 const std::chrono::duration<double> &timeout);

        bool is_whitelisted_param(const std::string &param_name);
    };

}
