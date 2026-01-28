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

#include <unordered_set>

#ifdef ROS2_VERSION_FOXY
#include <resource_retriever/retriever.h>
#else
#include <resource_retriever/retriever.hpp>
#endif

#include <rmw/types.h>
#include <ros2_bridge.hpp>
#include <http_server.h>

#include <algorithm>
#include <string>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <json.hpp>

// Include for HTTP server
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>

namespace cobridge
{
namespace
{
inline bool is_hidden_topic_or_service(const std::string & name)
{
  if (name.empty()) {
    throw std::invalid_argument("Topic or service name can't be empty");
  }
  return name.front() == '_' || name.find("/_") != std::string::npos;
}

}      // namespace

using namespace std::chrono_literals;
using namespace std::placeholders;
using cobridge_base::is_whitelisted;

CoBridge::CoBridge(const rclcpp::NodeOptions & options)
: Node("cobridge", options)
{
  const char * ros_distro = std::getenv("ROS_DISTRO");
  RCLCPP_INFO(
    this->get_logger(), "Starting cobridge (%s, %s@%s) with %s", ros_distro,
    cobridge_base::COBRIDGE_VERSION, cobridge_base::COBRIDGE_GIT_HASH,
    cobridge_base::websocket_user_agent());

  cobridge::declare_parameters(this);

  const auto port = static_cast<uint16_t>(this->get_parameter(PARAM_PORT).as_int());
  const auto address = this->get_parameter(PARAM_ADDRESS).as_string();
  const auto send_buffer_limit =
    static_cast<size_t>(this->get_parameter(PARAM_SEND_BUFFER_LIMIT).as_int());
  const auto useTLS = this->get_parameter(PARAM_USETLS).as_bool();
  const auto cert_file = this->get_parameter(PARAM_CERTFILE).as_string();
  const auto keyfile = this->get_parameter(PARAM_KEYFILE).as_string();
  _min_qos_depth = static_cast<size_t>(this->get_parameter(PARAM_MIN_QOS_DEPTH).as_int());
  _max_qos_depth = static_cast<size_t>(this->get_parameter(PARAM_MAX_QOS_DEPTH).as_int());
  const auto topic_whitelist = this->get_parameter(PARAM_TOPIC_WHITELIST).as_string_array();
  _topic_whitelist_patterns = parse_regex_strings(this, topic_whitelist);
  const auto service_whitelist = this->get_parameter(PARAM_SERVICE_WHITELIST).as_string_array();
  _service_whitelist_patterns = parse_regex_strings(this, service_whitelist);
  const auto param_whitelist = this->get_parameter(PARAM_PARAMETER_WHITELIST).as_string_array();
  const auto param_whitelist_patterns = parse_regex_strings(this, param_whitelist);
  const auto use_compression = this->get_parameter(PARAM_USE_COMPRESSION).as_bool();
  _use_sim_time = this->get_parameter("use_sim_time").as_bool();
  _capabilities = this->get_parameter(PARAM_CAPABILITIES).as_string_array();
  const auto client_topic_whitelist =
    this->get_parameter(PARAM_CLIENT_TOPIC_WHITELIST).as_string_array();
  const auto client_topic_whitelist_patterns = parse_regex_strings(this, client_topic_whitelist);
  _include_hidden = this->get_parameter(PARAM_INCLUDE_HIDDEN).as_bool();
  const auto asset_uri_allowlist = this->get_parameter(PARAM_ASSET_URI_ALLOWLIST).as_string_array();
  _asset_uri_allowlist_patterns = parse_regex_strings(this, asset_uri_allowlist);

  const auto log_handler = std::bind(&CoBridge::log_handler, this, _1, _2);
  // Fetching of assets may be blocking, hence we fetch them in a separate thread.
  _fetch_asset_queue =
    std::make_unique<cobridge_base::CallbackQueue>(log_handler, 1 /* num_threads */);

  std::string mac_addresses;
  std::vector<std::string> ip_addresses;
  std::string colink_ip;
  http_server::get_dev_mac_addr(mac_addresses);
  http_server::get_dev_ip_addrs(ip_addresses, colink_ip);

  auto http_log_handler = [this](http_server::LogLevel level, const char * msg) {
      switch (level) {
        case http_server::LogLevel::Debug:
          RCLCPP_DEBUG(this->get_logger(), "[HTTP_SERVER] %s", msg);
          break;
        case http_server::LogLevel::Info:
          RCLCPP_INFO(this->get_logger(), "[HTTP_SERVER] %s", msg);
          break;
        case http_server::LogLevel::Warn:
          RCLCPP_WARN(this->get_logger(), "[HTTP_SERVER] %s", msg);
          break;
        case http_server::LogLevel::Error:
          RCLCPP_ERROR(this->get_logger(), "[HTTP_SERVER] %s", msg);
          break;
        case http_server::LogLevel::Fatal:
          RCLCPP_FATAL(this->get_logger(), "[HTTP_SERVER] %s", msg);
          break;
      }
    };

  http_server_ = std::make_unique<http_server::HttpServer>(
    21275, mac_addresses, ip_addresses, http_log_handler);
  http_server_->start();

  cobridge_base::ServerOptions server_options;
  server_options.capabilities = _capabilities;
  if (_use_sim_time) {
    server_options.capabilities.push_back(cobridge_base::CAPABILITY_TIME);
  }
  server_options.capabilities.emplace_back(cobridge_base::CAPABILITY_MESSAGE_TIME);
  server_options.supported_encodings = {"cdr"};
  server_options.metadata = {{"ROS_DISTRO", ros_distro}, {"COLINK", colink_ip}};
  server_options.send_buffer_limit_bytes = send_buffer_limit;
  server_options.session_id = std::to_string(std::time(nullptr));
  server_options.use_compression = use_compression;
  server_options.use_tls = useTLS;
  server_options.cert_file = cert_file;
  server_options.key_file = keyfile;
  server_options.client_topic_whitelist_patterns = client_topic_whitelist_patterns;
  server_options.mac_addr = mac_addresses;
  server_options.ip_addrs = ip_addresses;

  _server = cobridge_base::ServerFactory::create_server<ConnectionHandle>(
    "cobridge", log_handler,
    server_options);

  cobridge_base::ServerHandlers<ConnectionHandle> handlers;
  handlers.subscribe_handler = std::bind(&CoBridge::subscribe, this, _1, _2);
  handlers.unsubscribe_handler = std::bind(&CoBridge::unsubscribe, this, _1, _2);
  handlers.client_advertise_handler = std::bind(&CoBridge::client_advertise, this, _1, _2);
  handlers.client_unadvertise_handler = std::bind(&CoBridge::client_unadvertise, this, _1, _2);
  handlers.client_message_handler = std::bind(&CoBridge::client_message, this, _1, _2);
  handlers.service_request_handler = std::bind(&CoBridge::service_request, this, _1, _2);
  handlers.subscribe_connection_graph_handler =
    std::bind(&CoBridge::subscribe_connection_graph, this, _1);

  if (has_capability(cobridge_base::CAPABILITY_PARAMETERS) ||
    has_capability(cobridge_base::CAPABILITY_PARAMETERS_SUBSCRIBE))
  {
    handlers.parameter_request_handler = std::bind(&CoBridge::get_parameters, this, _1, _2, _3);
    handlers.parameter_change_handler = std::bind(&CoBridge::set_parameters, this, _1, _2, _3);
    handlers.parameter_subscription_handler =
      std::bind(&CoBridge::subscribe_parameters, this, _1, _2, _3);

    _param_interface = std::make_shared<ParameterInterface>(this, param_whitelist_patterns);
    _param_interface->set_param_update_callback(
      std::bind(&CoBridge::parameter_updates, this, _1));
  }

  if (has_capability(cobridge_base::CAPABILITY_ASSETS)) {
    handlers.fetch_asset_handler = [this](
      const std::string & uri, uint32_t requestId,
      ConnectionHandle hdl)
      {
        _fetch_asset_queue->add_callback(
          std::bind(&CoBridge::fetch_asset, this, uri, requestId, hdl));
      };
  }

  _server->set_handlers(std::move(handlers));
  _server->start(address, port);

  // Get the actual port we bound to
  uint16_t listening_port = _server->get_port();
  if (port != listening_port) {
    RCLCPP_DEBUG(
      this->get_logger(), "Reassigning \"port\" parameter from %d to %d", port,
      listening_port);
    this->set_parameter(rclcpp::Parameter{PARAM_PORT, listening_port});
  }

  // Start the thread polling for rosgraph changes
  _rosgraph_poll_thread =
    std::make_unique<std::thread>(std::bind(&CoBridge::rosgraph_poll_thread, this));

  _subscription_callback_group = this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);
  _client_publish_callback_group =
    this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  _services_callback_group = this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);

  if (_use_sim_time) {
    _clock_subscription = this->create_subscription<rosgraph_msgs::msg::Clock>(
      "/clock", rclcpp::QoS{rclcpp::KeepLast(1)}.best_effort(),
      [&](std::shared_ptr<rosgraph_msgs::msg::Clock> msg)
      {
        const auto timestamp = rclcpp::Time{msg->clock}.nanoseconds();
        assert(timestamp >= 0 && "Timestamp is negative");
        _server->broadcast_time(static_cast<uint64_t>(timestamp));
      });
  }
}

CoBridge::~CoBridge()
{
  RCLCPP_INFO(this->get_logger(), "Shutting down %s", this->get_name());
  if (_rosgraph_poll_thread) {
    _rosgraph_poll_thread->join();
  }

  if (http_server_) {
    http_server_->stop();
  }

  _server->stop();
  RCLCPP_INFO(this->get_logger(), "Shutdown complete");
}

void CoBridge::rosgraph_poll_thread()
{
  RCLCPP_INFO(this->get_logger(), "rosgraph_poll_thread was called");
  update_advertised_topics(get_topic_names_and_types());
  update_advertised_services();

  auto graph_event = this->get_graph_event();
  while (rclcpp::ok()) {
    try {
      this->wait_for_graph_change(graph_event, 200ms);
      bool triggered = graph_event->check_and_clear();
      if (triggered) {
        RCLCPP_DEBUG(this->get_logger(), "rosgraph change detected");
        const auto topic_names_and_types = get_topic_names_and_types();
        update_advertised_topics(topic_names_and_types);
        update_advertised_services();
        if (_subscribe_graph_updates) {
          update_connection_graph(topic_names_and_types);
        }
        // Graph changes tend to come in batches, so wait a bit before checking again
        std::this_thread::sleep_for(500ms);
      }
    } catch (const std::exception & ex) {
      RCLCPP_ERROR(this->get_logger(), "Exception thrown in rosgraph_poll_thread: %s", ex.what());
    }
  }

  RCLCPP_INFO(this->get_logger(), "rosgraph polling thread exiting");
}

void CoBridge::update_advertised_topics(
  const std::map<std::string, std::vector<std::string>> & topic_names_and_types)
{
  if (!rclcpp::ok()) {
    return;
  }

  std::unordered_set<TopicAndDatatype, PairHash> latest_topics;
  latest_topics.reserve(topic_names_and_types.size());
  for (const auto & topic_names_and_type : topic_names_and_types) {
    const auto & topic_name = topic_names_and_type.first;
    const auto & datatypes = topic_names_and_type.second;

    // Ignore hidden topics if not explicitly included
    if (!_include_hidden && is_hidden_topic_or_service(topic_name)) {
      continue;
    }

    // Ignore the topic if it is not on the topic whitelist
    if (is_whitelisted(topic_name, _topic_whitelist_patterns)) {
      for (const auto & datatype : datatypes) {
        latest_topics.emplace(topic_name, datatype);
      }
    }
  }

  if (const auto num_ignored_topics = topic_names_and_types.size() - latest_topics.size()) {
    RCLCPP_DEBUG(
      this->get_logger(),
      "%zu topics have been ignored as they do not match any pattern on the topic whitelist",
      num_ignored_topics);
  }

  std::lock_guard<std::mutex> lock(_subscriptions_mutex);

  // Remove channels for which the topic does not exist anymore
  std::vector<cobridge_base::ChannelId> channel_ids_to_remove;
  for (auto channel_iter = _advertised_topics.begin(); channel_iter != _advertised_topics.end(); ) {
    const TopicAndDatatype topic_and_datatype = {channel_iter->second.topic,
      channel_iter->second.schema_name};
    if (latest_topics.find(topic_and_datatype) == latest_topics.end()) {
      const auto channel_id = channel_iter->first;
      channel_ids_to_remove.push_back(channel_id);
      _subscriptions.erase(channel_id);
      RCLCPP_INFO(
        this->get_logger(), "Removed channel %d for topic \"%s\" (%s)", channel_id,
        topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str());
      channel_iter = _advertised_topics.erase(channel_iter);
    } else {
      channel_iter++;
    }
  }
  _server->remove_channels(channel_ids_to_remove);

  // Add new channels for new topics
  std::vector<cobridge_base::ChannelWithoutId> channels_to_add;
  for (const auto & topic_and_datatype : latest_topics) {
    if (std::find_if(
        _advertised_topics.begin(), _advertised_topics.end(),
        [topic_and_datatype](const auto & channel_id_and_channel)
        {
          const auto & channel = channel_id_and_channel.second;
          return channel.topic == topic_and_datatype.first &&
          channel.schema_name == topic_and_datatype.second;
        }) != _advertised_topics.end())
    {
      continue;            // Topic already advertised
    }

    cobridge_base::ChannelWithoutId new_channel{};
    new_channel.topic = topic_and_datatype.first;
    new_channel.schema_name = topic_and_datatype.second;

    try {
      auto [format, schema] =
        _message_definition_cache.get_full_msg_text(topic_and_datatype.second);
      switch (format) {
        case cobridge_base::MessageDefinitionFormat::MSG:
        case cobridge_base::MessageDefinitionFormat::SRV_REQ:
        case cobridge_base::MessageDefinitionFormat::SRV_RESP:
          new_channel.encoding = "cdr";
          new_channel.schema = schema;
          new_channel.schema_encoding = "ros2msg";
          break;
        case cobridge_base::MessageDefinitionFormat::IDL:
          new_channel.encoding = "cdr";
          new_channel.schema = schema;
          new_channel.schema_encoding = "ros2idl";
          break;
      }
    } catch (const cobridge_base::DefinitionNotFoundError & err) {
      RCLCPP_WARN(
        this->get_logger(), "Could not find definition for type %s: %s",
        topic_and_datatype.second.c_str(), err.what());
      // We still advertise the channel, but with an emtpy schema
      new_channel.schema = "";
    } catch (const std::exception & err) {
      RCLCPP_WARN(
        this->get_logger(), "Failed to add channel for topic \"%s\" (%s): %s",
        topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str(), err.what());
      continue;
    }

    channels_to_add.push_back(new_channel);
  }

  const auto channel_ids = _server->add_channels(channels_to_add);
  for (size_t i = 0; i < channels_to_add.size(); ++i) {
    const auto channel_id = channel_ids[i];
    const auto & channel = channels_to_add[i];
    _advertised_topics.emplace(channel_id, channel);
    RCLCPP_DEBUG(
      this->get_logger(), "Advertising channel %d for topic \"%s\" (%s)", channel_id,
      channel.topic.c_str(), channel.schema_name.c_str());
  }
}

void CoBridge::update_advertised_services()
{
  if (!rclcpp::ok()) {
    return;
  } else if (!has_capability(cobridge_base::CAPABILITY_SERVICES)) {
    return;
  }

  // Get the current list of visible services and datatypes from the ROS graph
  const auto service_names_and_types =
    this->get_node_graph_interface()->get_service_names_and_types();

  std::lock_guard<std::mutex> lock(_services_mutex);

  // Remove advertisements for services that have been removed
  std::vector<cobridge_base::ServiceId> services_to_remove;
  for (const auto & service : _advertised_services) {
    const auto it = std::find_if(
      service_names_and_types.begin(), service_names_and_types.end(),
      [service](const auto & service_name_and_types)
      {
        return service_name_and_types.first == service.second.name;
      });
    if (it == service_names_and_types.end()) {
      services_to_remove.push_back(service.first);
    }
  }
  for (auto service_id : services_to_remove) {
    _advertised_services.erase(service_id);
  }
  _server->remove_services(services_to_remove);

  // Advertise new services
  std::vector<cobridge_base::ServiceWithoutId> new_services;
  for (const auto & service_names_and_type : service_names_and_types) {
    const auto & service_name = service_names_and_type.first;
    const auto & datatypes = service_names_and_type.second;

    // Ignore the service if it's already advertised
    if (std::find_if(
        _advertised_services.begin(), _advertised_services.end(),
        [service_name](const auto & idWithService)
        {
          return idWithService.second.name == service_name;
        }) != _advertised_services.end())
    {
      continue;
    }

    // Ignore hidden services if not explicitly included
    if (!_include_hidden && is_hidden_topic_or_service(service_name)) {
      continue;
    }

    // Ignore the service if it is not on the service whitelist
    if (!is_whitelisted(service_name, _service_whitelist_patterns)) {
      continue;
    }

    cobridge_base::ServiceWithoutId service;
    service.name = service_name;
    service.type = datatypes.front();

    try {
      RCLCPP_DEBUG(
        this->get_logger(), "service name: %s, service type: %s",
        service.name.c_str(), service.type.c_str());
      auto service_definition = _message_definition_cache.get_full_srv_text(service.type);
      service.request_schema = service_definition["request"].second;
      service.response_schema = service_definition["response"].second;
      RCLCPP_DEBUG(
        this->get_logger(), "service request schema: %s",
        service.request_schema.c_str());
      RCLCPP_DEBUG(
        this->get_logger(), "service response schema: %s",
        service.response_schema.c_str());
    } catch (const cobridge_base::DefinitionNotFoundError & err) {
      RCLCPP_WARN(
        this->get_logger(), "Could not find definition for type %s: %s",
        service.type.c_str(), err.what());
      service.request_schema = "";
      service.response_schema = "";
    } catch (const std::exception & err) {
      RCLCPP_WARN(
        this->get_logger(), "Failed to add service \"%s\" (%s): %s", service.name.c_str(),
        service.type.c_str(), err.what());
      continue;
    }

    new_services.push_back(service);
  }

  const auto service_ids = _server->add_services(new_services);
  for (size_t i = 0; i < service_ids.size(); ++i) {
    _advertised_services.emplace(service_ids[i], new_services[i]);
  }
}

void CoBridge::update_connection_graph(
  const std::map<std::string, std::vector<std::string>> & topic_names_and_types)
{
  cobridge_base::MapOfSets publishers, subscribers;

  for (const auto & topic_name_and_type : topic_names_and_types) {
    const auto & topic_name = topic_name_and_type.first;
    if (!is_whitelisted(topic_name, _topic_whitelist_patterns)) {
      continue;
    }

    const auto publishers_info = get_publishers_info_by_topic(topic_name);
    const auto subscribers_info = get_subscriptions_info_by_topic(topic_name);
    std::unordered_set<std::string> publisher_ids, subscriber_ids;
    for (const auto & publisher : publishers_info) {
      const auto & ns = publisher.node_namespace();
      const auto sep = (!ns.empty() && ns.back() == '/') ? "" : "/";
      publisher_ids.insert(ns + sep + publisher.node_name());
    }
    for (const auto & subscriber : subscribers_info) {
      const auto & ns = subscriber.node_namespace();
      const auto sep = (!ns.empty() && ns.back() == '/') ? "" : "/";
      subscriber_ids.insert(ns + sep + subscriber.node_name());
    }
    publishers.emplace(topic_name, publisher_ids);
    subscribers.emplace(topic_name, subscriber_ids);
  }

  cobridge_base::MapOfSets services;
  for (const auto & fqn_node_name : get_node_names()) {
    const auto [node_namespace, node_name] = get_node_and_node_namespace(fqn_node_name);
    const auto service_names_and_types = get_service_names_and_types_by_node(
      node_name,
      node_namespace);

    for (const auto & [service_name, service_types] : service_names_and_types) {
      (void) service_types;
      if (is_whitelisted(service_name, _service_whitelist_patterns)) {
        services[service_name].insert(fqn_node_name);
      }
    }
  }

  _server->update_connection_graph(publishers, subscribers, services);
}

void CoBridge::subscribe_connection_graph(bool subscribe)
{
  if ((_subscribe_graph_updates = subscribe)) {
    update_connection_graph(get_topic_names_and_types());
  }
}

void CoBridge::subscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle)
{
  std::lock_guard<std::mutex> lock(_subscriptions_mutex);
  auto it = _advertised_topics.find(channel_id);
  if (it == _advertised_topics.end()) {
    throw cobridge_base::ChannelError(
            channel_id,
            "Received subscribe request for unknown channel " + std::to_string(channel_id));
  }

  const auto & channel = it->second;
  const auto & topic = channel.topic;
  const auto & datatype = channel.schema_name;

  // Get client subscriptions for this channel or insert an empty map.
  auto [subscriptions_iter, first_subscription] =
    _subscriptions.emplace(channel_id, SubscriptionsByClient());
  auto & subscriptions_by_client = subscriptions_iter->second;

  if (!first_subscription &&
    subscriptions_by_client.find(client_handle) != subscriptions_by_client.end())
  {
    throw cobridge_base::ChannelError(
            channel_id, "Client is already subscribed to channel " + std::to_string(channel_id));
  }

  rclcpp::SubscriptionEventCallbacks event_callbacks;
  event_callbacks.incompatible_qos_callback = [&](const rclcpp::QOSRequestedIncompatibleQoSInfo &)
    {
      RCLCPP_ERROR(
        this->get_logger(), "Incompatible subscriber QoS settings for topic \"%s\" (%s)",
        topic.c_str(), datatype.c_str());
    };

  rclcpp::SubscriptionOptions subscription_options;
  subscription_options.event_callbacks = event_callbacks;
  subscription_options.callback_group = _subscription_callback_group;

  // Select an appropriate subscription QOS profile. This is similar to how ros2 topic echo
  // does it:
  // https://github.com/ros2/ros2cli/blob/619b3d1c9/ros2topic/ros2topic/verb/echo.py#L137-L194
  size_t depth = 0;
  size_t reliability_reliable_endpoints_count = 0;
  size_t durability_transient_local_endpoints_count = 0;

  const auto publisher_info = this->get_publishers_info_by_topic(topic);
  for (const auto & publisher : publisher_info) {
    const auto & qos = publisher.qos_profile();
    if (qos.get_rmw_qos_profile().reliability == RMW_QOS_POLICY_RELIABILITY_RELIABLE) {
      ++reliability_reliable_endpoints_count;
    }
    if (qos.get_rmw_qos_profile().durability == RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL) {
      ++durability_transient_local_endpoints_count;
    }
    const size_t publisher_history_depth = std::max(1ul, qos.get_rmw_qos_profile().depth);
    depth = depth + publisher_history_depth;
  }

  depth = std::max(depth, _min_qos_depth);
  if (depth > _max_qos_depth) {
    RCLCPP_WARN(
      this->get_logger(),
      "Limiting history depth for topic '%s' to %zu (was %zu). You may want to increase "
      "the max_qos_depth parameter value.",
      topic.c_str(), _max_qos_depth, depth);
    depth = _max_qos_depth;
  }

  rclcpp::QoS qos{rclcpp::KeepLast(depth)};

  // If all endpoints are reliable, ask for reliable
  if (reliability_reliable_endpoints_count == publisher_info.size()) {
    qos.reliable();
  } else {
    if (reliability_reliable_endpoints_count > 0) {
      RCLCPP_WARN(
        this->get_logger(),
        "Some, but not all, publishers on topic '%s' are offering QoSReliabilityPolicy.RELIABLE. "
        "Falling back to QoSReliabilityPolicy.BEST_EFFORT as it will connect to all publishers",
        topic.c_str());
    }
    qos.best_effort();
  }

  // If all endpoints are transient_local, ask for transient_local
  if (durability_transient_local_endpoints_count == publisher_info.size()) {
    qos.transient_local();
  } else {
    if (durability_transient_local_endpoints_count > 0) {
      RCLCPP_WARN(
        this->get_logger(),
        "Some, but not all, publishers on topic '%s' are offering "
        "QoSDurabilityPolicy.TRANSIENT_LOCAL. Falling back to "
        "QoSDurabilityPolicy.VOLATILE as it will connect to all publishers",
        topic.c_str());
    }
    qos.durability_volatile();
  }

  if (first_subscription) {
    RCLCPP_INFO(
      this->get_logger(), "Subscribing to topic \"%s\" (%s) on channel %d", topic.c_str(),
      datatype.c_str(), channel_id);
  } else {
    RCLCPP_INFO(
      this->get_logger(), "Adding subscriber #%zu to topic \"%s\" (%s) on channel %d",
      subscriptions_by_client.size(), topic.c_str(), datatype.c_str(), channel_id);
  }

  try {
#ifdef ROS2_VERSION_FOXY
    auto subscriber = cobridge::create_generic_subscription(
      this->get_node_topics_interface(),
      topic, datatype, qos,
      std::bind(&CoBridge::ros_message_handler, this, channel_id, client_handle, _1, _2));
#else
    auto subscriber = this->create_generic_subscription(
      topic, datatype, qos,
      [this, channel_id, client_handle](std::shared_ptr<rclcpp::SerializedMessage> msg) {
        this->ros_message_handler(channel_id, client_handle, msg);
      },
      subscription_options);
#endif

    subscriptions_by_client.emplace(client_handle, std::move(subscriber));
  } catch (const std::exception & ex) {
    throw cobridge_base::ChannelError(
            channel_id,
            "Failed to subscribe to topic " + topic + " (" + datatype + "): " + ex.what());
  }
}

void CoBridge::unsubscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle)
{
  std::lock_guard<std::mutex> lock(_subscriptions_mutex);

  const auto channel_iter = _advertised_topics.find(channel_id);
  if (channel_iter == _advertised_topics.end()) {
    throw cobridge_base::ChannelError(
            channel_id,
            "Received unsubscribe request for unknown channel " + std::to_string(channel_id));
  }
  const auto & channel = channel_iter->second;

  auto subscriptions_iter = _subscriptions.find(channel_id);
  if (subscriptions_iter == _subscriptions.end()) {
    throw cobridge_base::ChannelError(
            channel_id, "Received unsubscribe request for channel " +
            std::to_string(channel_id) +
            " that was not subscribed to");
  }

  auto & subscriptions_by_client = subscriptions_iter->second;
  const auto client_subscription = subscriptions_by_client.find(client_handle);
  if (client_subscription == subscriptions_by_client.end()) {
    throw cobridge_base::ChannelError(
            channel_id, "Received unsubscribe request for channel " + std::to_string(channel_id) +
            "from a client that was not subscribed to this channel");
  }

  subscriptions_by_client.erase(client_subscription);
  if (subscriptions_by_client.empty()) {
    RCLCPP_INFO(
      this->get_logger(), "Unsubscribing from topic \"%s\" (%s) on channel %d",
      channel.topic.c_str(), channel.schema_name.c_str(), channel_id);
    _subscriptions.erase(subscriptions_iter);
  } else {
    RCLCPP_INFO(
      this->get_logger(),
      "Removed one subscription from channel %d (%zu subscription(s) left)", channel_id,
      subscriptions_by_client.size());
  }
}

void CoBridge::client_advertise(
  const cobridge_base::ClientAdvertisement & advertisement,
  ConnectionHandle hdl)
{
  std::lock_guard<std::mutex> lock(_client_advertisements_mutex);

  // Get client publications or insert an empty map.
  auto [client_publications_iter, is_first_publication] =
    _client_advertised_topics.emplace(hdl, ClientPublications());

  auto & client_publications = client_publications_iter->second;

  if (!is_first_publication &&
    client_publications.find(advertisement.channel_id) != client_publications.end())
  {
    throw cobridge_base::ClientChannelError(
            advertisement.channel_id,
            "Received client advertisement from " + _server->remote_endpoint_string(
              hdl) + " for channel " +
            std::to_string(advertisement.channel_id) + " it had already advertised");
  }

  try {
    // Create a new topic advertisement
    const auto & topic_name = advertisement.topic;
    const auto & topic_type = advertisement.schema_name;

    // Lookup if there are publishers from other nodes for that topic. If that's the case, we use
    // a matching QoS profile.
    const auto other_publishers = get_publishers_info_by_topic(topic_name);
    const auto other_publisher_iter =
      std::find_if(
      other_publishers.begin(), other_publishers.end(),
      [this](const rclcpp::TopicEndpointInfo & endpoint)
      {
        return endpoint.node_name() != this->get_name() ||
        endpoint.node_namespace() != this->get_namespace();
      });
    rclcpp::QoS qos = other_publisher_iter == other_publishers.end() ? rclcpp::SystemDefaultsQoS() :
      other_publisher_iter->qos_profile();

    // When the QoS profile is copied from another existing publisher, it can happen that the
    // history policy is Unknown, leading to an error when subsequently trying to create a publisher
    // with that QoS profile. As a fix, we explicitly set the history policy to the system default.
    if (qos.get_rmw_qos_profile().history == RMW_QOS_POLICY_HISTORY_UNKNOWN) {
      qos.history(RMW_QOS_POLICY_HISTORY_SYSTEM_DEFAULT);
    }
    rclcpp::PublisherOptions publisher_options{};
    publisher_options.callback_group = _client_publish_callback_group;
#ifdef ROS2_VERSION_FOXY
    auto publisher = cobridge::create_generic_publisher(
      this->get_node_topics_interface(), topic_name, topic_type, qos);
#else
    auto publisher = rclcpp::create_generic_publisher(
      this->get_node_topics_interface(), topic_name, topic_type, qos);
#endif

    RCLCPP_INFO(
      this->get_logger(), "Client %s is advertising \"%s\" (%s) on channel %d",
      _server->remote_endpoint_string(hdl).c_str(), topic_name.c_str(), topic_type.c_str(),
      advertisement.channel_id);

    // Store the new topic advertisement
    client_publications.emplace(advertisement.channel_id, std::move(publisher));
  } catch (const std::exception & ex) {
    throw cobridge_base::ClientChannelError(
            advertisement.channel_id,
            std::string("Failed to create publisher: ") + ex.what());
  }
}

void CoBridge::client_unadvertise(cobridge_base::ChannelId channel_id, ConnectionHandle hdl)
{
  std::lock_guard<std::mutex> lock(_client_advertisements_mutex);

  auto it = _client_advertised_topics.find(hdl);
  if (it == _client_advertised_topics.end()) {
    throw cobridge_base::ClientChannelError(
            channel_id,
            "Ignoring client unadvertisement from " + _server->remote_endpoint_string(hdl) +
            " for unknown channel " + std::to_string(channel_id) +
            ", client has no advertised topics");
  }

  auto & client_publications = it->second;
  auto it2 = client_publications.find(channel_id);
  if (it2 == client_publications.end()) {
    throw cobridge_base::ClientChannelError(
            channel_id,
            "Ignoring client unadvertisement from " + _server->remote_endpoint_string(hdl) +
            " for unknown channel " + std::to_string(channel_id) + ", client has " +
            std::to_string(client_publications.size()) + " advertised topic(s)");
  }

  const auto & publisher = it2->second;
  RCLCPP_INFO(
    this->get_logger(),
    "Client %s is no longer advertising %s (%zu subscribers) on channel %d",
    _server->remote_endpoint_string(hdl).c_str(), publisher->get_topic_name(),
    publisher->get_subscription_count(), channel_id);

  client_publications.erase(it2);
  if (client_publications.empty()) {
    _client_advertised_topics.erase(it);
  }

  // Create a timer that immediately goes out of scope (so it never fires) which will trigger
  // the previously destroyed publisher to be cleaned up. This is a workaround for
  // https://github.com/ros2/rclcpp/issues/2146
  this->create_wall_timer(
    1s, []()
    {});
}

void CoBridge::client_message(const cobridge_base::ClientMessage & message, ConnectionHandle hdl)
{
#ifdef ROS2_VERSION_FOXY
  cobridge::GenericPublisher::SharedPtr publisher;
#else
  rclcpp::GenericPublisher::SharedPtr publisher;
#endif


  {
    const auto channel_id = message.advertisement.channel_id;
    std::lock_guard<std::mutex> lock(_client_advertisements_mutex);

    auto it = _client_advertised_topics.find(hdl);
    if (it == _client_advertised_topics.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id, "Dropping client message from " + _server->remote_endpoint_string(hdl) +
              " for unknown channel " + std::to_string(channel_id) +
              ", client has no advertised topics");
    }

    auto & client_publications = it->second;
    auto it2 = client_publications.find(channel_id);
    if (it2 == client_publications.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id, "Dropping client message from " + _server->remote_endpoint_string(hdl) +
              " for unknown channel " + std::to_string(channel_id) + ", client has " +
              std::to_string(client_publications.size()) + " advertised topic(s)");
    }
    publisher = it2->second;
  }

  // Copy the message payload into a SerializedMessage object
  rclcpp::SerializedMessage serialized_message{message.getLength()};
  auto & rcl_serialized_msg = serialized_message.get_rcl_serialized_message();
  std::memcpy(rcl_serialized_msg.buffer, message.getData(), message.getLength());
  rcl_serialized_msg.buffer_length = message.getLength();

  // Publish the message
#ifdef ROS2_VERSION_FOXY
  publisher->publish(
    std::make_shared<rcl_serialized_message_t>(
      serialized_message.
      get_rcl_serialized_message()));
#else
  publisher->publish(serialized_message);
#endif
}

void CoBridge::set_parameters(
  const std::vector<cobridge_base::Parameter> & parameters,
  const optional<std::string> & request_id, cobridge::ConnectionHandle hdl)
{
  _param_interface->set_params(parameters, std::chrono::seconds(5));

  // If a request Id was given, send potentially updated parameters back to client
  if (request_id.has_value()) {
    std::vector<std::string> parameter_names(parameters.size());
    for (size_t i = 0; i < parameters.size(); ++i) {
      parameter_names[i] = parameters[i].get_name();
    }
    get_parameters(parameter_names, request_id, hdl);
  }
}

void CoBridge::get_parameters(
  const std::vector<std::string> & parameters,
  const optional<std::string> & request_id, cobridge::ConnectionHandle hdl)
{
  const auto params = _param_interface->get_params(parameters, std::chrono::seconds(5));
  _server->publish_parameter_values(hdl, params, request_id);
}

void CoBridge::subscribe_parameters(
  const std::vector<std::string> & parameters,
  cobridge_base::ParameterSubscriptionOperation op,
  cobridge::ConnectionHandle)
{
  if (op == cobridge_base::ParameterSubscriptionOperation::SUBSCRIBE) {
    _param_interface->subscribe_params(parameters);
  } else {
    _param_interface->unsubscribe_params(parameters);
  }
}

void CoBridge::parameter_updates(const std::vector<cobridge_base::Parameter> & parameters)
{
  _server->update_parameter_values(parameters);
}

void CoBridge::log_handler(LogLevel level, char const * msg)
{
  switch (level) {
    case LogLevel::Debug:
      RCLCPP_DEBUG(this->get_logger(), "[WS] %s", msg);
      break;
    case LogLevel::Info:
      RCLCPP_INFO(this->get_logger(), "[WS] %s", msg);
      break;
    case LogLevel::Warn:
      RCLCPP_WARN(this->get_logger(), "[WS] %s", msg);
      break;
    case LogLevel::Error:
      RCLCPP_ERROR(this->get_logger(), "[WS] %s", msg);
      break;
    case LogLevel::Critical:
      RCLCPP_FATAL(this->get_logger(), "[WS] %s", msg);
      break;
  }
}

void CoBridge::ros_message_handler(
  const cobridge_base::ChannelId & channel_id,
  ConnectionHandle client_handle,
  std::shared_ptr<rclcpp::SerializedMessage> msg,
  uint64_t timestamp)
{
  // NOTE: Do not call any RCLCPP_* logging functions from this function. Otherwise, subscribing
  // to `/rosout` will cause a feedback loop
  const auto rcl_serialized_msg = msg->get_rcl_serialized_message();
  _server->send_message(
    client_handle, channel_id, timestamp != 0 ? timestamp : this->now().nanoseconds(),
    rcl_serialized_msg.buffer, rcl_serialized_msg.buffer_length);
}

void CoBridge::service_request(
  const cobridge_base::ServiceRequest & request,
  ConnectionHandle client_handle)
{
  RCLCPP_DEBUG(this->get_logger(), "Received a request for service %d", request.service_id);

  std::lock_guard<std::mutex> lock(_services_mutex);
  const auto service_iter = _advertised_services.find(request.service_id);
  if (service_iter == _advertised_services.end()) {
    throw cobridge_base::ServiceError(
            request.service_id,
            "Service with id " + std::to_string(request.service_id) + " does not exist");
  }

  auto client_iter = _service_clients.find(request.service_id);
  if (client_iter == _service_clients.end()) {
    try {
      auto client_options = rcl_client_get_default_options();
      auto gen_client = GenericClient::make_shared(
        this->get_node_base_interface().get(), this->get_node_graph_interface(),
        service_iter->second.name, service_iter->second.type, client_options);
      client_iter = _service_clients.emplace(request.service_id, std::move(gen_client)).first;
      this->get_node_services_interface()->add_client(
        client_iter->second,
        _services_callback_group);
    } catch (const std::exception & ex) {
      throw cobridge_base::ServiceError(
              request.service_id,
              "Failed to create service client for service " + service_iter->second.name + ": " +
              ex.what());
    }
  }

  auto client = client_iter->second;
  RCLCPP_DEBUG(
    this->get_logger(), "Waiting for service '%s' to be available...",
    service_iter->second.name.c_str());

  if (!client->wait_for_service(1s)) {
    RCLCPP_ERROR(
      this->get_logger(), "Service '%s' is not available (timeout after 1s)",
      service_iter->second.name.c_str());
    throw cobridge_base::ServiceError(
            request.service_id,
            "Service " + service_iter->second.name + " is not available");
  }

  RCLCPP_DEBUG(
    this->get_logger(), "Service '%s' is available, preparing request...",
    service_iter->second.name.c_str());

  auto req_message = std::make_shared<rclcpp::SerializedMessage>(request.serv_data.size());
  auto & rcl_serialized_msg = req_message->get_rcl_serialized_message();
  std::memcpy(rcl_serialized_msg.buffer, request.serv_data.data(), request.serv_data.size());
  rcl_serialized_msg.buffer_length = request.serv_data.size();

  RCLCPP_DEBUG(
    this->get_logger(),
    "Sending async service request to '%s' (service_id=%d, call_id=%d, size=%zu bytes)",
    service_iter->second.name.c_str(), request.service_id, request.call_id,
    request.serv_data.size());

  auto service_name = service_iter->second.name;  // Capture for callback
  auto response_received_callback = [this, request, service_name,
      client_handle](GenericClient::SharedFuture future)
    {
      RCLCPP_DEBUG(
        this->get_logger(),
        "Received response from service '%s' (service_id=%d, call_id=%d)",
        service_name.c_str(), request.service_id, request.call_id);

      const auto serialized_response_msg = future.get()->get_rcl_serialized_message();
      RCLCPP_DEBUG(
        this->get_logger(), "Response size: %zu bytes",
        serialized_response_msg.buffer_length);

      cobridge_base::ServiceRequest response{
        request.service_id, request.call_id, request.encoding,
        std::vector<uint8_t>(serialized_response_msg.buffer_length)};
      std::memcpy(
        response.serv_data.data(), serialized_response_msg.buffer,
        serialized_response_msg.buffer_length);

      RCLCPP_DEBUG(
        this->get_logger(),
        "Sending service response back to client (service_id=%d, call_id=%d)",
        request.service_id, request.call_id);
      _server->send_service_response(client_handle, response);
    };
  client->async_send_request(req_message, response_received_callback);
}

void CoBridge::fetch_asset(
  const std::string & asset_id, uint32_t request_id,
  ConnectionHandle client_handle)
{
  cobridge_base::FetchAssetResponse response;
  response.request_id = request_id;

  try {
    // We reject URIs that are not on the allowlist or that contain two consecutive dots. The latter
    // can be utilized to construct URIs for retrieving confidential files that should not be
    // accessible over the WebSocket connection. Example:
    // `package://<pkg_name>/../../../secret.txt`. This is an extra security measure and should not
    // be necessary if the allowlist is strict enough.
    if (asset_id.find("..") != std::string::npos ||
      !is_whitelisted(asset_id, _asset_uri_allowlist_patterns))
    {
      throw std::runtime_error("Asset URI not allowed: " + asset_id);
    }

    resource_retriever::Retriever resource_retriever;
    const resource_retriever::MemoryResource memory_resource = resource_retriever.get(asset_id);
    response.status = cobridge_base::FetchAssetStatus::Success;
    response.error_message = "";
    response.data.resize(memory_resource.size);
    std::memcpy(response.data.data(), memory_resource.data.get(), memory_resource.size);
  } catch (const std::exception & ex) {
    RCLCPP_WARN(
      this->get_logger(), "Failed to retrieve asset '%s': %s", asset_id.c_str(),
      ex.what());
    response.status = cobridge_base::FetchAssetStatus::Error;
    response.error_message = "Failed to retrieve asset " + asset_id;
  }

  if (_server) {
    _server->send_fetch_asset_response(client_handle, response);
  }
}

bool CoBridge::has_capability(const std::string & capability)
{
  return std::find(_capabilities.begin(), _capabilities.end(), capability) != _capabilities.end();
}

}  // namespace cobridge

#include <rclcpp_components/register_node_macro.hpp>

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(cobridge::CoBridge)
