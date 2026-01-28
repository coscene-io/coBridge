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

// #include <shared_mutex>
#include <nodelet/nodelet.h>
#include <http_server.h>

#include <pluginlib/class_list_macros.h>
#include <resource_retriever/retriever.h>
#include <ros/message_event.h>
#include <ros/ros.h>
#include <ros/xmlrpc_manager.h>
#include <ros_babel_fish/babel_fish_message.h>
#include <ros_babel_fish/generation/providers/integrated_description_provider.h>
#include <rosgraph_msgs/Clock.h>
#include <websocketpp/common/connection_hdl.hpp>

#include <cobridge.hpp>
#include <generic_service.hpp>
#include <param_utils.hpp>
#include <regex_utils.hpp>
#include <server_factory.hpp>
#include <service_utils.hpp>
#include <websocket_server.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <regex>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace http_server
{
class HttpServer;
}

namespace
{

inline std::unordered_set<std::string> rpc_value_to_string_set(const XmlRpc::XmlRpcValue & v)
{
  std::unordered_set<std::string> set;
  for (int i = 0; i < v.size(); ++i) {
    XmlRpc::XmlRpcValue item = v[i];
    set.insert(static_cast<std::string>(item));
  }
  return set;
}

}  // namespace

namespace cobridge
{

constexpr int DEFAULT_PORT = 21274;
constexpr char DEFAULT_ADDRESS[] = "0.0.0.0";
constexpr int DEFAULT_MAX_UPDATE_MS = 5000;
constexpr char ROS1_CHANNEL_ENCODING[] = "ros1";
constexpr uint32_t SUBSCRIPTION_QUEUE_LENGTH = 10;
constexpr double MIN_UPDATE_PERIOD_MS = 100.0;
constexpr uint32_t PUBLICATION_QUEUE_LENGTH = 10;
constexpr int DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS = 250;

using ConnectionHandle = websocketpp::connection_hdl;
using TopicAndDatatype = std::pair<std::string, std::string>;
using SubscriptionsByClient = std::map<ConnectionHandle, ros::Subscriber,
    std::owner_less<ConnectionHandle>>;
using ClientPublications = std::unordered_map<cobridge_base::ClientChannelId, ros::Publisher>;
using PublicationsByClient = std::map<ConnectionHandle, ClientPublications,
    std::owner_less<ConnectionHandle>>;
using cobridge_base::is_whitelisted;

class CoBridge : public nodelet::Nodelet
{
public:
  CoBridge() = default;
  void onInit() override
  {
    auto & nhp = getPrivateNodeHandle();
    const auto address = nhp.param<std::string>("address", DEFAULT_ADDRESS);
    const int port = nhp.param<int>("port", DEFAULT_PORT);
    const auto send_buffer_limit = static_cast<size_t>(
      nhp.param<int>("send_buffer_limit", cobridge_base::DEFAULT_SEND_BUFFER_LIMIT_BYTES));
    const auto use_tls = nhp.param<bool>("tls", false);
    const auto certfile = nhp.param<std::string>("certfile", "");
    const auto keyfile = nhp.param<std::string>("keyfile", "");
    max_update_ms_ = static_cast<size_t>(nhp.param<int>("max_update_ms", DEFAULT_MAX_UPDATE_MS));
    const auto use_compression = nhp.param<bool>("use_compression", false);
    use_sim_time_ = nhp.param<bool>("/use_sim_time", false);
    const auto session_id = nhp.param<std::string>("/run_id", std::to_string(std::time(nullptr)));
    capabilities_ = nhp.param<std::vector<std::string>>(
      "capabilities", std::vector<std::string>(
        cobridge_base::DEFAULT_CAPABILITIES.begin(),
        cobridge_base::DEFAULT_CAPABILITIES.end()));
    service_retrieval_timeout_ms_ = nhp.param<int>(
      "service_type_retrieval_timeout_ms",
      DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS);

    const auto topic_whitelist_patterns =
      nhp.param<std::vector<std::string>>("topic_whitelist", {".*"});

    topic_whitelist_patterns_ = parse_regex_patterns(topic_whitelist_patterns);
    if (topic_whitelist_patterns.size() != topic_whitelist_patterns_.size()) {
      ROS_ERROR("Failed to parse one or more topic whitelist patterns");
      ROS_ERROR(
        "Input patterns: %zu, Parsed patterns: %zu",
        topic_whitelist_patterns.size(), topic_whitelist_patterns_.size());
    }

    const auto param_whitelist = nhp.param<std::vector<std::string>>("param_whitelist", {".*"});
    param_whitelist_patterns = parse_regex_patterns(param_whitelist);
    if (param_whitelist.size() != param_whitelist_patterns.size()) {
      ROS_ERROR("Failed to parse one or more param whitelist patterns");
    }

    const auto service_whitelist = nhp.param<std::vector<std::string>>("service_whitelist", {".*"});
    service_whitelist_patterns = parse_regex_patterns(service_whitelist);
    if (service_whitelist.size() != service_whitelist_patterns.size()) {
      ROS_ERROR("Failed to parse one or more service whitelist patterns");
    }

    const auto client_topic_whitelist =
      nhp.param<std::vector<std::string>>("client_topic_whitelist", {".*"});
    const auto client_topic_whitelist_patterns = parse_regex_patterns(client_topic_whitelist);
    if (client_topic_whitelist.size() != client_topic_whitelist_patterns.size()) {
      ROS_ERROR("Failed to parse one or more topic whitelist patterns");
    }

    const auto asset_uri_allowlist = nhp.param<std::vector<std::string>>(
      "asset_uri_allowlist",
      {"^package://(?:\\w+/"
        ")*\\w+\\.(?:dae|fbx|glb|gltf|jpeg|jpg|mtl|obj|png|stl|tif|tiff|urdf|webp|xacro)$"});
    asset_uri_allowlist_patterns_ = parse_regex_patterns(asset_uri_allowlist);
    if (asset_uri_allowlist.size() != asset_uri_allowlist_patterns_.size()) {
      ROS_ERROR("Failed to parse one or more asset URI whitelist patterns");
    }

    const char * ros_distro = std::getenv("ROS_DISTRO");
    ROS_INFO(
      "Starting cobridge (%s, %s@%s) with %s", ros_distro,
      cobridge_base::COBRIDGE_VERSION, cobridge_base::COBRIDGE_GIT_HASH,
      cobridge_base::websocket_user_agent());

    std::string mac_addresses;
    std::vector<std::string> ip_addresses;
    std::string colink_ip;
    http_server::get_dev_mac_addr(mac_addresses);
    http_server::get_dev_ip_addrs(ip_addresses, colink_ip);

    auto http_log_handler = [this](http_server::LogLevel level, const char * msg) {
        switch (level) {
          case http_server::LogLevel::Debug:
            ROS_DEBUG("[HTTP_SERVER] %s", msg);
            break;
          case http_server::LogLevel::Info:
            ROS_INFO("[HTTP_SERVER] %s", msg);
            break;
          case http_server::LogLevel::Warn:
            ROS_WARN("[HTTP_SERVER] %s", msg);
            break;
          case http_server::LogLevel::Error:
            ROS_ERROR("[HTTP_SERVER] %s", msg);
            break;
          case http_server::LogLevel::Fatal:
            ROS_FATAL("[HTTP_SERVER] %s", msg);
            break;
        }
      };

    http_server_ = std::unique_ptr<http_server::HttpServer>(
      new http_server::HttpServer(21275, mac_addresses, ip_addresses, http_log_handler));
    http_server_->start();

    try {
      cobridge_base::ServerOptions server_options;
      server_options.capabilities = capabilities_;
      if (use_sim_time_) {
        server_options.capabilities.emplace_back(cobridge_base::CAPABILITY_TIME);
      }
      server_options.capabilities.emplace_back(cobridge_base::CAPABILITY_MESSAGE_TIME);
      server_options.supported_encodings = {ROS1_CHANNEL_ENCODING};
      server_options.metadata = {{"ROS_DISTRO", ros_distro}, {"COLINK", colink_ip}};
      server_options.send_buffer_limit_bytes = send_buffer_limit;
      server_options.session_id = session_id;
      server_options.use_tls = use_tls;
      server_options.cert_file = certfile;
      server_options.key_file = keyfile;
      server_options.use_compression = use_compression;
      server_options.client_topic_whitelist_patterns = client_topic_whitelist_patterns;
      server_options.mac_addr = mac_addresses;
      server_options.ip_addrs = ip_addresses;

      const auto log_handler =
        std::bind(&CoBridge::log_handler, this, std::placeholders::_1, std::placeholders::_2);

      // Fetching of assets may be blocking, hence we fetch them in a separate thread.
      // fetch_asset_queue_ =
      //   std::make_unique<cobridge_base::CallbackQueue>(log_handler, 1 /* num_threads */);
      fetch_asset_queue_ =
        std::unique_ptr<cobridge_base::CallbackQueue>(
        new cobridge_base::CallbackQueue(log_handler, 1 /* num_threads */));

      _server = cobridge_base::ServerFactory::create_server<ConnectionHandle>(
        "cobridge",
        log_handler, server_options);
      cobridge_base::ServerHandlers<ConnectionHandle> hdlrs;
      hdlrs.subscribe_handler = std::bind(
        &CoBridge::subscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.unsubscribe_handler = std::bind(
        &CoBridge::unsubscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_advertise_handler = std::bind(
        &CoBridge::client_advertise, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_unadvertise_handler = std::bind(
        &CoBridge::client_unadvertise, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_message_handler = std::bind(
        &CoBridge::client_message, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.parameter_request_handler = std::bind(
        &CoBridge::get_parameters, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameter_change_handler = std::bind(
        &CoBridge::set_parameters, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameter_subscription_handler = std::bind(
        &CoBridge::subscribe_parameters, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
      hdlrs.service_request_handler = std::bind(
        &CoBridge::service_request, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.subscribe_connection_graph_handler = [this](bool subscribe) {
          subscribe_graph_updates_ = subscribe;
        };

      if (has_capability(cobridge_base::CAPABILITY_ASSETS)) {
        hdlrs.fetch_asset_handler = [this](const std::string & uri, uint32_t requestId,
            cobridge_base::ConnHandle hdl) {
            fetch_asset_queue_->add_callback(
              std::bind(&CoBridge::fetch_asset, this, uri, requestId, hdl));
          };
      }

      _server->set_handlers(std::move(hdlrs));

      _server->start(address, static_cast<uint16_t>(port));

      xmlrpc_server_.bind(
        "paramUpdate", std::bind(
          &CoBridge::parameter_updates, this,
          std::placeholders::_1, std::placeholders::_2));
      xmlrpc_server_.start();

      update_advertised_topics_and_services(ros::TimerEvent());

      if (use_sim_time_) {
        clock_subscription_ = getMTNodeHandle().subscribe<rosgraph_msgs::Clock>(
          "/clock", 10, [&](const rosgraph_msgs::Clock::ConstPtr msg) {
            _server->broadcast_time(msg->clock.toNSec());
          });
      }
    } catch (const std::exception & err) {
      ROS_ERROR("Failed to start websocket server: %s", err.what());
      // Rethrow exception such that the nodelet is unloaded.
      throw err;
    }
  }
  ~CoBridge() override
  {
    xmlrpc_server_.shutdown();
    if (_server) {
      _server->stop();
    }

    if (http_server_) {
      http_server_->stop();
    }
  }

private:
  struct PairHash
  {
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> & pair) const
    {
      return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
  };

  void subscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle)
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    auto it = advertised_topics_.find(channel_id);
    if (it == advertised_topics_.end()) {
      const std::string err_msg =
        "Received subscribe request for unknown channel " + std::to_string(channel_id);
      ROS_WARN_STREAM(err_msg);
      throw cobridge_base::ChannelError(channel_id, err_msg);
    }

    const auto & channel = it->second;
    const auto & topic = channel.topic;
    const auto & datatype = channel.schema_name;

    // Get client subscriptions for this channel or insert an empty map.
    // auto [subscriptions_it, first_subscription] =
    auto sub_elem =
      subscriptions_.emplace(channel_id, SubscriptionsByClient());
    auto & subscriptions_by_client = sub_elem.first->second;

    if (!sub_elem.second &&
      subscriptions_by_client.find(client_handle) != subscriptions_by_client.end())
    {
      const std::string err_msg =
        "Client is already subscribed to channel " + std::to_string(channel_id);
      ROS_WARN_STREAM(err_msg);
      throw cobridge_base::ChannelError(channel_id, err_msg);
    }

    try {
      subscriptions_by_client.emplace(
        client_handle, getMTNodeHandle().subscribe<ros_babel_fish::BabelFishMessage>(
          topic, SUBSCRIPTION_QUEUE_LENGTH,
          std::bind(
            &CoBridge::ros_message_handler, this, channel_id, client_handle,
            std::placeholders::_1)));
      if (sub_elem.second) {
        ROS_INFO(
          "Subscribed to topic \"%s\" (%s) on channel %d", topic.c_str(), datatype.c_str(),
          channel_id);

      } else {
        ROS_INFO(
          "Added subscriber #%zu to topic \"%s\" (%s) on channel %d",
          subscriptions_by_client.size(), topic.c_str(), datatype.c_str(), channel_id);
      }
    } catch (const std::exception & ex) {
      const std::string err_msg =
        "Failed to subscribe to topic '" + topic + "' (" + datatype + "): " + ex.what();
      ROS_ERROR_STREAM(err_msg);
      throw cobridge_base::ChannelError(channel_id, err_msg);
    }
  }

  void unsubscribe(cobridge_base::ChannelId channel_id, ConnectionHandle client_handle)
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    const auto channel_it = advertised_topics_.find(channel_id);
    if (channel_it == advertised_topics_.end()) {
      const std::string err_msg =
        "Received unsubscribe request for unknown channel " + std::to_string(channel_id);
      ROS_WARN_STREAM(err_msg);
      throw cobridge_base::ChannelError(channel_id, err_msg);
    }
    const auto & channel = channel_it->second;

    auto subscriptions_it = subscriptions_.find(channel_id);
    if (subscriptions_it == subscriptions_.end()) {
      throw cobridge_base::ChannelError(
              channel_id, "Received unsubscribe request for channel " +
              std::to_string(channel_id) +
              " that was not subscribed to ");
    }

    auto & subscriptions_by_client = subscriptions_it->second;
    const auto client_subscription = subscriptions_by_client.find(client_handle);
    if (client_subscription == subscriptions_by_client.end()) {
      throw cobridge_base::ChannelError(
              channel_id, "Received unsubscribe request for channel " + std::to_string(channel_id) +
              "from a client that was not subscribed to this channel");
    }

    subscriptions_by_client.erase(client_subscription);
    if (subscriptions_by_client.empty()) {
      ROS_INFO(
        "Unsubscribing from topic \"%s\" (%s) on channel %d", channel.topic.c_str(),
        channel.schema_name.c_str(), channel_id);
      subscriptions_.erase(subscriptions_it);
    } else {
      ROS_INFO(
        "Removed one subscription from channel %d (%zu subscription(s) left)", channel_id,
        subscriptions_by_client.size());
    }
  }

  void client_advertise(
    const cobridge_base::ClientAdvertisement & channel,
    ConnectionHandle client_handle)
  {
    if (channel.encoding != ROS1_CHANNEL_ENCODING) {
      throw cobridge_base::ClientChannelError(
              channel.channel_id, "Unsupported encoding. Only '" + std::string(
                ROS1_CHANNEL_ENCODING) +
              "' encoding is supported at the moment.");
    }

    std::lock_guard<std::mutex> lock(publications_mutex_);

    // Get client publications or insert an empty map.
    // auto [client_publications_it, is_first_publication] =
    auto advertised_topic =
      client_advertised_topics_.emplace(client_handle, ClientPublications());

    auto & client_publications = advertised_topic.first->second;
    if (!advertised_topic.second &&
      client_publications.find(channel.channel_id) != client_publications.end())
    {
      throw cobridge_base::ClientChannelError(
              channel.channel_id, "Received client advertisement from " +
              _server->remote_endpoint_string(client_handle) + " for channel " +
              std::to_string(channel.channel_id) + " it had already advertised");
    }

    const auto message_description =
      ros_type_info_provider_.getMessageDescription(channel.schema_name);
    if (!message_description) {
      throw cobridge_base::ClientChannelError(
              channel.channel_id, "Failed to retrieve type information of data type '" +
              channel.schema_name + "'. Unable to advertise topic " + channel.topic);
    }

    ros::AdvertiseOptions advertise_options;
    advertise_options.datatype = channel.schema_name;
    advertise_options.has_header = false;
    advertise_options.latch = false;
    advertise_options.md5sum = message_description->md5;
    advertise_options.message_definition = message_description->message_definition;
    advertise_options.queue_size = PUBLICATION_QUEUE_LENGTH;
    advertise_options.topic = channel.topic;
    auto publisher = getMTNodeHandle().advertise(advertise_options);

    if (publisher) {
      client_publications.insert({channel.channel_id, std::move(publisher)});
      ROS_INFO(
        "Client %s is advertising \"%s\" (%s) on channel %d",
        _server->remote_endpoint_string(client_handle).c_str(), channel.topic.c_str(),
        channel.schema_name.c_str(), channel.channel_id);
    } else {
      const auto err_msg =
        "Failed to create publisher for topic " + channel.topic + "(" + channel.schema_name + ")";
      ROS_ERROR_STREAM(err_msg);
      throw cobridge_base::ClientChannelError(channel.channel_id, err_msg);
    }
  }

  void client_unadvertise(cobridge_base::ClientChannelId channel_id, ConnectionHandle client_handle)
  {
    std::lock_guard<std::mutex> lock(publications_mutex_);

    auto client_publications_it = client_advertised_topics_.find(client_handle);
    if (client_publications_it == client_advertised_topics_.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id, "Ignoring client unadvertisement from " +
              _server->remote_endpoint_string(client_handle) + " for unknown channel " +
              std::to_string(channel_id) + ", client has no advertised topics");
    }

    auto & client_publications = client_publications_it->second;

    auto channel_publication_it = client_publications.find(channel_id);
    if (channel_publication_it == client_publications.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id, "Ignoring client unadvertisement from " +
              _server->remote_endpoint_string(client_handle) + " for unknown channel " +
              std::to_string(channel_id) + ", client has " +
              std::to_string(client_publications.size()) + " advertised topic(s)");
    }

    const auto & publisher = channel_publication_it->second;
    ROS_INFO(
      "Client %s is no longer advertising %s (%d subscribers) on channel %d",
      _server->remote_endpoint_string(client_handle).c_str(), publisher.getTopic().c_str(),
      publisher.getNumSubscribers(), channel_id);
    client_publications.erase(channel_publication_it);

    if (client_publications.empty()) {
      client_advertised_topics_.erase(client_publications_it);
    }
  }

  void client_message(
    const cobridge_base::ClientMessage & client_msg,
    ConnectionHandle client_handle)
  {
    ros_babel_fish::BabelFishMessage::Ptr msg(new ros_babel_fish::BabelFishMessage);
    msg->read(client_msg);

    const auto channel_id = client_msg.advertisement.channel_id;
    std::lock_guard<std::mutex> lock(publications_mutex_);

    auto client_publications_it = client_advertised_topics_.find(client_handle);
    if (client_publications_it == client_advertised_topics_.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id,
              "Dropping client message from " + _server->remote_endpoint_string(client_handle) +
              " for unknown channel " + std::to_string(channel_id) +
              ", client has no advertised topics");
    }

    auto & client_publications = client_publications_it->second;

    auto channel_publication_it = client_publications.find(client_msg.advertisement.channel_id);
    if (channel_publication_it == client_publications.end()) {
      throw cobridge_base::ClientChannelError(
              channel_id,
              "Dropping client message from " + _server->remote_endpoint_string(client_handle) +
              " for unknown channel " + std::to_string(channel_id) + ", client has " +
              std::to_string(client_publications.size()) + " advertised topic(s)");
    }

    try {
      channel_publication_it->second.publish(msg);
    } catch (const std::exception & ex) {
      throw cobridge_base::ClientChannelError(
              channel_id, "Failed to publish message on topic '" +
              channel_publication_it->second.getTopic() +
              "': " + ex.what());
    }
  }

  void update_advertised_topics_and_services(const ros::TimerEvent &)
  {
    update_timer_.stop();
    if (!ros::ok()) {
      return;
    }

    const bool services_enabled = has_capability(cobridge_base::CAPABILITY_SERVICES);
    const bool query_system_state = services_enabled || subscribe_graph_updates_;

    std::vector<std::string> service_names;
    cobridge_base::MapOfSets publishers, subscribers, services;

    // Retrieve system state from ROS master.
    if (query_system_state) {
      XmlRpc::XmlRpcValue params, result, payload;
      params[0] = this->getName();
      if (ros::master::execute("getSystemState", params, result, payload, false) &&
        static_cast<int>(result[0]) == 1)
      {
        const auto & system_state = result[2];
        const auto & publishers_xml_rpc = system_state[0];
        const auto & subscribers_xml_rpc = system_state[1];
        const auto & services_xml_rpc = system_state[2];

        for (int i = 0; i < services_xml_rpc.size(); ++i) {
          XmlRpc::XmlRpcValue name_value = services_xml_rpc[i][0];
          const std::string name = static_cast<std::string>(name_value);
          if (is_whitelisted(name, service_whitelist_patterns)) {
            service_names.push_back(name);
            services.emplace(name, rpc_value_to_string_set(services_xml_rpc[i][1]));
          }
        }
        for (int i = 0; i < publishers_xml_rpc.size(); ++i) {
          XmlRpc::XmlRpcValue name_value = publishers_xml_rpc[i][0];
          const std::string name = static_cast<std::string>(name_value);
          if (is_whitelisted(name, topic_whitelist_patterns_)) {
            publishers.emplace(name, rpc_value_to_string_set(publishers_xml_rpc[i][1]));
          }
        }
        for (int i = 0; i < subscribers_xml_rpc.size(); ++i) {
          XmlRpc::XmlRpcValue name_value = subscribers_xml_rpc[i][0];
          const std::string name = static_cast<std::string>(name_value);
          if (is_whitelisted(name, topic_whitelist_patterns_)) {
            subscribers.emplace(name, rpc_value_to_string_set(subscribers_xml_rpc[i][1]));
          }
        }
      } else {
        ROS_WARN("Failed to call getSystemState: %s", result.toXml().c_str());
      }
    }

    update_advertised_topics();
    if (services_enabled) {
      update_advertised_services(service_names);
    }
    if (subscribe_graph_updates_) {
      _server->update_connection_graph(publishers, subscribers, services);
    }

    // Schedule the next update using truncated exponential backoff, between `MIN_UPDATE_PERIOD_MS`
    // and `_maxUpdateMs`
    update_count_++;
    const auto next_update_ms = std::max(
      MIN_UPDATE_PERIOD_MS, static_cast<double>(std::min(
        size_t(
          1) << update_count_, max_update_ms_)));
    update_timer_ = getMTNodeHandle().createTimer(
      ros::Duration(next_update_ms / 1e3), &CoBridge::update_advertised_topics_and_services, this);
  }

  void update_advertised_topics()
  {
    // Get the current list of visible topics and datatypes from the ROS graph
    std::vector<ros::master::TopicInfo> topic_names_and_types;
    if (!ros::master::getTopics(topic_names_and_types)) {
      ROS_WARN("Failed to retrieve published topics from ROS master.");
      return;
    }
    std::unordered_set<TopicAndDatatype, PairHash> latest_topics;
    latest_topics.reserve(topic_names_and_types.size());
    for (const auto & topic_name_and_type : topic_names_and_types) {
      const auto & topic_name = topic_name_and_type.name;
      const auto & datatype = topic_name_and_type.datatype;

      // Ignore the topic if it is not on the topic whitelist
      try {
        if (is_whitelisted(topic_name, topic_whitelist_patterns_)) {
          latest_topics.emplace(topic_name, datatype);
        }
      } catch (const std::regex_error & ex) {
        ROS_WARN("Regex error when checking topic '%s': %s", topic_name.c_str(), ex.what());
        continue;
      } catch (const std::exception & ex) {
        ROS_WARN("Error when checking topic '%s': %s", topic_name.c_str(), ex.what());
        continue;
      }
    }

    if (const auto num_ignored_topics = topic_names_and_types.size() - latest_topics.size()) {
      ROS_INFO(
        "%zu topics have been ignored as they do not match any pattern on the topic whitelist",
        num_ignored_topics);
    }

    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    // Remove channels for which the topic does not exist anymore
    std::vector<cobridge_base::ChannelId> channel_ids_to_remove;
    for (auto channel_it = advertised_topics_.begin(); channel_it != advertised_topics_.end(); ) {
      const TopicAndDatatype topic_and_datatype = {channel_it->second.topic,
        channel_it->second.schema_name};
      if (latest_topics.find(topic_and_datatype) == latest_topics.end()) {
        const auto channel_id = channel_it->first;
        channel_ids_to_remove.push_back(channel_id);
        subscriptions_.erase(channel_id);
        ROS_INFO(
          "Removed channel %d for topic \"%s\" (%s)", channel_id,
          topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str());
        channel_it = advertised_topics_.erase(channel_it);
      } else {
        channel_it++;
      }
    }
    _server->remove_channels(channel_ids_to_remove);

    // Add new channels for new topics
    std::vector<cobridge_base::ChannelWithoutId> channels_to_add;
    for (const auto & topic_and_datatype : latest_topics) {
      if (std::find_if(
          advertised_topics_.begin(), advertised_topics_.end(),
          [topic_and_datatype](const std::pair<cobridge_base::ChannelId,
          cobridge_base::ChannelWithoutId> & channel_id_and_channel) {
            const auto & channel = channel_id_and_channel.second;
            return channel.topic == topic_and_datatype.first &&
            channel.schema_name == topic_and_datatype.second;
          }) != advertised_topics_.end())
      {
        continue;  // Topic already advertised
      }

      try {
        cobridge_base::ChannelWithoutId new_channel;
        new_channel.topic = topic_and_datatype.first;
        new_channel.schema_name = topic_and_datatype.second;
        new_channel.encoding = ROS1_CHANNEL_ENCODING;

        try {
          const auto msg_description =
            ros_type_info_provider_.getMessageDescription(topic_and_datatype.second);
          if (msg_description) {
            new_channel.schema = msg_description->message_definition;
          } else {
            ROS_WARN("Could not find definition for type %s", topic_and_datatype.second.c_str());
            // We still advertise the channel, but with an empty schema
            new_channel.schema = "";
          }
        } catch (const std::regex_error & ex) {
          ROS_WARN(
            "Regex error when getting message description for topic \"%s\" (%s): %s",
            topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str(), ex.what());
          new_channel.schema = "";
        } catch (const std::exception & err) {
          ROS_WARN(
            "Failed to get message description for topic \"%s\" (%s): %s",
            topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str(), err.what());
          new_channel.schema = "";
        }

        channels_to_add.push_back(std::move(new_channel));
      } catch (const std::regex_error & ex) {
        ROS_WARN(
          "Failed to add channel for topic \"%s\" (%s): regex_error - %s",
          topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str(), ex.what());
        continue;
      } catch (const std::exception & err) {
        ROS_WARN(
          "Failed to add channel for topic \"%s\" (%s): %s",
          topic_and_datatype.first.c_str(), topic_and_datatype.second.c_str(), err.what());
        continue;
      }
    }

    const auto channel_ids = _server->add_channels(channels_to_add);
    for (size_t i = 0; i < channels_to_add.size(); ++i) {
      const auto channel_id = channel_ids[i];
      const auto & channel = channels_to_add[i];
      advertised_topics_.emplace(channel_id, channel);
      ROS_INFO(
        "Advertising channel %d for topic \"%s\" (%s)", channel_id, channel.topic.c_str(),
        channel.schema_name.c_str());
    }
  }

  void update_advertised_services(const std::vector<std::string> & service_names)
  {
    std::lock_guard<std::mutex> lock(services_mutex_);

    // Remove advertisements for services that have been removed
    std::vector<cobridge_base::ServiceId> services_to_remove;
    for (const auto & service : advertised_services_) {
      const auto it =
        std::find_if(
        service_names.begin(), service_names.end(), [service](const std::string & service_name) {
          return service_name == service.second.name;
        });
      if (it == service_names.end()) {
        services_to_remove.push_back(service.first);
      }
    }
    for (auto service_id : services_to_remove) {
      advertised_services_.erase(service_id);
    }
    _server->remove_services(services_to_remove);

    // Advertise new services
    std::vector<cobridge_base::ServiceWithoutId> new_services;
    for (const auto & service_name : service_names) {
      if (std::find_if(
          advertised_services_.begin(), advertised_services_.end(),
          [&service_name](const std::pair<cobridge_base::ServiceId,
          cobridge_base::ServiceWithoutId> & id_with_service) {
            return id_with_service.second.name == service_name;
          }) != advertised_services_.end())
      {
        continue;  // Already advertised
      }

      try {
        const auto service_type =
          retrieve_service_type(
          service_name,
          std::chrono::milliseconds(service_retrieval_timeout_ms_));
        const auto srv_description = ros_type_info_provider_.getServiceDescription(service_type);

        cobridge_base::ServiceWithoutId service;
        service.name = service_name;
        service.type = service_type;

        if (srv_description) {
          service.request_schema = srv_description->request->message_definition;
          service.response_schema = srv_description->response->message_definition;
        } else {
          ROS_WARN(
            "Failed to retrieve type information for service '%s' of type '%s'",
            service_name.c_str(), service_type.c_str());

          // We still advertise the channel, but with empty schema.
          service.request_schema = "";
          service.response_schema = "";
        }
        new_services.push_back(service);
      } catch (const std::exception & e) {
        ROS_WARN(
          "Failed to retrieve service type or service description of service %s: %s",
          service_name.c_str(), e.what());
        continue;
      }
    }

    const auto service_ids = _server->add_services(new_services);
    for (size_t i = 0; i < service_ids.size(); ++i) {
      advertised_services_.emplace(service_ids[i], new_services[i]);
    }
  }

  void get_parameters(
    const std::vector<std::string> & parameters,
    const optional<std::string> & request_id, ConnectionHandle hdl)
  {
    const bool all_parameters_requested = parameters.empty();
    std::vector<std::string> parameter_names = parameters;
    if (all_parameters_requested) {
      if (!getMTNodeHandle().getParamNames(parameter_names)) {
        const auto err_msg = "Failed to retrieve parameter names";
        ROS_ERROR_STREAM(err_msg);
        throw std::runtime_error(err_msg);
      }
    }

    bool success = true;
    std::vector<cobridge_base::Parameter> params;
    for (const auto & param_name : parameter_names) {
      if (!is_whitelisted(param_name, param_whitelist_patterns)) {
        if (all_parameters_requested) {
          continue;
        } else {
          ROS_ERROR("Parameter '%s' is not on the allowlist", param_name.c_str());
          success = false;
        }
      }

      try {
        XmlRpc::XmlRpcValue value;
        getMTNodeHandle().getParam(param_name, value);
        params.push_back(from_ros_param(param_name, value));
      } catch (const std::exception & ex) {
        ROS_ERROR("Invalid parameter '%s': %s", param_name.c_str(), ex.what());
        success = false;
      } catch (const XmlRpc::XmlRpcException & ex) {
        ROS_ERROR("Invalid parameter '%s': %s", param_name.c_str(), ex.getMessage().c_str());
        success = false;
      } catch (...) {
        ROS_ERROR("Invalid parameter '%s'", param_name.c_str());
        success = false;
      }
    }

    _server->publish_parameter_values(hdl, params, request_id);

    if (!success) {
      throw std::runtime_error("Failed to retrieve one or multiple parameters");
    }
  }

  void set_parameters(
    const std::vector<cobridge_base::Parameter> & parameters,
    const optional<std::string> & request_id, ConnectionHandle hdl)
  {
    using cobridge_base::ParameterType;
    auto nh = this->getMTNodeHandle();

    bool success = true;
    for (const auto & param : parameters) {
      const auto param_name = param.get_name();
      if (!is_whitelisted(param_name, param_whitelist_patterns)) {
        ROS_ERROR("Parameter '%s' is not on the allowlist", param_name.c_str());
        success = false;
        continue;
      }

      try {
        const auto param_type = param.get_type();
        const auto param_value = param.get_value();
        if (param_type == ParameterType::PARAMETER_NOT_SET) {
          nh.deleteParam(param_name);
        } else {
          nh.setParam(param_name, to_ros_param(param_value));
        }
      } catch (const std::exception & ex) {
        ROS_ERROR("Failed to set parameter '%s': %s", param_name.c_str(), ex.what());
        success = false;
      } catch (const XmlRpc::XmlRpcException & ex) {
        ROS_ERROR("Failed to set parameter '%s': %s", param_name.c_str(), ex.getMessage().c_str());
        success = false;
      } catch (...) {
        ROS_ERROR("Failed to set parameter '%s'", param_name.c_str());
        success = false;
      }
    }

    // If a request Id was given, send potentially updated parameters back to client
    if (request_id.has_value()) {
      std::vector<std::string> parameter_names(parameters.size());
      for (size_t i = 0; i < parameters.size(); ++i) {
        parameter_names[i] = parameters[i].get_name();
      }
      get_parameters(parameter_names, request_id, hdl);
    }

    if (!success) {
      throw std::runtime_error("Failed to set one or multiple parameters");
    }
  }

  void subscribe_parameters(
    const std::vector<std::string> & parameters,
    cobridge_base::ParameterSubscriptionOperation op, ConnectionHandle)
  {
    const auto op_verb =
      (op ==
      cobridge_base::ParameterSubscriptionOperation::SUBSCRIBE) ? "subscribe" : "unsubscribe";
    bool success = true;
    for (const auto & param_name : parameters) {
      if (!is_whitelisted(param_name, param_whitelist_patterns)) {
        ROS_ERROR("Parameter '%s' is not allowlist", param_name.c_str());
        continue;
      }

      XmlRpc::XmlRpcValue params, result, payload;
      params[0] = getName() + "2";
      params[1] = xmlrpc_server_.getServerURI();
      params[2] = ros::names::resolve(param_name);

      const std::string op_name = std::string(op_verb) + "Param";
      if (ros::master::execute(op_name, params, result, payload, false)) {
        ROS_INFO("%s '%s'", op_name.c_str(), param_name.c_str());
      } else {
        ROS_WARN("Failed to %s '%s': %s", op_verb, param_name.c_str(), result.toXml().c_str());
        success = false;
      }
    }

    if (!success) {
      throw std::runtime_error(
              "Failed to " + std::string(op_verb) +
              " one or multiple parameters.");
    }
  }

  void parameter_updates(XmlRpc::XmlRpcValue & params, XmlRpc::XmlRpcValue & result)
  {
    result[0] = 1;
    result[1] = std::string("");
    result[2] = 0;

    if (params.size() != 3) {
      ROS_ERROR("Parameter update called with invalid parameter size: %d", params.size());
      return;
    }

    try {
      const std::string param_name = ros::names::clean(params[1]);
      const XmlRpc::XmlRpcValue param_value = params[2];
      const auto param = from_ros_param(param_name, param_value);
      _server->update_parameter_values({param});
    } catch (const std::exception & ex) {
      ROS_ERROR("Failed to update parameter: %s", ex.what());
    } catch (const XmlRpc::XmlRpcException & ex) {
      ROS_ERROR("Failed to update parameter: %s", ex.getMessage().c_str());
    } catch (...) {
      ROS_ERROR("Failed to update parameter");
    }
  }

  void log_handler(cobridge_base::WebSocketLogLevel level, char const * msg)
  {
    switch (level) {
      case cobridge_base::WebSocketLogLevel::Debug:
        ROS_DEBUG("[WS] %s", msg);
        break;
      case cobridge_base::WebSocketLogLevel::Info:
        ROS_INFO("[WS] %s", msg);
        break;
      case cobridge_base::WebSocketLogLevel::Warn:
        ROS_WARN("[WS] %s", msg);
        break;
      case cobridge_base::WebSocketLogLevel::Error:
        ROS_ERROR("[WS] %s", msg);
        break;
      case cobridge_base::WebSocketLogLevel::Critical:
        ROS_FATAL("[WS] %s", msg);
        break;
    }
  }

  void ros_message_handler(
    const cobridge_base::ChannelId channel_id, ConnectionHandle client_handle,
    const ros::MessageEvent<ros_babel_fish::BabelFishMessage const> & msg_event)
  {
    const auto & msg = msg_event.getConstMessage();
    _server->send_message(
      client_handle, channel_id,
      msg_event.getReceiptTime().toNSec(),
      msg->buffer(),
      msg->size());
  }

  void service_request(
    const cobridge_base::ServiceRequest & request,
    ConnectionHandle client_handle)
  {
    std::lock_guard<std::mutex> lock(services_mutex_);
    const auto service_it = advertised_services_.find(request.service_id);
    if (service_it == advertised_services_.end()) {
      const auto err_msg =
        "Service with id " + std::to_string(request.service_id) + " does not exist";
      ROS_ERROR_STREAM(err_msg);
      throw cobridge_base::ServiceError(request.service_id, err_msg);
    }
    const auto & service_name = service_it->second.name;
    const auto & service_type = service_it->second.type;
    ROS_INFO(
      "Received a service request for service %s (%s)", service_name.c_str(),
      service_type.c_str());

    if (!ros::service::exists(service_name, false)) {
      throw cobridge_base::ServiceError(
              request.service_id,
              "Service '" + service_name + "' does not exist");
    }

    const auto srv_description = ros_type_info_provider_.getServiceDescription(service_type);
    if (!srv_description) {
      const auto err_msg =
        "Failed to retrieve type information for service " + service_name + "(" + service_type +
        ")";
      ROS_ERROR_STREAM(err_msg);
      throw cobridge_base::ServiceError(request.service_id, err_msg);
    }

    GenericService gen_req, gen_res;
    gen_req.type = gen_res.type = service_type;
    gen_req.md5sum = gen_res.md5sum = srv_description->md5;
    gen_req.data = request.serv_data;

    if (ros::service::call(service_name, gen_req, gen_res)) {
      cobridge_base::ServiceResponse res;
      res.service_id = request.service_id;
      res.call_id = request.call_id;
      res.encoding = request.encoding;
      res.serv_data = gen_res.data;
      _server->send_service_response(client_handle, res);
    } else {
      throw cobridge_base::ServiceError(
              request.service_id,
              "Failed to call service " + service_name + "(" + service_type + ")");
    }
  }

  void fetch_asset(const std::string & uri, uint32_t request_id, ConnectionHandle client_handle)
  {
    cobridge_base::FetchAssetResponse response;
    response.request_id = request_id;

    try {
      // We reject URIs that are not on the allowlist or that contain two consecutive dots. The
      // latter can be utilized to construct URIs for retrieving confidential files that should not
      // be accessible over the WebSocket connection. Example:
      // `package://<pkg_name>/../../../secret.txt`. This is an extra security measure and should
      // not be necessary if the allowlist is strict enough.
      if (uri.find("..") != std::string::npos ||
        !is_whitelisted(uri, asset_uri_allowlist_patterns_))
      {
        throw std::runtime_error("Asset URI not allowed: " + uri);
      }

      resource_retriever::Retriever resource_retriever;
      const resource_retriever::MemoryResource memory_resource = resource_retriever.get(uri);
      response.status = cobridge_base::FetchAssetStatus::Success;
      response.error_message = "";
      response.data.resize(memory_resource.size);
      std::memcpy(response.data.data(), memory_resource.data.get(), memory_resource.size);
    } catch (const std::exception & ex) {
      ROS_WARN("Failed to retrieve asset '%s': %s", uri.c_str(), ex.what());
      response.status = cobridge_base::FetchAssetStatus::Error;
      response.error_message = "Failed to retrieve asset " + uri;
    }

    if (_server) {
      _server->send_fetch_asset_response(client_handle, response);
    }
  }

  bool has_capability(const std::string & capability)
  {
    return std::find(capabilities_.begin(), capabilities_.end(), capability) != capabilities_.end();
  }

  std::unique_ptr<cobridge_base::ServerInterface<ConnectionHandle>> _server;
  ros_babel_fish::IntegratedDescriptionProvider ros_type_info_provider_;
  std::vector<std::regex> topic_whitelist_patterns_;
  std::vector<std::regex> param_whitelist_patterns;
  std::vector<std::regex> service_whitelist_patterns;
  std::vector<std::regex> asset_uri_allowlist_patterns_;
  ros::XMLRPCManager xmlrpc_server_;
  std::unordered_map<cobridge_base::ChannelId, cobridge_base::ChannelWithoutId> advertised_topics_;
  std::unordered_map<cobridge_base::ChannelId, SubscriptionsByClient> subscriptions_;
  std::unordered_map<cobridge_base::ServiceId,
    cobridge_base::ServiceWithoutId> advertised_services_;
  PublicationsByClient client_advertised_topics_;
  std::mutex subscriptions_mutex_;
  std::mutex publications_mutex_;
  std::mutex services_mutex_;
  ros::Timer update_timer_;
  size_t max_update_ms_ = size_t(DEFAULT_MAX_UPDATE_MS);
  size_t update_count_ = 0;
  ros::Subscriber clock_subscription_;
  bool use_sim_time_ = false;
  std::vector<std::string> capabilities_;
  int service_retrieval_timeout_ms_ = DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS;
  std::atomic<bool> subscribe_graph_updates_{false};
  std::unique_ptr<cobridge_base::CallbackQueue> fetch_asset_queue_;

  std::unique_ptr<http_server::HttpServer> http_server_;
};

}  // namespace cobridge

PLUGINLIB_EXPORT_CLASS(cobridge::CoBridge, nodelet::Nodelet)
