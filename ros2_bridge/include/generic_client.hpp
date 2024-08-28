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
#ifndef GENERIC_CLIENT_HPP_
#define GENERIC_CLIENT_HPP_

#include <rclcpp/client.hpp>
#include <rclcpp/serialized_message.hpp>
#include <rcpputils/shared_library.hpp>

#include <future>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace cobridge
{

class GenericClient : public rclcpp::ClientBase
{
public:
  using SharedRequest = std::shared_ptr<rclcpp::SerializedMessage>;
  using SharedResponse = std::shared_ptr<rclcpp::SerializedMessage>;
  using Promise = std::promise<SharedResponse>;
  using PromiseWithRequest = std::promise<std::pair<SharedRequest, SharedResponse>>;
  using SharedPromise = std::shared_ptr<Promise>;
  using SharedPromiseWithRequest = std::shared_ptr<PromiseWithRequest>;
  using SharedFuture = std::shared_future<SharedResponse>;
  using SharedFutureWithRequest = std::shared_future<std::pair<SharedRequest, SharedResponse>>;
  using CallbackType = std::function<void (SharedFuture)>;
  using CallbackWithRequestType = std::function<void (SharedFutureWithRequest)>;

  using SharedPtr = std::shared_ptr<GenericClient>;
  using ConstSharedPtr = std::shared_ptr<const GenericClient>;

  template<typename ... Args>
  static std::shared_ptr<GenericClient> make_shared(Args && ... args)
  {
    return std::make_shared<GenericClient>(std::forward<Args>(args)...);
  }

  GenericClient(
    rclcpp::node_interfaces::NodeBaseInterface * node_base,
    rclcpp::node_interfaces::NodeGraphInterface::SharedPtr node_graph,
    std::string service_name, std::string service_type,
    rcl_client_options_t & client_options);

  GenericClient(const GenericClient &) = delete;

  GenericClient & operator=(const GenericClient &) = delete;

  ~GenericClient() override = default;


  std::shared_ptr<void> create_response() override;

  std::shared_ptr<rmw_request_id_t> create_request_header() override;

  void handle_response(
    std::shared_ptr<rmw_request_id_t> request_header,
    std::shared_ptr<void> response) override;

  SharedFuture async_send_request(SharedRequest request);

  SharedFuture async_send_request(SharedRequest request, CallbackType && cb);

private:
  std::map<int64_t, std::tuple<SharedPromise, CallbackType, SharedFuture>> pending_requests_;
  std::mutex pending_requests_mutex_;
  std::shared_ptr<rcpputils::SharedLibrary> _type_support_lib;
  std::shared_ptr<rcpputils::SharedLibrary> _type_introspection_lib;
  const rosidl_service_type_support_t * _service_type_support_handle;
  const rosidl_message_type_support_t * _request_type_support_handle;
  const rosidl_message_type_support_t * _response_type_support_handle;
  const rosidl_service_type_support_t * _type_introspection_handle;
};

}  // namespace cobridge

#endif  // GENERIC_CLIENT_HPP_
