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

#include <common.hpp>
#include <param_utils.hpp>

namespace cobridge {

    void declare_parameters(rclcpp::Node *node) {
        auto port_description = rcl_interfaces::msg::ParameterDescriptor{};
        port_description.name = PARAM_PORT;
        port_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
        port_description.description = "The TCP port to bind the WebSocket server to";
        port_description.read_only = true;
        port_description.additional_constraints =
                "Must be a valid TCP port number, or 0 to use a random port";
        port_description.integer_range.resize(1);
        port_description.integer_range[0].from_value = 0;
        port_description.integer_range[0].to_value = 65535;
        port_description.integer_range[0].step = 1;
        node->declare_parameter(PARAM_PORT, DEFAULT_PORT, port_description);

        auto address_description = rcl_interfaces::msg::ParameterDescriptor{};
        address_description.name = PARAM_ADDRESS;
        address_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
        address_description.description = "The host address to bind the WebSocket server to";
        address_description.read_only = true;
        node->declare_parameter(PARAM_ADDRESS, DEFAULT_ADDRESS, address_description);

        auto send_buffer_limit_description = rcl_interfaces::msg::ParameterDescriptor{};
        send_buffer_limit_description.name = PARAM_SEND_BUFFER_LIMIT;
        send_buffer_limit_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
        send_buffer_limit_description.description =
                "Connection send buffer limit in bytes. Messages will be dropped when a connection's send "
                "buffer reaches this limit to avoid a queue of outdated messages building up.";
        send_buffer_limit_description.integer_range.resize(1);
        send_buffer_limit_description.integer_range[0].from_value = 0;
        send_buffer_limit_description.integer_range[0].to_value = std::numeric_limits<int64_t>::max();
        send_buffer_limit_description.read_only = true;
        node->declare_parameter(PARAM_SEND_BUFFER_LIMIT, DEFAULT_SEND_BUFFER_LIMIT,
                                send_buffer_limit_description);

        auto useT_tls_description = rcl_interfaces::msg::ParameterDescriptor{};
        useT_tls_description.name = PARAM_USETLS;
        useT_tls_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
        useT_tls_description.description = "Use Transport Layer Security for encrypted communication";
        useT_tls_description.read_only = true;
        node->declare_parameter(PARAM_USETLS, false, useT_tls_description);

        auto cert_file_description = rcl_interfaces::msg::ParameterDescriptor{};
        cert_file_description.name = PARAM_CERTFILE;
        cert_file_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
        cert_file_description.description = "Path to the certificate to use for TLS";
        cert_file_description.read_only = true;
        node->declare_parameter(PARAM_CERTFILE, "", cert_file_description);

        auto keyfile_description = rcl_interfaces::msg::ParameterDescriptor{};
        keyfile_description.name = PARAM_KEYFILE;
        keyfile_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
        keyfile_description.description = "Path to the private key to use for TLS";
        keyfile_description.read_only = true;
        node->declare_parameter(PARAM_KEYFILE, "", keyfile_description);

        auto min_qos_depth_description = rcl_interfaces::msg::ParameterDescriptor{};
        min_qos_depth_description.name = PARAM_MIN_QOS_DEPTH;
        min_qos_depth_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
        min_qos_depth_description.description = "Minimum depth used for the QoS profile of subscriptions.";
        min_qos_depth_description.read_only = true;
        min_qos_depth_description.additional_constraints = "Must be a non-negative integer";
        min_qos_depth_description.integer_range.resize(1);
        min_qos_depth_description.integer_range[0].from_value = 0;
        min_qos_depth_description.integer_range[0].to_value = INT32_MAX;
        min_qos_depth_description.integer_range[0].step = 1;
        node->declare_parameter(PARAM_MIN_QOS_DEPTH, DEFAULT_MIN_QOS_DEPTH, min_qos_depth_description);

        auto max_qos_depth_description = rcl_interfaces::msg::ParameterDescriptor{};
        max_qos_depth_description.name = PARAM_MAX_QOS_DEPTH;
        max_qos_depth_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
        max_qos_depth_description.description = "Maximum depth used for the QoS profile of subscriptions.";
        max_qos_depth_description.read_only = true;
        max_qos_depth_description.additional_constraints = "Must be a non-negative integer";
        max_qos_depth_description.integer_range.resize(1);
        max_qos_depth_description.integer_range[0].from_value = 0;
        max_qos_depth_description.integer_range[0].to_value = INT32_MAX;
        max_qos_depth_description.integer_range[0].step = 1;
        node->declare_parameter(PARAM_MAX_QOS_DEPTH, DEFAULT_MAX_QOS_DEPTH, max_qos_depth_description);

        auto topic_whitelist_description = rcl_interfaces::msg::ParameterDescriptor{};
        topic_whitelist_description.name = PARAM_TOPIC_WHITELIST;
        topic_whitelist_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        topic_whitelist_description.description =
                "List of regular expressions (ECMAScript) of whitelisted topic names.";
        topic_whitelist_description.read_only = true;
        node->declare_parameter(PARAM_TOPIC_WHITELIST, std::vector<std::string>({".*"}),
                                topic_whitelist_description);

        auto service_whitelist_description = rcl_interfaces::msg::ParameterDescriptor{};
        service_whitelist_description.name = PARAM_SERVICE_WHITELIST;
        service_whitelist_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        service_whitelist_description.description =
                "List of regular expressions (ECMAScript) of whitelisted service names.";
        service_whitelist_description.read_only = true;
        node->declare_parameter(PARAM_SERVICE_WHITELIST, std::vector<std::string>({".*"}),
                                service_whitelist_description);

        auto param_whitelist_description = rcl_interfaces::msg::ParameterDescriptor{};
        param_whitelist_description.name = PARAM_PARAMETER_WHITELIST;
        param_whitelist_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        param_whitelist_description.description =
                "List of regular expressions (ECMAScript) of whitelisted parameter names.";
        param_whitelist_description.read_only = true;
        node->declare_parameter(PARAM_PARAMETER_WHITELIST, std::vector<std::string>({".*"}),
                                param_whitelist_description);

        auto use_compression_description = rcl_interfaces::msg::ParameterDescriptor{};
        use_compression_description.name = PARAM_USE_COMPRESSION;
        use_compression_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
        use_compression_description.description =
                "Use websocket compression (permessage-deflate). Suited for connections with smaller bandwith, "
                "at the cost of additional CPU load.";
        use_compression_description.read_only = true;
        node->declare_parameter(PARAM_USE_COMPRESSION, false, use_compression_description);

        auto param_capabilities = rcl_interfaces::msg::ParameterDescriptor{};
        param_capabilities.name = PARAM_CAPABILITIES;
        param_capabilities.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        param_capabilities.description = "Server capabilities";
        param_capabilities.read_only = true;
        node->declare_parameter(
                PARAM_CAPABILITIES,
                std::vector<std::string>(std::vector<std::string>(cobridge::DEFAULT_CAPABILITIES.begin(),
                                                                  cobridge::DEFAULT_CAPABILITIES.end())),
                param_capabilities);

        auto client_topic_whitelist_description = rcl_interfaces::msg::ParameterDescriptor{};
        client_topic_whitelist_description.name = PARAM_CLIENT_TOPIC_WHITELIST;
        client_topic_whitelist_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        client_topic_whitelist_description.description =
                "List of regular expressions (ECMAScript) of whitelisted client-published topic names.";
        client_topic_whitelist_description.read_only = true;
        node->declare_parameter(PARAM_CLIENT_TOPIC_WHITELIST, std::vector<std::string>({".*"}),
                                param_whitelist_description);

        auto include_hidden_description = rcl_interfaces::msg::ParameterDescriptor{};
        include_hidden_description.name = PARAM_INCLUDE_HIDDEN;
        include_hidden_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
        include_hidden_description.description = "Include hidden topics and services";
        include_hidden_description.read_only = true;
        node->declare_parameter(PARAM_INCLUDE_HIDDEN, false, include_hidden_description);

        auto asset_uri_allowlist_description = rcl_interfaces::msg::ParameterDescriptor{};
        asset_uri_allowlist_description.name = PARAM_ASSET_URI_ALLOWLIST;
        asset_uri_allowlist_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY;
        asset_uri_allowlist_description.description =
                "List of regular expressions (ECMAScript) of whitelisted asset URIs.";
        asset_uri_allowlist_description.read_only = true;
        node->declare_parameter(
                PARAM_ASSET_URI_ALLOWLIST,
                std::vector<std::string>(
                        {"^package://(?:\\w+/"
                         ")*\\w+\\.(?:dae|fbx|glb|gltf|jpeg|jpg|mtl|obj|png|stl|tif|tiff|urdf|webp|xacro)$"}),
                param_whitelist_description);
    }

    std::vector<std::regex> parse_regex_strings(rclcpp::Node *node,
                                                const std::vector<std::string> &strings) {
        std::vector<std::regex> regex_vector;
        regex_vector.reserve(strings.size());

        for (const auto &pattern: strings) {
            try {
                regex_vector.emplace_back(pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
            } catch (const std::exception &ex) {
                RCLCPP_ERROR(node->get_logger(), "Ignoring invalid regular expression '%s': %s",
                             pattern.c_str(), ex.what());
            }
        }

        return regex_vector;
    }

}
