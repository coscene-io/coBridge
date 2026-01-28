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

#include <param_utils.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <ros/ros.h>

namespace cobridge
{

cobridge_base::ParameterValue fromRosParam(const XmlRpc::XmlRpcValue & value)
{
  XmlRpc::XmlRpcValue non_const_value = value;
  switch (non_const_value.getType()) {
    case XmlRpc::XmlRpcValue::TypeBoolean:
      {
        return cobridge_base::ParameterValue(static_cast<bool>(non_const_value));
      }

    case XmlRpc::XmlRpcValue::TypeInt:
      {
        return cobridge_base::ParameterValue(static_cast<int>(non_const_value));
      }

    case XmlRpc::XmlRpcValue::TypeDouble:
      {
        return cobridge_base::ParameterValue(static_cast<double>(non_const_value));
      }

    case XmlRpc::XmlRpcValue::TypeString:
      {
        return cobridge_base::ParameterValue(static_cast<std::string>(non_const_value));
      }

    case XmlRpc::XmlRpcValue::TypeStruct:
      {
        std::unordered_map<std::string, cobridge_base::ParameterValue> param_map;
        for (const auto & element : non_const_value) {
          param_map.insert(std::make_pair(element.first, fromRosParam(element.second)));
        }
        return cobridge_base::ParameterValue(param_map);
      }

    case XmlRpc::XmlRpcValue::TypeArray:
      {
        std::vector<cobridge_base::ParameterValue> param_vec;
        for (int i = 0; i < non_const_value.size(); ++i) {
          param_vec.push_back(fromRosParam(non_const_value[i]));
        }
        return cobridge_base::ParameterValue(param_vec);
      }

    case XmlRpc::XmlRpcValue::TypeInvalid:
      throw std::runtime_error("Parameter not set");

    default:
      throw std::runtime_error(
              "Unsupported parameter type: " +
              std::to_string(non_const_value.getType()));
  }
}

cobridge_base::Parameter from_ros_param(const std::string & name, const XmlRpc::XmlRpcValue & value)
{
  return cobridge_base::Parameter(name, fromRosParam(value));
}

XmlRpc::XmlRpcValue to_ros_param(const cobridge_base::ParameterValue & param)
{
  const auto param_type = param.getType();
  if (param_type == cobridge_base::ParameterType::PARAMETER_BOOL) {
    return param.getValue<bool>();
  } else if (param_type == cobridge_base::ParameterType::PARAMETER_INTEGER) {
    return static_cast<int>(param.getValue<int64_t>());
  } else if (param_type == cobridge_base::ParameterType::PARAMETER_DOUBLE) {
    return param.getValue<double>();
  } else if (param_type == cobridge_base::ParameterType::PARAMETER_STRING) {
    return param.getValue<std::string>();
  } else if (param_type == cobridge_base::ParameterType::PARAMETER_STRUCT) {
    XmlRpc::XmlRpcValue value_struct;
    const auto & param_map =
      param.getValue<std::unordered_map<std::string, cobridge_base::ParameterValue>>();
    for (const auto & param_element : param_map) {
      value_struct[param_element.first] = to_ros_param(param_element.second);
    }
    return value_struct;
  } else if (param_type == cobridge_base::ParameterType::PARAMETER_ARRAY) {
    XmlRpc::XmlRpcValue arr;
    const auto vec = param.getValue<std::vector<cobridge_base::ParameterValue>>();
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
      arr[i] = to_ros_param(vec[i]);
    }
    return arr;
  } else {
    throw std::runtime_error("Unsupported parameter type");
  }

  return XmlRpc::XmlRpcValue();
}

std::vector<std::regex> parse_regex_patterns(const std::vector<std::string> & patterns)
{
  std::vector<std::regex> result;
  result.reserve(patterns.size());

  for (const auto & pattern : patterns) {
    try {
      result.emplace_back(
        pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
    } catch (const std::regex_error & ex) {
      ROS_ERROR("Failed to parse regex pattern '%s': %s", pattern.c_str(), ex.what());
    } catch (const std::exception & ex) {
      ROS_ERROR("Failed to parse regex pattern '%s': %s", pattern.c_str(), ex.what());
    }
  }

  if (result.empty()) {
    ROS_WARN("No valid regex patterns found, using default '.*' pattern");
    try {
      result.emplace_back(".*", std::regex_constants::ECMAScript | std::regex_constants::icase);
    } catch (const std::exception & ex) {
      ROS_ERROR("Failed to create default regex pattern: %s", ex.what());
    }
  }

  return result;
}

}  // namespace cobridge
