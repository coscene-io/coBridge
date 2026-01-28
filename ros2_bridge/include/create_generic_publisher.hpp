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

#ifndef CREATE_GENERIC_PUBLISHER_HPP_
#define CREATE_GENERIC_PUBLISHER_HPP_

#ifdef ROS2_VERSION_FOXY
#include <memory>
#include <string>
#include <utility>

#include "rclcpp/node_interfaces/node_topics_interface.hpp"
#include "rclcpp/publisher_options.hpp"
#include "rclcpp/qos.hpp"

#include "generic_publisher.hpp"
#include "typesupport_helpers.hpp"

namespace cobridge
{
std::shared_ptr<GenericPublisher> create_generic_publisher(
  rclcpp::node_interfaces::NodeTopicsInterface::SharedPtr topics_interface,
  const std::string & topic, const std::string & type, const rclcpp::QoS & qos)
{
  auto library_generic_publisher = cobridge::get_typesupport_library(
    type, "rosidl_typesupport_cpp");
  auto type_support = cobridge::get_typesupport_handle(
    type, "rosidl_typesupport_cpp", library_generic_publisher);
  return std::make_shared<GenericPublisher>(
    topics_interface->get_node_base_interface(), *type_support, topic, qos);
}
}  // namespace cobridge
#endif

#endif  // CREATE_GENERIC_PUBLISHER_HPP_
