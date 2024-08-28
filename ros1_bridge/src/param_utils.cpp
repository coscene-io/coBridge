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

namespace cobridge
{

cobridge_base::ParameterValue fromRosParam(const XmlRpc::XmlRpcValue & value)
{
  const auto type = value.getType();

  if (type == XmlRpc::XmlRpcValue::Type::TypeBoolean) {
    return cobridge_base::ParameterValue(static_cast<bool>(value));
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeInt) {
    return cobridge_base::ParameterValue(static_cast<int64_t>(static_cast<int>(value)));
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeDouble) {
    return cobridge_base::ParameterValue(static_cast<double>(value));
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeString) {
    return cobridge_base::ParameterValue(static_cast<std::string>(value));
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeStruct) {
    std::unordered_map<std::string, cobridge_base::ParameterValue> paramMap;
    for (const auto & [elementName, elementVal] : value) {
      paramMap.insert({elementName, fromRosParam(elementVal)});
    }
    return cobridge_base::ParameterValue(paramMap);
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeArray) {
    std::vector<cobridge_base::ParameterValue> paramVec;
    for (int i = 0; i < value.size(); ++i) {
      paramVec.push_back(fromRosParam(value[i]));
    }
    return cobridge_base::ParameterValue(paramVec);
  } else if (type == XmlRpc::XmlRpcValue::Type::TypeInvalid) {
    throw std::runtime_error("Parameter not set");
  } else {
    throw std::runtime_error("Unsupported parameter type: " + std::to_string(type));
  }
}

cobridge_base::Parameter fromRosParam(const std::string & name, const XmlRpc::XmlRpcValue & value)
{
  return cobridge_base::Parameter(name, fromRosParam(value));
}

XmlRpc::XmlRpcValue toRosParam(const cobridge_base::ParameterValue & param)
{
  const auto paramType = param.getType();
  if (paramType == cobridge_base::ParameterType::PARAMETER_BOOL) {
    return param.getValue<bool>();
  } else if (paramType == cobridge_base::ParameterType::PARAMETER_INTEGER) {
    return static_cast<int>(param.getValue<int64_t>());
  } else if (paramType == cobridge_base::ParameterType::PARAMETER_DOUBLE) {
    return param.getValue<double>();
  } else if (paramType == cobridge_base::ParameterType::PARAMETER_STRING) {
    return param.getValue<std::string>();
  } else if (paramType == cobridge_base::ParameterType::PARAMETER_STRUCT) {
    XmlRpc::XmlRpcValue valueStruct;
    const auto & paramMap =
      param.getValue<std::unordered_map<std::string, cobridge_base::ParameterValue>>();
    for (const auto & [paramName, paramElement] : paramMap) {
      valueStruct[paramName] = toRosParam(paramElement);
    }
    return valueStruct;
  } else if (paramType == cobridge_base::ParameterType::PARAMETER_ARRAY) {
    XmlRpc::XmlRpcValue arr;
    const auto vec = param.getValue<std::vector<cobridge_base::ParameterValue>>();
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
      arr[i] = toRosParam(vec[i]);
    }
    return arr;
  } else {
    throw std::runtime_error("Unsupported parameter type");
  }

  return XmlRpc::XmlRpcValue();
}

std::vector<std::regex> parseRegexPatterns(const std::vector<std::string> & patterns)
{
  std::vector<std::regex> result;
  for (const auto & pattern : patterns) {
    try {
      result.push_back(
        std::regex(pattern, std::regex_constants::ECMAScript | std::regex_constants::icase));
    } catch (...) {
      continue;
    }
  }
  return result;
}

}  // namespace cobridge
