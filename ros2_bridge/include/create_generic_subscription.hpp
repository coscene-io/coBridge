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

#ifndef ROS2_WS_CREATE_GENERIC_SUBSCRIPTION_HPP
#define ROS2_WS_CREATE_GENERIC_SUBSCRIPTION_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "rcl/subscription.h"
#include "rclcpp/node_interfaces/node_topics_interface.hpp"
#include "rclcpp/qos.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/subscription_options.hpp"

#include "generic_subscription.hpp"
#include "typesupport_helpers.hpp"

namespace cobridge {
    std::shared_ptr<GenericSubscription> create_generic_subscription(
            rclcpp::node_interfaces::NodeTopicsInterface::SharedPtr topics_interface,
            const std::string &topic,
            const std::string &type,
            const rclcpp::QoS &qos,
            std::function<void(std::shared_ptr<rclcpp::SerializedMessage>, uint64_t timestamp)> callback) {
        auto library_generic_subscriber = cobridge::get_typesupport_library(
                type, "rosidl_typesupport_cpp");
        auto type_support = cobridge::get_typesupport_handle(
                type, "rosidl_typesupport_cpp", library_generic_subscriber);
        auto subscription = std::shared_ptr<GenericSubscription>();

        try {
            subscription = std::make_shared<GenericSubscription>(
                    topics_interface->get_node_base_interface(),
                    *type_support,
                    topic,
                    type,
                    qos,
                    callback);

            topics_interface->add_subscription(subscription, nullptr);
        } catch (const std::runtime_error &ex) {
            std::runtime_error("Error subscribing to topic '" + topic + "'. Error: " + ex.what());
        }

        return subscription;
    }
}

#endif //ROS2_WS_CREATE_GENERIC_SUBSCRIPTION_HPP
