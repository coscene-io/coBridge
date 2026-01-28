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

#include "parameter_interface.hpp"
#include <json.hpp>
#include <regex_utils.hpp>
#include <utils.hpp>

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace
{

constexpr char PARAM_SEP = '.';

std::pair<std::string, std::string> get_node_and_param_name(
  const std::string & node_name_and_param_name)
{
  return {node_name_and_param_name.substr(0UL, node_name_and_param_name.find(PARAM_SEP)),
    node_name_and_param_name.substr(node_name_and_param_name.find(PARAM_SEP) + 1UL)};
}

std::string prepend_node_name_to_param_name(
  const std::string & param_name,
  const std::string & node_name)
{
  return node_name + PARAM_SEP + param_name;
}

rclcpp::Parameter to_ros_param(const cobridge_base::Parameter & p)
{
  using cobridge_base::Parameter;
  using cobridge_base::ParameterType;

  const auto param_type = p.get_type();
  const auto value = p.get_value();

  if (param_type == ParameterType::PARAMETER_BOOL) {
    return {p.get_name(), value.getValue<bool>()};
  } else if (param_type == ParameterType::PARAMETER_INTEGER) {
    return {p.get_name(), value.getValue<int64_t>()};
  } else if (param_type == ParameterType::PARAMETER_DOUBLE) {
    return {p.get_name(), value.getValue<double>()};
  } else if (param_type == ParameterType::PARAMETER_STRING) {
    return {p.get_name(), value.getValue<std::string>()};
  } else if (param_type == ParameterType::PARAMETER_BYTE_ARRAY) {
    return {p.get_name(), value.getValue<std::vector<unsigned char>>()};
  } else if (param_type == ParameterType::PARAMETER_ARRAY) {
    const auto param_array = value.getValue<std::vector<cobridge_base::ParameterValue>>();

    const auto element_type = param_array.front().getType();
    if (element_type == ParameterType::PARAMETER_BOOL) {
      std::vector<bool> vec;
      vec.reserve(param_array.size());
      for (const auto & param_value : param_array) {
        vec.push_back(param_value.getValue<bool>());
      }
      return {p.get_name(), vec};
    } else if (element_type == ParameterType::PARAMETER_INTEGER) {
      std::vector<int64_t> vec;
      vec.reserve(param_array.size());
      for (const auto & param_value : param_array) {
        vec.push_back(param_value.getValue<int64_t>());
      }
      return {p.get_name(), vec};
    } else if (element_type == ParameterType::PARAMETER_DOUBLE) {
      std::vector<double> vec;
      vec.reserve(param_array.size());
      for (const auto & paramValue : param_array) {
        vec.push_back(paramValue.getValue<double>());
      }
      return {p.get_name(), vec};
    } else if (element_type == ParameterType::PARAMETER_STRING) {
      std::vector<std::string> vec;
      vec.reserve(param_array.size());
      for (const auto & paramValue : param_array) {
        vec.push_back(paramValue.getValue<std::string>());
      }
      return {p.get_name(), vec};
    }
    throw std::runtime_error("Unsupported parameter type");
  } else if (param_type == ParameterType::PARAMETER_NOT_SET) {
    return rclcpp::Parameter(p.get_name());
  } else {
    throw std::runtime_error("Unsupported parameter type");
  }
  return rclcpp::Parameter();
}

cobridge_base::Parameter from_ros_param(const rclcpp::Parameter & p)
{
  const auto type = p.get_type();

  if (type == rclcpp::ParameterType::PARAMETER_NOT_SET) {
    return {p.get_name(), cobridge_base::ParameterValue()};
  } else if (type == rclcpp::ParameterType::PARAMETER_BOOL) {
    return {p.get_name(), cobridge_base::ParameterValue(p.as_bool())};
  } else if (type == rclcpp::ParameterType::PARAMETER_INTEGER) {
    return {p.get_name(), cobridge_base::ParameterValue(p.as_int())};
  } else if (type == rclcpp::ParameterType::PARAMETER_DOUBLE) {
    return {p.get_name(), cobridge_base::ParameterValue(p.as_double())};
  } else if (type == rclcpp::ParameterType::PARAMETER_STRING) {
    return {p.get_name(), cobridge_base::ParameterValue(p.as_string())};
  } else if (type == rclcpp::ParameterType::PARAMETER_BYTE_ARRAY) {
    return {p.get_name(), cobridge_base::ParameterValue(p.as_byte_array())};
  } else if (type == rclcpp::ParameterType::PARAMETER_BOOL_ARRAY) {
    std::vector<cobridge_base::ParameterValue> param_array;
    for (const auto value : p.as_bool_array()) {
      param_array.emplace_back(value);
    }
    return {p.get_name(), cobridge_base::ParameterValue(param_array)};
  } else if (type == rclcpp::ParameterType::PARAMETER_INTEGER_ARRAY) {
    std::vector<cobridge_base::ParameterValue> param_array;
    for (const auto value : p.as_integer_array()) {
      param_array.emplace_back(value);
    }
    return {p.get_name(), cobridge_base::ParameterValue(param_array)};
  } else if (type == rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY) {
    std::vector<cobridge_base::ParameterValue> param_array;
    for (const auto value : p.as_double_array()) {
      param_array.emplace_back(value);
    }
    return {p.get_name(), cobridge_base::ParameterValue(param_array)};
  } else if (type == rclcpp::ParameterType::PARAMETER_STRING_ARRAY) {
    std::vector<cobridge_base::ParameterValue> param_array;
    for (const auto & value : p.as_string_array()) {
      param_array.emplace_back(value);
    }
    return {p.get_name(), cobridge_base::ParameterValue(param_array)};
  } else {
    throw std::runtime_error("Unsupported parameter type");
  }
}

}  // namespace

namespace cobridge
{

using cobridge_base::is_whitelisted;

ParameterInterface::ParameterInterface(
  rclcpp::Node * node,
  std::vector<std::regex> param_whitelist_patterns)
: _node(node), _param_whitelist_patterns(param_whitelist_patterns),
  _callback_group(node->create_callback_group(rclcpp::CallbackGroupType::Reentrant))
{}

ParameterList ParameterInterface::get_params(
  const std::vector<std::string> & param_names,
  const std::chrono::duration<double> & timeout)
{
  std::lock_guard<std::mutex> lock(_mutex);

  std::unordered_map<std::string, std::vector<std::string>> param_names_by_node_name;
  const auto this_node = _node->get_fully_qualified_name();

  if (!param_names.empty()) {
    // Break apart fully qualified {node_name}.{param_name} strings and build a
    // mape of node names to the list of parameters for each node
    for (const auto & full_param_name : param_names) {
      const auto & [node_name, param_name] = get_node_and_param_name(full_param_name);
      param_names_by_node_name[node_name].push_back(param_name);
    }

    RCLCPP_INFO(
      _node->get_logger(), "Getting %zu parameters from %zu nodes...", param_names.size(),
      param_names_by_node_name.size());
  } else {
    // Make a map of node names to empty parameter lists
    // Only consider nodes that offer services to list & get parameters.
    for (const auto & fqn_node_name : _node->get_node_names()) {
      if (fqn_node_name == this_node || fqn_node_name == "/cobridge_component_manager") {
        continue;
      }
      const auto [node_namespace, node_name] = get_node_and_node_namespace(fqn_node_name);
      const auto service_names_and_types =
        _node->get_service_names_and_types_by_node(node_name, node_namespace);

      bool list_params_srv_found = false, get_params_srv_found = false;
      constexpr char GET_PARAMS_SERVICE_TYPE[] = "rcl_interfaces/srv/GetParameters";
      constexpr char LIST_PARAMS_SERVICE_TYPE[] = "rcl_interfaces/srv/ListParameters";

      for (const auto & [service_name, service_types] : service_names_and_types) {
        if (!get_params_srv_found) {
          get_params_srv_found = std::find(
            service_types.begin(), service_types.end(),
            GET_PARAMS_SERVICE_TYPE) != service_types.end();
        }
        if (!list_params_srv_found) {
          list_params_srv_found = std::find(
            service_types.begin(), service_types.end(),
            LIST_PARAMS_SERVICE_TYPE) != service_types.end();
        }
      }

      if (list_params_srv_found && get_params_srv_found) {
        param_names_by_node_name.insert({fqn_node_name, {}});
      }
    }

    if (!param_names_by_node_name.empty()) {
      RCLCPP_DEBUG(
        _node->get_logger(), "Getting all parameters from %zu nodes...",
        param_names_by_node_name.size());
    }
  }

  std::vector<std::future<ParameterList>> get_parameters_future;
  for (const auto & [node_name, node_param_names] : param_names_by_node_name) {
    if (node_name == this_node) {
      continue;
    }

    auto client_iter = _param_clients_by_node.find(node_name);
    if (client_iter == _param_clients_by_node.end()) {
      const auto inserted_pair = _param_clients_by_node.emplace(
        node_name, rclcpp::AsyncParametersClient::make_shared(
          _node, node_name, rmw_qos_profile_parameters, _callback_group));
      client_iter = inserted_pair.first;
    }

    get_parameters_future.emplace_back(
      std::async(
        std::launch::async, &ParameterInterface::get_node_parameters, this,
        client_iter->second, node_name, node_param_names, timeout));
  }

  ParameterList result;
  for (auto & future : get_parameters_future) {
    try {
      const auto params = future.get();
      result.insert(result.begin(), params.begin(), params.end());
    } catch (const std::exception & e) {
      RCLCPP_ERROR(_node->get_logger(), "Exception when getting parameters: %s", e.what());
    }
  }

  return result;
}

void ParameterInterface::set_params(
  const ParameterList & parameters,
  const std::chrono::duration<double> & timeout)
{
  std::lock_guard<std::mutex> lock(_mutex);

  rclcpp::ParameterMap params_by_node;
  for (const auto & param : parameters) {
    if (!is_whitelisted(param.get_name(), _param_whitelist_patterns)) {
      return;
    }

    const auto ros_param = to_ros_param(param);
    const auto & [node_name, param_name] = get_node_and_param_name(ros_param.get_name());
    params_by_node[node_name].emplace_back(param_name, ros_param.get_parameter_value());
  }

  std::vector<std::future<void>> set_parameters_future;
  for (const auto & [node_name, params] : params_by_node) {
    auto param_client_iter = _param_clients_by_node.find(node_name);
    if (param_client_iter == _param_clients_by_node.end()) {
      const auto inserted_pair = _param_clients_by_node.emplace(
        node_name, rclcpp::AsyncParametersClient::make_shared(
          _node, node_name, rmw_qos_profile_parameters, _callback_group));
      param_client_iter = inserted_pair.first;
    }

    set_parameters_future.emplace_back(
      std::async(
        std::launch::async,
        &ParameterInterface::set_node_parameters, this,
        param_client_iter->second, node_name, params, timeout));
  }

  for (auto & future : set_parameters_future) {
    try {
      future.get();
    } catch (const std::exception & e) {
      RCLCPP_ERROR(_node->get_logger(), "Exception when setting parameters: %s", e.what());
    }
  }
}

void ParameterInterface::subscribe_params(const std::vector<std::string> & param_names)
{
  std::lock_guard<std::mutex> lock(_mutex);

  std::unordered_set<std::string> nodes_to_subscribe;
  for (const auto & param_name : param_names) {
    if (!is_whitelisted(param_name, _param_whitelist_patterns)) {
      return;
    }

    // TODO(fei): name of paramN
    const auto & [node_name, paramN] = get_node_and_param_name(param_name);
    auto [subscribed_params_iter,
      newly_created] = _subscribed_params_by_node.try_emplace(node_name);

    auto & subscribed_node_params = subscribed_params_iter->second;
    subscribed_node_params.insert(paramN);

    if (newly_created) {
      nodes_to_subscribe.insert(node_name);
    }
  }

  for (const auto & node_name : nodes_to_subscribe) {
    auto param_client_iter = _param_clients_by_node.find(node_name);
    if (param_client_iter == _param_clients_by_node.end()) {
      const auto inserted_pair = _param_clients_by_node.emplace(
        node_name, rclcpp::AsyncParametersClient::make_shared(
          _node, node_name, rmw_qos_profile_parameters, _callback_group));
      param_client_iter = inserted_pair.first;
    }

    auto & param_client = param_client_iter->second;

    _param_subscriptions_by_node[node_name] = param_client->on_parameter_event(
      [this, node_name](rcl_interfaces::msg::ParameterEvent::ConstSharedPtr msg)
      {
        RCLCPP_DEBUG(
          _node->get_logger(), "Retrieved param update for node %s: %zu params changed",
          node_name.c_str(), msg->changed_parameters.size());

        ParameterList result;
        const auto & subscribed_node_params = _subscribed_params_by_node[node_name];
        for (const auto & param : msg->changed_parameters) {
          if (subscribed_node_params.find(param.name) != subscribed_node_params.end()) {
            result.push_back(
              from_ros_param(
                rclcpp::Parameter(
                  prepend_node_name_to_param_name(param.name, node_name),
                  param.value)));
          }
        }

        if (!result.empty() && _param_update_func) {
          _param_update_func(result);
        }
      });
  }
}

void ParameterInterface::unsubscribe_params(const std::vector<std::string> & param_names)
{
  std::lock_guard<std::mutex> lock(_mutex);

  for (const auto & param_name : param_names) {
    const auto & [node_name, paramN] = get_node_and_param_name(param_name);

    const auto subscribed_node_params_iter = _subscribed_params_by_node.find(node_name);
    if (subscribed_node_params_iter != _subscribed_params_by_node.end()) {
      subscribed_node_params_iter->second.erase(subscribed_node_params_iter->second.find(paramN));

      if (subscribed_node_params_iter->second.empty()) {
        _subscribed_params_by_node.erase(subscribed_node_params_iter);
        _param_subscriptions_by_node.erase(_param_subscriptions_by_node.find(node_name));
      }
    }
  }
}

void ParameterInterface::set_param_update_callback(ParamUpdateFunc param_update_func)
{
  std::lock_guard<std::mutex> lock(_mutex);
  _param_update_func = param_update_func;
}

ParameterList ParameterInterface::get_node_parameters(
  const rclcpp::AsyncParametersClient::SharedPtr param_client, const std::string & node_name,
  const std::vector<std::string> & param_names, const std::chrono::duration<double> & timeout)
{
  if (!param_client->service_is_ready()) {
    throw std::runtime_error("Parameter service for node '" + node_name + "' is not ready");
  }

  auto params_to_request = param_names;
  if (params_to_request.empty()) {
    // `paramNames` is empty, list all parameter names for this node
    auto future = param_client->list_parameters({}, 0UL);
    if (std::future_status::ready != future.wait_for(timeout)) {
      throw std::runtime_error("Failed to retrieve parameter names for node '" + node_name + "'");
    }
    params_to_request = future.get().names;
  }

  // Start parameter fetches and wait for them to complete
  auto get_params_future = param_client->get_parameters(params_to_request);
  if (std::future_status::ready != get_params_future.wait_for(timeout)) {
    throw std::runtime_error(
            "Timed out waiting for " + std::to_string(params_to_request.size()) +
            " parameter(s) from node '" + node_name + "'");
  }
  const auto params = get_params_future.get();

  ParameterList result;
  for (const auto & param : params) {
    const auto full_param_name = prepend_node_name_to_param_name(param.get_name(), node_name);
    if (is_whitelisted(full_param_name, _param_whitelist_patterns)) {
      result.push_back(
        from_ros_param(
          rclcpp::Parameter(
            full_param_name,
            param.get_parameter_value())));
    }
  }
  return result;
}

void ParameterInterface::set_node_parameters(
  rclcpp::AsyncParametersClient::SharedPtr param_client,
  const std::string & node_name,
  const std::vector<rclcpp::Parameter> & params,
  const std::chrono::duration<double> & timeout)
{
  if (!param_client->service_is_ready()) {
    throw std::runtime_error("Parameter service for node '" + node_name + "' is not ready");
  }

  auto future = param_client->set_parameters(params);

//  std::vector<std::string> paramsToDelete;
//  for (const auto& p : params) {
//    if (p.get_type() == rclcpp::ParameterType::PARAMETER_NOT_SET) {
//      paramsToDelete.push_back(p.get_name());
//    }
//  }
//
//  if (!paramsToDelete.empty()) {
//    auto deleteFuture = paramClient->delete_parameters(paramsToDelete);
//    if (std::future_status::ready != deleteFuture.wait_for(timeout)) {
//      RCLCPP_WARN(
//        _node->get_logger(),
//        "Param client failed to delete %zu parameter(s) for node '%s' within the given timeout",
//        paramsToDelete.size(), nodeName.c_str());
//    }
//  }

  if (std::future_status::ready != future.wait_for(timeout)) {
    throw std::runtime_error(
            "Param client failed to set " + std::to_string(params.size()) +
            " parameter(s) for node '" + node_name + "' within the given timeout");
  }

  const auto set_param_results = future.get();
  for (auto & result : set_param_results) {
    if (!result.successful) {
      RCLCPP_WARN(
        _node->get_logger(), "Failed to set one or more parameters for node '%s': %s",
        node_name.c_str(), result.reason.c_str());
    }
  }
}

}  // namespace cobridge
