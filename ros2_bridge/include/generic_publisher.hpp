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

#ifndef ROS2_WS_GENERIC_PUBLISHER_HPP
#define ROS2_WS_GENERIC_PUBLISHER_HPP

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

namespace cos_bridge {
    class GenericPublisher : public rclcpp::PublisherBase {
    public:
        RCLCPP_SMART_PTR_DEFINITIONS(GenericPublisher)

        GenericPublisher(
                rclcpp::node_interfaces::NodeBaseInterface *node_base,
                const rosidl_message_type_support_t &type_support,
                const std::string &topic_name,
                const rclcpp::QoS &qos);

        RCLCPP_PUBLIC
        ~GenericPublisher() override = default;

        RCLCPP_PUBLIC
        void publish(const std::shared_ptr<rcl_serialized_message_t> &message);
    };

}

#endif //ROS2_WS_GENERIC_PUBLISHER_HPP
