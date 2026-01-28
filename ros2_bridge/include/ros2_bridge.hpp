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
#ifndef ROS2_BRIDGE_HPP_
#define ROS2_BRIDGE_HPP_

#ifdef ROS2_VERSION_FOXY
#include <create_generic_subscription.hpp>
#include <create_generic_publisher.hpp>
#endif


#include <rclcpp/rclcpp.hpp>
#include <rosgraph_msgs/msg/clock.hpp>
#include <cobridge.hpp>
#include <generic_client.hpp>
#include <callback_queue.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <message_definition_cache.hpp>
#include <param_utils.hpp>
#include <parameter_interface.hpp>
#include <regex_utils.hpp>
#include <server_factory.hpp>
#include <utils.hpp>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace http_server
{
class HttpServer;
}

namespace cobridge
{

using ConnectionHandle = websocketpp::connection_hdl;
using LogLevel = cobridge_base::WebSocketLogLevel;

#ifdef ROS2_VERSION_FOXY
using SubscriptionsByClient =
  std::map<ConnectionHandle, cobridge::GenericSubscription::SharedPtr, std::owner_less<>>;
using ClientPublications =
  std::unordered_map<cobridge_base::ClientChannelId, cobridge::GenericPublisher::SharedPtr>;
using PublicationsByClient =
  std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
#else
using SubscriptionsByClient =
  std::map<ConnectionHandle, rclcpp::GenericSubscription::SharedPtr, std::owner_less<>>;
using ClientPublications =
  std::unordered_map<cobridge_base::ClientChannelId, rclcpp::GenericPublisher::SharedPtr>;
using PublicationsByClient =
  std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
#endif

class CoBridge : public rclcpp::Node
{
public:
  using TopicAndDatatype = std::pair<std::string, std::string>;

  explicit CoBridge(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  ~CoBridge() override;

  void rosgraph_poll_thread();

  void update_advertised_topics(
    const std::map<std::string, std::vector<std::string>> & channel_id_and_channel);

  void update_advertised_services();

  void update_connection_graph(
    const std::map<std::string, std::vector<std::string>> & topic_names_and_types);

private:
  struct PairHash
  {
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> & pair) const
    {
      return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
  };

  std::unique_ptr<cobridge_base::ServerInterface<ConnectionHandle>> _server;
  cobridge_base::MessageDefinitionCache _message_definition_cache;
  std::vector<std::regex> _topic_whitelist_patterns;
  std::vector<std::regex> _service_whitelist_patterns;
  std::vector<std::regex> _asset_uri_allowlist_patterns;
  std::shared_ptr<ParameterInterface> _param_interface;
  std::unordered_map<cobridge_base::ChannelId, cobridge_base::ChannelWithoutId> _advertised_topics;
  std::unordered_map<cobridge_base::ServiceId,
    cobridge_base::ServiceWithoutId> _advertised_services;
  std::unordered_map<cobridge_base::ChannelId, SubscriptionsByClient> _subscriptions;
  PublicationsByClient _client_advertised_topics;
  std::unordered_map<cobridge_base::ServiceId, GenericClient::SharedPtr> _service_clients;
  rclcpp::CallbackGroup::SharedPtr _subscription_callback_group;
  rclcpp::CallbackGroup::SharedPtr _client_publish_callback_group;
  rclcpp::CallbackGroup::SharedPtr _services_callback_group;
  std::mutex _subscriptions_mutex;
  std::mutex _client_advertisements_mutex;
  std::mutex _services_mutex;
  std::unique_ptr<std::thread> _rosgraph_poll_thread;
  size_t _min_qos_depth = DEFAULT_MIN_QOS_DEPTH;
  size_t _max_qos_depth = DEFAULT_MAX_QOS_DEPTH;
  std::shared_ptr<rclcpp::Subscription<rosgraph_msgs::msg::Clock>> _clock_subscription;
  bool _use_sim_time = false;
  std::vector<std::string> _capabilities;
  std::atomic<bool> _subscribe_graph_updates = false;
  bool _include_hidden = false;
  std::unique_ptr<cobridge_base::CallbackQueue> _fetch_asset_queue;

  std::unique_ptr<http_server::HttpServer> http_server_;

  void subscribe_connection_graph(bool subscribe);

  void subscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle);

  void unsubscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle);

  void client_advertise(
    const cobridge_base::ClientAdvertisement & advertisement,
    ConnectionHandle hdl);

  void client_unadvertise(cobridge_base::ChannelId channel_id, ConnectionHandle hdl);

  void client_message(const cobridge_base::ClientMessage & message, ConnectionHandle hdl);

  void set_parameters(
    const std::vector<cobridge_base::Parameter> & parameters,
    const optional<std::string> & request_id, ConnectionHandle hdl);

  void subscribe_parameters(
    const std::vector<std::string> & parameters,
    cobridge_base::ParameterSubscriptionOperation op, ConnectionHandle hdl);

  void get_parameters(
    const std::vector<std::string> & parameters,
    const optional<std::string> & request_id, ConnectionHandle hdl);

  void parameter_updates(const std::vector<cobridge_base::Parameter> & parameters);

  void log_handler(LogLevel level, char const * msg);

  void  ros_message_handler(
    const cobridge_base::ChannelId & channel_id, ConnectionHandle client_handle,
    std::shared_ptr<rclcpp::SerializedMessage> msg, uint64_t timestamp = 0);

  void service_request(
    const cobridge_base::ServiceRequest & request,
    ConnectionHandle client_handle);

  void fetch_asset(
    const std::string & asset_id, uint32_t request_id,
    ConnectionHandle client_handle);

  bool has_capability(const std::string & capability);
};
}  // namespace cobridge
#endif  // ROS2_BRIDGE_HPP_
