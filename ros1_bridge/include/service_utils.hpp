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
#ifndef SERVICE_UTILS_HPP_
#define SERVICE_UTILS_HPP_

#include <chrono>
#include <string>

namespace cobridge
{

/**
 * Opens a socket to the service server and retrieves the service type from the connection header.
 * This is necessary as the service type is not stored on the ROS master.
 */
std::string retrieve_service_type(
  const std::string & service_name,
  std::chrono::milliseconds timeout_ms);

}  // namespace cobridge
#endif  // SERVICE_UTILS_HPP_
