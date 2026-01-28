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
#ifndef PARAM_UTILS_HPP_
#define PARAM_UTILS_HPP_

#ifdef ROS1_VERSION_INDIGO
#include <XmlRpc.h>
#else
#include <xmlrpcpp/XmlRpc.h>
#endif

#include <parameter.hpp>

#include <regex>
#include <string>
#include <vector>

namespace cobridge
{

cobridge_base::Parameter from_ros_param(
  const std::string & name,
  const XmlRpc::XmlRpcValue & value);
XmlRpc::XmlRpcValue to_ros_param(const cobridge_base::ParameterValue & param);
std::vector<std::regex> parse_regex_patterns(const std::vector<std::string> & strings);

}
#endif  // PARAM_UTILS_HPP_
