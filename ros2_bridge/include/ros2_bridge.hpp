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

#include <atomic>
#include <chrono>
#include <memory>
#include <regex>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <rosgraph_msgs/msg/clock.hpp>
#include <websocketpp/common/connection_hdl.hpp>

#include <callback_queue.hpp>
#include <cobridge.hpp>
#include <generic_client.hpp>
#include <message_definition_cache.hpp>
#include <param_utils.hpp>
#include <parameter_interface.hpp>
#include <regex_utils.hpp>
#include <server_factory.hpp>
#include <utils.hpp>

#include <create_generic_subscription.hpp>
#include <create_generic_publisher.hpp>

namespace cobridge {

    using ConnectionHandle = websocketpp::connection_hdl;
    using LogLevel = cobridge::WebSocketLogLevel;
    using Subscription = cobridge::GenericSubscription::SharedPtr;
    using SubscriptionsByClient = std::map<ConnectionHandle, Subscription, std::owner_less<>>;
    using Publication = cobridge::GenericPublisher::SharedPtr;
    using ClientPublications = std::unordered_map<cobridge::ClientChannelId, Publication>;
    using PublicationsByClient = std::map<ConnectionHandle, ClientPublications, std::owner_less<>>;

    class CoBridge : public rclcpp::Node {
    public:
        using TopicAndDatatype = std::pair<std::string, std::string>;

        CoBridge(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

        ~CoBridge();

        void rosgraph_poll_thread();

        void update_advertised_topics(
                const std::map<std::string, std::vector<std::string>> &channel_id_and_channel);

        void update_advertised_services();

        void update_connection_graph(
                const std::map<std::string, std::vector<std::string>> &topic_names_and_types);

    private:
        struct PairHash {
            template<class T1, class T2>
            std::size_t operator()(const std::pair<T1, T2> &pair) const {
                return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
            }
        };

        std::unique_ptr<cobridge::ServerInterface<ConnectionHandle>> _server;
        cobridge::MessageDefinitionCache _message_definition_cache;
        std::vector<std::regex> _topic_whitelist_patterns;
        std::vector<std::regex> _service_whitelist_patterns;
        std::vector<std::regex> _asset_uri_allowlist_patterns;
//        std::shared_ptr<ParameterInterface> _param_interface;
        std::unordered_map<cobridge::ChannelId, cobridge::ChannelWithoutId> _advertised_topics;
        std::unordered_map<cobridge::ServiceId, cobridge::ServiceWithoutId> _advertised_services;
        std::unordered_map<cobridge::ChannelId, SubscriptionsByClient> _subscriptions;
        PublicationsByClient _client_advertised_topics;
        std::unordered_map<cobridge::ServiceId, GenericClient::SharedPtr> _service_clients;
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
        std::unique_ptr<cobridge::CallbackQueue> _fetch_asset_queue;

        void subscribe_connection_graph(bool subscribe);

        void subscribe(cobridge::ChannelId channel_id, ConnectionHandle client_handle);

        void unsubscribe(cobridge::ChannelId channel_id, ConnectionHandle client_handle);

        void client_advertise(const cobridge::ClientAdvertisement &advertisement, ConnectionHandle hdl);

        void client_unadvertise(cobridge::ChannelId channel_id, ConnectionHandle hdl);

        void client_message(const cobridge::ClientMessage &message, ConnectionHandle hdl);

//        void set_parameters(const std::vector<cobridge::Parameter> &parameters,
//                           const std::optional<std::string> &request_id, ConnectionHandle hdl);
//
//        void get_parameters(const std::vector<std::string> &parameters,
//                           const std::optional<std::string> &request_id, ConnectionHandle hdl);
//
//        void subscribe_parameters(const std::vector<std::string> &parameters,
//                                 cobridge::ParameterSubscriptionOperation op, ConnectionHandle);

        void parameter_updates(const std::vector<cobridge::Parameter> &parameters);

        void log_handler(LogLevel level, char const *msg);

        void  ros_message_handler(const cobridge::ChannelId &channel_id, ConnectionHandle client_handle,
                                  std::shared_ptr<rclcpp::SerializedMessage> msg, uint64_t timestamp);

        void service_request(const cobridge::ServiceRequest &request, ConnectionHandle client_handle);

        void fetch_asset(const std::string &asset_id, uint32_t request_id, ConnectionHandle client_handle);

        bool has_capability(const std::string &capability);
    };

}
