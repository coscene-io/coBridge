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

#ifndef GENERIC_SUBSCRIPTION_HPP_
#define GENERIC_SUBSCRIPTION_HPP_

#ifdef ROS2_VERSION_FOXY
#include <memory>
#include <string>

#include "rclcpp/any_subscription_callback.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/serialization.hpp"
#include "rclcpp/subscription.hpp"

namespace cobridge
{

/**
 * This class is an implementation of an rclcpp::Subscription for serialized messages whose topic
 * is not known at compile time (hence templating does not work).
 *
 * It does not support intra-process handling
 */
class GenericSubscription : public rclcpp::SubscriptionBase
{
public:
  using SharedPtr = std::shared_ptr<GenericSubscription>;

  /**
   * Constructor. In order to properly subscribe to a topic, this subscription needs to be added to
   * the node_topic_interface of the node passed into this constructor.
   *
   * \param node_base NodeBaseInterface pointer used in parts of the setup.
   * \param ts Type support handle
   * \param topic_name Topic name
   * \param callback Callback for new messages of serialized form
   */
  GenericSubscription(
    rclcpp::node_interfaces::NodeBaseInterface * node_base,
    const rosidl_message_type_support_t & ts,
    std::string topic_name,
    std::string topic_type,
    const rclcpp::QoS & qos,
    std::function<void(std::shared_ptr<rclcpp::SerializedMessage>, uint64_t timestamp)> callback);

  GenericSubscription(const GenericSubscription &) = delete;

  GenericSubscription & operator=(const GenericSubscription &) = delete;

  // Same as create_serialized_message() as the subscription is to serialized_messages only
  std::shared_ptr<void> create_message() override;

  std::shared_ptr<rclcpp::SerializedMessage> create_serialized_message() override;

  void handle_message(
    std::shared_ptr<void> & message, const rclcpp::MessageInfo & message_info) override;

  void handle_loaned_message(
    void * loaned_message, const rclcpp::MessageInfo & message_info) override;

  // Same as return_serialized_message() as the subscription is to serialized_messages only
  void return_message(std::shared_ptr<void> & message) override;

  void return_serialized_message(std::shared_ptr<rclcpp::SerializedMessage> & message) override;

  // Provide a const reference to the QoS Profile used to create this subscription.
  const rclcpp::QoS & qos_profile() const;

private:
  static std::shared_ptr<rclcpp::SerializedMessage> borrow_serialized_message(size_t capacity);

  rcutils_allocator_t _default_allocator;
  std::function<void(std::shared_ptr<rclcpp::SerializedMessage>, uint64_t timestamp)> _callback;
  const rclcpp::QoS _qos;
  int64_t _last_frame_timestamp;
  std::string _message_type;
  std::string _topic_name;
  bool _use_down_sample{};
};
}  // namespace cobridge
#endif

#endif  // GENERIC_SUBSCRIPTION_HPP_
