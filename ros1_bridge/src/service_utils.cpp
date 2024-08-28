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


#include <ros/connection.h>
#include <ros/service_manager.h>
#include <ros/service_server_link.h>

#include <service_utils.hpp>

#include <chrono>
#include <future>
#include <string>

namespace cobridge
{

std::string retrieveServiceType(const std::string & serviceName, std::chrono::milliseconds timeout)
{
  auto link = ros::ServiceManager::instance()->createServiceServerLink(
    serviceName, false, "*", "*",
    {{"probe", "1"}});
  if (!link) {
    throw std::runtime_error("Failed to create service link");
  } else if (!link->getConnection()) {
    throw std::runtime_error("Failed to get service link connection");
  }

  std::promise<std::string> promise;
  auto future = promise.get_future();

  link->getConnection()->setHeaderReceivedCallback(
    [&promise](const ros::ConnectionPtr & conn, const ros::Header & header) {
      std::string serviceType;
      if (header.getValue("type", serviceType)) {
        promise.set_value(serviceType);
      } else {
        promise.set_exception(
          std::make_exception_ptr(
            std::runtime_error("Key 'type' not found in service connection header")));
      }
      // Close connection since we don't need it any more.
      conn->drop(ros::Connection::DropReason::Destructing);
      return true;
    });

  if (future.wait_for(timeout) != std::future_status::ready) {
    // Drop connection here, removes the link from the service manager instance and avoids
    // that the header-received callback is called after the promise has already been destroyed.
    link->getConnection()->drop(ros::Connection::DropReason::Destructing);
    throw std::runtime_error("Timed out when retrieving service type");
  }

  return future.get();
}

}  // namespace cobridge
