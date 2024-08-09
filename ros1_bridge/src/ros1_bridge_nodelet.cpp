#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <nodelet/nodelet.h>
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

namespace {

inline std::unordered_set<std::string> rpcValueToStringSet(const XmlRpc::XmlRpcValue& v) {
  std::unordered_set<std::string> set;
  for (int i = 0; i < v.size(); ++i) {
    set.insert(v[i]);
  }
  return set;
}

}  // namespace

namespace cobridge {

constexpr int DEFAULT_PORT = 8765;
constexpr char DEFAULT_ADDRESS[] = "0.0.0.0";
constexpr int DEFAULT_MAX_UPDATE_MS = 5000;
constexpr char ROS1_CHANNEL_ENCODING[] = "ros1";
constexpr uint32_t SUBSCRIPTION_QUEUE_LENGTH = 10;
constexpr double MIN_UPDATE_PERIOD_MS = 100.0;
constexpr uint32_t PUBLICATION_QUEUE_LENGTH = 10;
constexpr int DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS = 250;

using ConnectionHandle = websocketpp::connection_hdl;
using TopicAndDatatype = std::pair<std::string, std::string>;
using SubscriptionsByClient = std::map<ConnectionHandle, ros::Subscriber, std::owner_less<>>;
using ClientPublications = std::unordered_map<cobridge::ClientChannelId, ros::Publisher>;
using PublicationsByClient = std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;
using cobridge::is_whitelisted;

class CoBridge : public nodelet::Nodelet {
public:
  CoBridge() = default;
  virtual void onInit() {
    auto& nhp = getPrivateNodeHandle();
    const auto address = nhp.param<std::string>("address", DEFAULT_ADDRESS);
    const int port = nhp.param<int>("port", DEFAULT_PORT);
    const auto send_buffer_limit = static_cast<size_t>(
      nhp.param<int>("send_buffer_limit", cobridge::DEFAULT_SEND_BUFFER_LIMIT_BYTES));
    const auto useTLS = nhp.param<bool>("tls", false);
    const auto certfile = nhp.param<std::string>("certfile", "");
    const auto keyfile = nhp.param<std::string>("keyfile", "");
    _maxUpdateMs = static_cast<size_t>(nhp.param<int>("max_update_ms", DEFAULT_MAX_UPDATE_MS));
    const auto useCompression = nhp.param<bool>("use_compression", false);
    _useSimTime = nhp.param<bool>("/use_sim_time", false);
    const auto sessionId = nhp.param<std::string>("/run_id", std::to_string(std::time(nullptr)));
    _capabilities = nhp.param<std::vector<std::string>>(
      "capabilities", std::vector<std::string>(cobridge::DEFAULT_CAPABILITIES.begin(),
                                               cobridge::DEFAULT_CAPABILITIES.end()));
    _serviceRetrievalTimeoutMs = nhp.param<int>("service_type_retrieval_timeout_ms",
                                                DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS);

    const auto topicWhitelistPatterns =
      nhp.param<std::vector<std::string>>("topic_whitelist", {".*"});
    _topicWhitelistPatterns = parseRegexPatterns(topicWhitelistPatterns);
    if (topicWhitelistPatterns.size() != _topicWhitelistPatterns.size()) {
      ROS_ERROR("Failed to parse one or more topic whitelist patterns");
    }
    const auto paramWhitelist = nhp.param<std::vector<std::string>>("param_whitelist", {".*"});
    _paramWhitelistPatterns = parseRegexPatterns(paramWhitelist);
    if (paramWhitelist.size() != _paramWhitelistPatterns.size()) {
      ROS_ERROR("Failed to parse one or more param whitelist patterns");
    }

    const auto serviceWhitelist = nhp.param<std::vector<std::string>>("service_whitelist", {".*"});
    _serviceWhitelistPatterns = parseRegexPatterns(serviceWhitelist);
    if (serviceWhitelist.size() != _serviceWhitelistPatterns.size()) {
      ROS_ERROR("Failed to parse one or more service whitelist patterns");
    }

    const auto clientTopicWhitelist =
      nhp.param<std::vector<std::string>>("client_topic_whitelist", {".*"});
    const auto clientTopicWhitelistPatterns = parseRegexPatterns(clientTopicWhitelist);
    if (clientTopicWhitelist.size() != clientTopicWhitelistPatterns.size()) {
      ROS_ERROR("Failed to parse one or more service whitelist patterns");
    }

    const auto assetUriAllowlist = nhp.param<std::vector<std::string>>(
      "asset_uri_allowlist",
      {"^package://(?:\\w+/"
       ")*\\w+\\.(?:dae|fbx|glb|gltf|jpeg|jpg|mtl|obj|png|stl|tif|tiff|urdf|webp|xacro)$"});
    _assetUriAllowlistPatterns = parseRegexPatterns(assetUriAllowlist);
    if (assetUriAllowlist.size() != _assetUriAllowlistPatterns.size()) {
      ROS_ERROR("Failed to parse one or more asset URI whitelist patterns");
    }

    const char* rosDistro = std::getenv("ROS_DISTRO");
    ROS_INFO("Starting cobridge (%s, %s@%s) with %s", rosDistro,
             cobridge::COBRIDGE_VERSION, cobridge::COBRIDGE_GIT_HASH,
             cobridge::websocket_user_agent());

    try {
        cobridge::ServerOptions serverOptions;
      serverOptions.capabilities = _capabilities;
      if (_useSimTime) {
        serverOptions.capabilities.push_back(cobridge::CAPABILITY_TIME);
      }
      serverOptions.supported_encodings = {ROS1_CHANNEL_ENCODING};
      serverOptions.metadata = {{"ROS_DISTRO", rosDistro}};
      serverOptions.send_buffer_limit_bytes = send_buffer_limit;
      serverOptions.session_id = sessionId;
      serverOptions.use_tls = useTLS;
      serverOptions.cert_file = certfile;
      serverOptions.key_file = keyfile;
      serverOptions.use_compression = useCompression;
      serverOptions.client_topic_whitelist_patterns = clientTopicWhitelistPatterns;

      const auto logHandler =
        std::bind(&CoBridge::logHandler, this, std::placeholders::_1, std::placeholders::_2);

      // Fetching of assets may be blocking, hence we fetch them in a separate thread.
      _fetchAssetQueue = std::make_unique<cobridge::CallbackQueue>(logHandler, 1 /* num_threads */);

      _server = cobridge::ServerFactory::create_server<ConnectionHandle>("cobridge",
                                                                        logHandler, serverOptions);
        cobridge::ServerHandlers<ConnectionHandle> hdlrs;
      hdlrs.subscribe_handler =
        std::bind(&CoBridge::subscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.unsubscribe_handler =
        std::bind(&CoBridge::unsubscribe, this, std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_advertise_handler = std::bind(&CoBridge::clientAdvertise, this,
                                                 std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_unadvertise_handler = std::bind(&CoBridge::clientUnadvertise, this,
                                                   std::placeholders::_1, std::placeholders::_2);
      hdlrs.client_message_handler = std::bind(&CoBridge::clientMessage, this,
                                               std::placeholders::_1, std::placeholders::_2);
      hdlrs.parameter_request_handler =
        std::bind(&CoBridge::getParameters, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameter_change_handler =
        std::bind(&CoBridge::setParameters, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
      hdlrs.parameter_subscription_handler =
        std::bind(&CoBridge::subscribeParameters, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
      hdlrs.service_request_handler = std::bind(&CoBridge::serviceRequest, this,
                                                std::placeholders::_1, std::placeholders::_2);
      hdlrs.subscribe_connection_graph_handler = [this](bool subscribe) {
        _subscribeGraphUpdates = subscribe;
      };

      if (hasCapability(cobridge::CAPABILITY_ASSETS)) {
        hdlrs.fetch_asset_handler = [this](const std::string& uri, uint32_t requestId,
                                           cobridge::ConnHandle hdl) {
          _fetchAssetQueue->add_callback(
            std::bind(&CoBridge::fetchAsset, this, uri, requestId, hdl));
        };
      }

      _server->set_handlers(std::move(hdlrs));

      _server->start(address, static_cast<uint16_t>(port));

      xmlrpcServer.bind("paramUpdate", std::bind(&CoBridge::parameterUpdates, this,
                                                 std::placeholders::_1, std::placeholders::_2));
      xmlrpcServer.start();

      updateAdvertisedTopicsAndServices(ros::TimerEvent());

      if (_useSimTime) {
        _clockSubscription = getMTNodeHandle().subscribe<rosgraph_msgs::Clock>(
          "/clock", 10, [&](const rosgraph_msgs::Clock::ConstPtr msg) {
            _server->broadcast_time(msg->clock.toNSec());
          });
      }
    } catch (const std::exception& err) {
      ROS_ERROR("Failed to start websocket server: %s", err.what());
      // Rethrow exception such that the nodelet is unloaded.
      throw err;
    }
  };
  virtual ~CoBridge() {
    xmlrpcServer.shutdown();
    if (_server) {
      _server->stop();
    }
  }

private:
  struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& pair) const {
      return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
  };

  void subscribe(cobridge::ChannelId channelId, ConnectionHandle clientHandle) {
    std::lock_guard<std::mutex> lock(_subscriptionsMutex);

    auto it = _advertisedTopics.find(channelId);
    if (it == _advertisedTopics.end()) {
      const std::string errMsg =
        "Received subscribe request for unknown channel " + std::to_string(channelId);
      ROS_WARN_STREAM(errMsg);
      throw cobridge::ChannelError(channelId, errMsg);
    }

    const auto& channel = it->second;
    const auto& topic = channel.topic;
    const auto& datatype = channel.schema_name;

    // Get client subscriptions for this channel or insert an empty map.
    auto [subscriptionsIt, firstSubscription] =
      _subscriptions.emplace(channelId, SubscriptionsByClient());
    auto& subscriptionsByClient = subscriptionsIt->second;

    if (!firstSubscription &&
        subscriptionsByClient.find(clientHandle) != subscriptionsByClient.end()) {
      const std::string errMsg =
        "Client is already subscribed to channel " + std::to_string(channelId);
      ROS_WARN_STREAM(errMsg);
      throw cobridge::ChannelError(channelId, errMsg);
    }

    try {
      subscriptionsByClient.emplace(
        clientHandle, getMTNodeHandle().subscribe<ros_babel_fish::BabelFishMessage>(
                        topic, SUBSCRIPTION_QUEUE_LENGTH,
                        std::bind(&CoBridge::rosMessageHandler, this, channelId, clientHandle,
                                  std::placeholders::_1)));
      if (firstSubscription) {
        ROS_INFO("Subscribed to topic \"%s\" (%s) on channel %d", topic.c_str(), datatype.c_str(),
                 channelId);

      } else {
        ROS_INFO("Added subscriber #%zu to topic \"%s\" (%s) on channel %d",
                 subscriptionsByClient.size(), topic.c_str(), datatype.c_str(), channelId);
      }
    } catch (const std::exception& ex) {
      const std::string errMsg =
        "Failed to subscribe to topic '" + topic + "' (" + datatype + "): " + ex.what();
      ROS_ERROR_STREAM(errMsg);
      throw cobridge::ChannelError(channelId, errMsg);
    }
  }

  void unsubscribe(cobridge::ChannelId channelId, ConnectionHandle clientHandle) {
    std::lock_guard<std::mutex> lock(_subscriptionsMutex);

    const auto channelIt = _advertisedTopics.find(channelId);
    if (channelIt == _advertisedTopics.end()) {
      const std::string errMsg =
        "Received unsubscribe request for unknown channel " + std::to_string(channelId);
      ROS_WARN_STREAM(errMsg);
      throw cobridge::ChannelError(channelId, errMsg);
    }
    const auto& channel = channelIt->second;

    auto subscriptionsIt = _subscriptions.find(channelId);
    if (subscriptionsIt == _subscriptions.end()) {
      throw cobridge::ChannelError(channelId, "Received unsubscribe request for channel " +
                                                std::to_string(channelId) +
                                                " that was not subscribed to ");
    }

    auto& subscriptionsByClient = subscriptionsIt->second;
    const auto clientSubscription = subscriptionsByClient.find(clientHandle);
    if (clientSubscription == subscriptionsByClient.end()) {
      throw cobridge::ChannelError(
        channelId, "Received unsubscribe request for channel " + std::to_string(channelId) +
                     "from a client that was not subscribed to this channel");
    }

    subscriptionsByClient.erase(clientSubscription);
    if (subscriptionsByClient.empty()) {
      ROS_INFO("Unsubscribing from topic \"%s\" (%s) on channel %d", channel.topic.c_str(),
               channel.schema_name.c_str(), channelId);
      _subscriptions.erase(subscriptionsIt);
    } else {
      ROS_INFO("Removed one subscription from channel %d (%zu subscription(s) left)", channelId,
               subscriptionsByClient.size());
    }
  }

  void clientAdvertise(const cobridge::ClientAdvertisement& channel,
                       ConnectionHandle clientHandle) {
    if (channel.encoding != ROS1_CHANNEL_ENCODING) {
      throw cobridge::ClientChannelError(
        channel.channel_id, "Unsupported encoding. Only '" + std::string(ROS1_CHANNEL_ENCODING) +
                             "' encoding is supported at the moment.");
    }

    std::unique_lock<std::shared_mutex> lock(_publicationsMutex);

    // Get client publications or insert an empty map.
    auto [clientPublicationsIt, isFirstPublication] =
      _clientAdvertisedTopics.emplace(clientHandle, ClientPublications());

    auto& clientPublications = clientPublicationsIt->second;
    if (!isFirstPublication &&
        clientPublications.find(channel.channel_id) != clientPublications.end()) {
      throw cobridge::ClientChannelError(
        channel.channel_id, "Received client advertisement from " +
                             _server->remote_endpoint_string(clientHandle) + " for channel " +
                             std::to_string(channel.channel_id) + " it had already advertised");
    }

    const auto msgDescription = _rosTypeInfoProvider.getMessageDescription(channel.schema_name);
    if (!msgDescription) {
      throw cobridge::ClientChannelError(
        channel.channel_id, "Failed to retrieve type information of data type '" +
                             channel.schema_name + "'. Unable to advertise topic " + channel.topic);
    }

    ros::AdvertiseOptions advertiseOptions;
    advertiseOptions.datatype = channel.schema_name;
    advertiseOptions.has_header = false;  // TODO
    advertiseOptions.latch = false;
    advertiseOptions.md5sum = msgDescription->md5;
    advertiseOptions.message_definition = msgDescription->message_definition;
    advertiseOptions.queue_size = PUBLICATION_QUEUE_LENGTH;
    advertiseOptions.topic = channel.topic;
    auto publisher = getMTNodeHandle().advertise(advertiseOptions);

    if (publisher) {
      clientPublications.insert({channel.channel_id, std::move(publisher)});
      ROS_INFO("Client %s is advertising \"%s\" (%s) on channel %d",
               _server->remote_endpoint_string(clientHandle).c_str(), channel.topic.c_str(),
               channel.schema_name.c_str(), channel.channel_id);
    } else {
      const auto errMsg =
        "Failed to create publisher for topic " + channel.topic + "(" + channel.schema_name + ")";
      ROS_ERROR_STREAM(errMsg);
      throw cobridge::ClientChannelError(channel.channel_id, errMsg);
    }
  }

  void clientUnadvertise(cobridge::ClientChannelId channelId, ConnectionHandle clientHandle) {
    std::unique_lock<std::shared_mutex> lock(_publicationsMutex);

    auto clientPublicationsIt = _clientAdvertisedTopics.find(clientHandle);
    if (clientPublicationsIt == _clientAdvertisedTopics.end()) {
      throw cobridge::ClientChannelError(
        channelId, "Ignoring client unadvertisement from " +
                     _server->remote_endpoint_string(clientHandle) + " for unknown channel " +
                     std::to_string(channelId) + ", client has no advertised topics");
    }

    auto& clientPublications = clientPublicationsIt->second;

    auto channelPublicationIt = clientPublications.find(channelId);
    if (channelPublicationIt == clientPublications.end()) {
      throw cobridge::ClientChannelError(
        channelId, "Ignoring client unadvertisement from " +
                     _server->remote_endpoint_string(clientHandle) + " for unknown channel " +
                     std::to_string(channelId) + ", client has " +
                     std::to_string(clientPublications.size()) + " advertised topic(s)");
    }

    const auto& publisher = channelPublicationIt->second;
    ROS_INFO("Client %s is no longer advertising %s (%d subscribers) on channel %d",
             _server->remote_endpoint_string(clientHandle).c_str(), publisher.getTopic().c_str(),
             publisher.getNumSubscribers(), channelId);
    clientPublications.erase(channelPublicationIt);

    if (clientPublications.empty()) {
      _clientAdvertisedTopics.erase(clientPublicationsIt);
    }
  }

  void clientMessage(const cobridge::ClientMessage& clientMsg, ConnectionHandle clientHandle) {
    ros_babel_fish::BabelFishMessage::Ptr msg(new ros_babel_fish::BabelFishMessage);
    msg->read(clientMsg);

    const auto channelId = clientMsg.advertisement.channel_id;
    std::shared_lock<std::shared_mutex> lock(_publicationsMutex);

    auto clientPublicationsIt = _clientAdvertisedTopics.find(clientHandle);
    if (clientPublicationsIt == _clientAdvertisedTopics.end()) {
      throw cobridge::ClientChannelError(
        channelId, "Dropping client message from " + _server->remote_endpoint_string(clientHandle) +
                     " for unknown channel " + std::to_string(channelId) +
                     ", client has no advertised topics");
    }

    auto& clientPublications = clientPublicationsIt->second;

    auto channelPublicationIt = clientPublications.find(clientMsg.advertisement.channel_id);
    if (channelPublicationIt == clientPublications.end()) {
      throw cobridge::ClientChannelError(
        channelId, "Dropping client message from " + _server->remote_endpoint_string(clientHandle) +
                     " for unknown channel " + std::to_string(channelId) + ", client has " +
                     std::to_string(clientPublications.size()) + " advertised topic(s)");
    }

    try {
      channelPublicationIt->second.publish(msg);
    } catch (const std::exception& ex) {
      throw cobridge::ClientChannelError(channelId, "Failed to publish message on topic '" +
                                                      channelPublicationIt->second.getTopic() +
                                                      "': " + ex.what());
    }
  }

  void updateAdvertisedTopicsAndServices(const ros::TimerEvent&) {
    _updateTimer.stop();
    if (!ros::ok()) {
      return;
    }

    const bool servicesEnabled = hasCapability(cobridge::CAPABILITY_SERVICES);
    const bool querySystemState = servicesEnabled || _subscribeGraphUpdates;

    std::vector<std::string> serviceNames;
      cobridge::MapOfSets publishers, subscribers, services;

    // Retrieve system state from ROS master.
    if (querySystemState) {
      XmlRpc::XmlRpcValue params, result, payload;
      params[0] = this->getName();
      if (ros::master::execute("getSystemState", params, result, payload, false) &&
          static_cast<int>(result[0]) == 1) {
        const auto& systemState = result[2];
        const auto& publishersXmlRpc = systemState[0];
        const auto& subscribersXmlRpc = systemState[1];
        const auto& servicesXmlRpc = systemState[2];

        for (int i = 0; i < servicesXmlRpc.size(); ++i) {
          const std::string& name = servicesXmlRpc[i][0];
          if (is_whitelisted(name, _serviceWhitelistPatterns)) {
            serviceNames.push_back(name);
            services.emplace(name, rpcValueToStringSet(servicesXmlRpc[i][1]));
          }
        }
        for (int i = 0; i < publishersXmlRpc.size(); ++i) {
          const std::string& name = publishersXmlRpc[i][0];
          if (is_whitelisted(name, _topicWhitelistPatterns)) {
            publishers.emplace(name, rpcValueToStringSet(publishersXmlRpc[i][1]));
          }
        }
        for (int i = 0; i < subscribersXmlRpc.size(); ++i) {
          const std::string& name = subscribersXmlRpc[i][0];
          if (is_whitelisted(name, _topicWhitelistPatterns)) {
            subscribers.emplace(name, rpcValueToStringSet(subscribersXmlRpc[i][1]));
          }
        }
      } else {
        ROS_WARN("Failed to call getSystemState: %s", result.toXml().c_str());
      }
    }

    updateAdvertisedTopics();
    if (servicesEnabled) {
      updateAdvertisedServices(serviceNames);
    }
    if (_subscribeGraphUpdates) {
      _server->update_connection_graph(publishers, subscribers, services);
    }

    // Schedule the next update using truncated exponential backoff, between `MIN_UPDATE_PERIOD_MS`
    // and `_maxUpdateMs`
    _updateCount++;
    const auto nextUpdateMs = std::max(
      MIN_UPDATE_PERIOD_MS, static_cast<double>(std::min(size_t(1) << _updateCount, _maxUpdateMs)));
    _updateTimer = getMTNodeHandle().createTimer(
            ros::Duration(nextUpdateMs / 1e3), &CoBridge::updateAdvertisedTopicsAndServices, this);
  }

  void updateAdvertisedTopics() {
    // Get the current list of visible topics and datatypes from the ROS graph
    std::vector<ros::master::TopicInfo> topicNamesAndTypes;
    if (!ros::master::getTopics(topicNamesAndTypes)) {
      ROS_WARN("Failed to retrieve published topics from ROS master.");
      return;
    }

    std::unordered_set<TopicAndDatatype, PairHash> latestTopics;
    latestTopics.reserve(topicNamesAndTypes.size());
    for (const auto& topicNameAndType : topicNamesAndTypes) {
      const auto& topicName = topicNameAndType.name;
      const auto& datatype = topicNameAndType.datatype;

      // Ignore the topic if it is not on the topic whitelist
      if (is_whitelisted(topicName, _topicWhitelistPatterns)) {
        latestTopics.emplace(topicName, datatype);
      }
    }

    if (const auto numIgnoredTopics = topicNamesAndTypes.size() - latestTopics.size()) {
      ROS_DEBUG(
        "%zu topics have been ignored as they do not match any pattern on the topic whitelist",
        numIgnoredTopics);
    }

    std::lock_guard<std::mutex> lock(_subscriptionsMutex);

    // Remove channels for which the topic does not exist anymore
    std::vector<cobridge::ChannelId> channelIdsToRemove;
    for (auto channelIt = _advertisedTopics.begin(); channelIt != _advertisedTopics.end();) {
      const TopicAndDatatype topicAndDatatype = {channelIt->second.topic,
                                                 channelIt->second.schema_name};
      if (latestTopics.find(topicAndDatatype) == latestTopics.end()) {
        const auto channelId = channelIt->first;
        channelIdsToRemove.push_back(channelId);
        _subscriptions.erase(channelId);
        ROS_DEBUG("Removed channel %d for topic \"%s\" (%s)", channelId,
                  topicAndDatatype.first.c_str(), topicAndDatatype.second.c_str());
        channelIt = _advertisedTopics.erase(channelIt);
      } else {
        channelIt++;
      }
    }
    _server->remove_channels(channelIdsToRemove);

    // Add new channels for new topics
    std::vector<cobridge::ChannelWithoutId> channelsToAdd;
    for (const auto& topicAndDatatype : latestTopics) {
      if (std::find_if(_advertisedTopics.begin(), _advertisedTopics.end(),
                       [topicAndDatatype](const auto& channelIdAndChannel) {
                         const auto& channel = channelIdAndChannel.second;
                         return channel.topic == topicAndDatatype.first &&
                                channel.schema_name == topicAndDatatype.second;
                       }) != _advertisedTopics.end()) {
        continue;  // Topic already advertised
      }

      cobridge::ChannelWithoutId newChannel{};
      newChannel.topic = topicAndDatatype.first;
      newChannel.schema_name = topicAndDatatype.second;
      newChannel.encoding = ROS1_CHANNEL_ENCODING;

      try {
        const auto msgDescription =
          _rosTypeInfoProvider.getMessageDescription(topicAndDatatype.second);
        if (msgDescription) {
          newChannel.schema = msgDescription->message_definition;
        } else {
          ROS_WARN("Could not find definition for type %s", topicAndDatatype.second.c_str());

          // We still advertise the channel, but with an emtpy schema
          newChannel.schema = "";
        }
      } catch (const std::exception& err) {
        ROS_WARN("Failed to add channel for topic \"%s\" (%s): %s", topicAndDatatype.first.c_str(),
                 topicAndDatatype.second.c_str(), err.what());
        continue;
      }

      channelsToAdd.push_back(newChannel);
    }

    const auto channelIds = _server->add_channels(channelsToAdd);
    for (size_t i = 0; i < channelsToAdd.size(); ++i) {
      const auto channelId = channelIds[i];
      const auto& channel = channelsToAdd[i];
      _advertisedTopics.emplace(channelId, channel);
      ROS_DEBUG("Advertising channel %d for topic \"%s\" (%s)", channelId, channel.topic.c_str(),
                channel.schema_name.c_str());
    }
  }

  void updateAdvertisedServices(const std::vector<std::string>& serviceNames) {
    std::unique_lock<std::shared_mutex> lock(_servicesMutex);

    // Remove advertisements for services that have been removed
    std::vector<cobridge::ServiceId> servicesToRemove;
    for (const auto& service : _advertisedServices) {
      const auto it =
        std::find_if(serviceNames.begin(), serviceNames.end(), [service](const auto& serviceName) {
          return serviceName == service.second.name;
        });
      if (it == serviceNames.end()) {
        servicesToRemove.push_back(service.first);
      }
    }
    for (auto serviceId : servicesToRemove) {
      _advertisedServices.erase(serviceId);
    }
    _server->remove_services(servicesToRemove);

    // Advertise new services
    std::vector<cobridge::ServiceWithoutId> newServices;
    for (const auto& serviceName : serviceNames) {
      if (std::find_if(_advertisedServices.begin(), _advertisedServices.end(),
                       [&serviceName](const auto& idWithService) {
                         return idWithService.second.name == serviceName;
                       }) != _advertisedServices.end()) {
        continue;  // Already advertised
      }

      try {
        const auto serviceType =
          retrieveServiceType(serviceName, std::chrono::milliseconds(_serviceRetrievalTimeoutMs));
        const auto srvDescription = _rosTypeInfoProvider.getServiceDescription(serviceType);

          cobridge::ServiceWithoutId service;
        service.name = serviceName;
        service.type = serviceType;

        if (srvDescription) {
          service.request_schema = srvDescription->request->message_definition;
          service.response_schema = srvDescription->response->message_definition;
        } else {
          ROS_ERROR("Failed to retrieve type information for service '%s' of type '%s'",
                    serviceName.c_str(), serviceType.c_str());

          // We still advertise the channel, but with empty schema.
          service.request_schema = "";
          service.response_schema = "";
        }
        newServices.push_back(service);
      } catch (const std::exception& e) {
        ROS_ERROR("Failed to retrieve service type or service description of service %s: %s",
                  serviceName.c_str(), e.what());
        continue;
      }
    }

    const auto serviceIds = _server->add_services(newServices);
    for (size_t i = 0; i < serviceIds.size(); ++i) {
      _advertisedServices.emplace(serviceIds[i], newServices[i]);
    }
  }

  void getParameters(const std::vector<std::string>& parameters,
                     const std::optional<std::string>& requestId, ConnectionHandle hdl) {
    const bool allParametersRequested = parameters.empty();
    std::vector<std::string> parameterNames = parameters;
    if (allParametersRequested) {
      if (!getMTNodeHandle().getParamNames(parameterNames)) {
        const auto errMsg = "Failed to retrieve parameter names";
        ROS_ERROR_STREAM(errMsg);
        throw std::runtime_error(errMsg);
      }
    }

    bool success = true;
    std::vector<cobridge::Parameter> params;
    for (const auto& paramName : parameterNames) {
      if (!is_whitelisted(paramName, _paramWhitelistPatterns)) {
        if (allParametersRequested) {
          continue;
        } else {
          ROS_ERROR("Parameter '%s' is not on the allowlist", paramName.c_str());
          success = false;
        }
      }

      try {
        XmlRpc::XmlRpcValue value;
        getMTNodeHandle().getParam(paramName, value);
        params.push_back(fromRosParam(paramName, value));
      } catch (const std::exception& ex) {
        ROS_ERROR("Invalid parameter '%s': %s", paramName.c_str(), ex.what());
        success = false;
      } catch (const XmlRpc::XmlRpcException& ex) {
        ROS_ERROR("Invalid parameter '%s': %s", paramName.c_str(), ex.getMessage().c_str());
        success = false;
      } catch (...) {
        ROS_ERROR("Invalid parameter '%s'", paramName.c_str());
        success = false;
      }
    }

    _server->publish_parameter_values(hdl, params, requestId);

    if (!success) {
      throw std::runtime_error("Failed to retrieve one or multiple parameters");
    }
  }

  void setParameters(const std::vector<cobridge::Parameter>& parameters,
                     const std::optional<std::string>& requestId, ConnectionHandle hdl) {
    using cobridge::ParameterType;
    auto nh = this->getMTNodeHandle();

    bool success = true;
    for (const auto& param : parameters) {
      const auto paramName = param.get_name();
      if (!is_whitelisted(paramName, _paramWhitelistPatterns)) {
        ROS_ERROR("Parameter '%s' is not on the allowlist", paramName.c_str());
        success = false;
        continue;
      }

      try {
        const auto paramType = param.get_type();
        const auto paramValue = param.get_value();
        if (paramType == ParameterType::PARAMETER_NOT_SET) {
          nh.deleteParam(paramName);
        } else {
          nh.setParam(paramName, toRosParam(paramValue));
        }
      } catch (const std::exception& ex) {
        ROS_ERROR("Failed to set parameter '%s': %s", paramName.c_str(), ex.what());
        success = false;
      } catch (const XmlRpc::XmlRpcException& ex) {
        ROS_ERROR("Failed to set parameter '%s': %s", paramName.c_str(), ex.getMessage().c_str());
        success = false;
      } catch (...) {
        ROS_ERROR("Failed to set parameter '%s'", paramName.c_str());
        success = false;
      }
    }

    // If a request Id was given, send potentially updated parameters back to client
    if (requestId) {
      std::vector<std::string> parameterNames(parameters.size());
      for (size_t i = 0; i < parameters.size(); ++i) {
        parameterNames[i] = parameters[i].get_name();
      }
      getParameters(parameterNames, requestId, hdl);
    }

    if (!success) {
      throw std::runtime_error("Failed to set one or multiple parameters");
    }
  }

  void subscribeParameters(const std::vector<std::string>& parameters,
                           cobridge::ParameterSubscriptionOperation op, ConnectionHandle) {
    const auto opVerb =
      (op == cobridge::ParameterSubscriptionOperation::SUBSCRIBE) ? "subscribe" : "unsubscribe";
    bool success = true;
    for (const auto& paramName : parameters) {
      if (!is_whitelisted(paramName, _paramWhitelistPatterns)) {
        ROS_ERROR("Parameter '%s' is not allowlist", paramName.c_str());
        continue;
      }

      XmlRpc::XmlRpcValue params, result, payload;
      params[0] = getName() + "2";
      params[1] = xmlrpcServer.getServerURI();
      params[2] = ros::names::resolve(paramName);

      const std::string opName = std::string(opVerb) + "Param";
      if (ros::master::execute(opName, params, result, payload, false)) {
        ROS_DEBUG("%s '%s'", opName.c_str(), paramName.c_str());
      } else {
        ROS_WARN("Failed to %s '%s': %s", opVerb, paramName.c_str(), result.toXml().c_str());
        success = false;
      }
    }

    if (!success) {
      throw std::runtime_error("Failed to " + std::string(opVerb) + " one or multiple parameters.");
    }
  }

  void parameterUpdates(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result) {
    result[0] = 1;
    result[1] = std::string("");
    result[2] = 0;

    if (params.size() != 3) {
      ROS_ERROR("Parameter update called with invalid parameter size: %d", params.size());
      return;
    }

    try {
      const std::string paramName = ros::names::clean(params[1]);
      const XmlRpc::XmlRpcValue paramValue = params[2];
      const auto param = fromRosParam(paramName, paramValue);
      _server->update_parameter_values({param});
    } catch (const std::exception& ex) {
      ROS_ERROR("Failed to update parameter: %s", ex.what());
    } catch (const XmlRpc::XmlRpcException& ex) {
      ROS_ERROR("Failed to update parameter: %s", ex.getMessage().c_str());
    } catch (...) {
      ROS_ERROR("Failed to update parameter");
    }
  }

  void logHandler(cobridge::WebSocketLogLevel level, char const* msg) {
    switch (level) {
      case cobridge::WebSocketLogLevel::Debug:
        ROS_DEBUG("[WS] %s", msg);
        break;
      case cobridge::WebSocketLogLevel::Info:
        ROS_INFO("[WS] %s", msg);
        break;
      case cobridge::WebSocketLogLevel::Warn:
        ROS_WARN("[WS] %s", msg);
        break;
      case cobridge::WebSocketLogLevel::Error:
        ROS_ERROR("[WS] %s", msg);
        break;
      case cobridge::WebSocketLogLevel::Critical:
        ROS_FATAL("[WS] %s", msg);
        break;
    }
  }

  void rosMessageHandler(
    const cobridge::ChannelId channelId, ConnectionHandle clientHandle,
    const ros::MessageEvent<ros_babel_fish::BabelFishMessage const>& msgEvent) {
    const auto& msg = msgEvent.getConstMessage();
    const auto receiptTimeNs = msgEvent.getReceiptTime().toNSec();
    _server->send_message(clientHandle, channelId, receiptTimeNs, msg->buffer(), msg->size());
  }

  void serviceRequest(const cobridge::ServiceRequest& request, ConnectionHandle clientHandle) {
    std::shared_lock<std::shared_mutex> lock(_servicesMutex);
    const auto serviceIt = _advertisedServices.find(request.service_id);
    if (serviceIt == _advertisedServices.end()) {
      const auto errMsg =
        "Service with id " + std::to_string(request.service_id) + " does not exist";
      ROS_ERROR_STREAM(errMsg);
      throw cobridge::ServiceError(request.service_id, errMsg);
    }
    const auto& serviceName = serviceIt->second.name;
    const auto& serviceType = serviceIt->second.type;
    ROS_DEBUG("Received a service request for service %s (%s)", serviceName.c_str(),
              serviceType.c_str());

    if (!ros::service::exists(serviceName, false)) {
      throw cobridge::ServiceError(request.service_id,
                                   "Service '" + serviceName + "' does not exist");
    }

    const auto srvDescription = _rosTypeInfoProvider.getServiceDescription(serviceType);
    if (!srvDescription) {
      const auto errMsg =
        "Failed to retrieve type information for service " + serviceName + "(" + serviceType + ")";
      ROS_ERROR_STREAM(errMsg);
      throw cobridge::ServiceError(request.service_id, errMsg);
    }

    GenericService genReq, genRes;
    genReq.type = genRes.type = serviceType;
    genReq.md5sum = genRes.md5sum = srvDescription->md5;
    genReq.data = request.serv_data;

    if (ros::service::call(serviceName, genReq, genRes)) {
        cobridge::ServiceResponse res;
      res.service_id = request.service_id;
      res.call_id = request.call_id;
      res.encoding = request.encoding;
      res.serv_data = genRes.data;
      _server->send_service_response(clientHandle, res);
    } else {
      throw cobridge::ServiceError(
        request.service_id, "Failed to call service " + serviceName + "(" + serviceType + ")");
    }
  }

  void fetchAsset(const std::string& uri, uint32_t requestId, ConnectionHandle clientHandle) {
      cobridge::FetchAssetResponse response;
    response.request_id = requestId;

    try {
      // We reject URIs that are not on the allowlist or that contain two consecutive dots. The
      // latter can be utilized to construct URIs for retrieving confidential files that should not
      // be accessible over the WebSocket connection. Example:
      // `package://<pkg_name>/../../../secret.txt`. This is an extra security measure and should
      // not be necessary if the allowlist is strict enough.
      if (uri.find("..") != std::string::npos || !is_whitelisted(uri, _assetUriAllowlistPatterns)) {
        throw std::runtime_error("Asset URI not allowed: " + uri);
      }

      resource_retriever::Retriever resource_retriever;
      const resource_retriever::MemoryResource memoryResource = resource_retriever.get(uri);
      response.status = cobridge::FetchAssetStatus::Success;
      response.error_message = "";
      response.data.resize(memoryResource.size);
      std::memcpy(response.data.data(), memoryResource.data.get(), memoryResource.size);
    } catch (const std::exception& ex) {
      ROS_WARN("Failed to retrieve asset '%s': %s", uri.c_str(), ex.what());
      response.status = cobridge::FetchAssetStatus::Error;
      response.error_message = "Failed to retrieve asset " + uri;
    }

    if (_server) {
      _server->send_fetch_asset_response(clientHandle, response);
    }
  }

  bool hasCapability(const std::string& capability) {
    return std::find(_capabilities.begin(), _capabilities.end(), capability) != _capabilities.end();
  }

  std::unique_ptr<cobridge::ServerInterface<ConnectionHandle>> _server;
  ros_babel_fish::IntegratedDescriptionProvider _rosTypeInfoProvider;
  std::vector<std::regex> _topicWhitelistPatterns;
  std::vector<std::regex> _paramWhitelistPatterns;
  std::vector<std::regex> _serviceWhitelistPatterns;
  std::vector<std::regex> _assetUriAllowlistPatterns;
  ros::XMLRPCManager xmlrpcServer;
  std::unordered_map<cobridge::ChannelId, cobridge::ChannelWithoutId> _advertisedTopics;
  std::unordered_map<cobridge::ChannelId, SubscriptionsByClient> _subscriptions;
  std::unordered_map<cobridge::ServiceId, cobridge::ServiceWithoutId> _advertisedServices;
  PublicationsByClient _clientAdvertisedTopics;
  std::mutex _subscriptionsMutex;
  std::shared_mutex _publicationsMutex;
  std::shared_mutex _servicesMutex;
  ros::Timer _updateTimer;
  size_t _maxUpdateMs = size_t(DEFAULT_MAX_UPDATE_MS);
  size_t _updateCount = 0;
  ros::Subscriber _clockSubscription;
  bool _useSimTime = false;
  std::vector<std::string> _capabilities;
  int _serviceRetrievalTimeoutMs = DEFAULT_SERVICE_TYPE_RETRIEVAL_TIMEOUT_MS;
  std::atomic<bool> _subscribeGraphUpdates = false;
  std::unique_ptr<cobridge::CallbackQueue> _fetchAssetQueue;
};

}

PLUGINLIB_EXPORT_CLASS(cobridge::CoBridge, nodelet::Nodelet)
