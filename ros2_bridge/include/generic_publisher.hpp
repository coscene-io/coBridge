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

#ifndef GENERIC_PUBLISHER_HPP_
#define GENERIC_PUBLISHER_HPP_

#ifdef ROS2_VERSION_FOXY
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

namespace cobridge
{
class GenericPublisher : public rclcpp::PublisherBase
{
public:
  using SharedPtr = std::shared_ptr<GenericPublisher>;

  GenericPublisher(
    rclcpp::node_interfaces::NodeBaseInterface * node_base,
    const rosidl_message_type_support_t & type_support,
    const std::string & topic_name,
    const rclcpp::QoS & qos);

  RCLCPP_PUBLIC
  ~GenericPublisher() override = default;

  RCLCPP_PUBLIC
  void publish(const std::shared_ptr<rcl_serialized_message_t> & message);
};

}  // namespace cobridge
#endif

#endif  // GENERIC_PUBLISHER_HPP_
