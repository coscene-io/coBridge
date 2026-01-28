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

#include <rclcpp/client.hpp>
#include <rclcpp/serialized_message.hpp>
#include <typesupport_helpers.hpp>
#include <rosidl_typesupport_introspection_cpp/field_types.hpp>
#include <rosidl_typesupport_introspection_cpp/service_introspection.hpp>

#include <generic_client.hpp>

#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace cobridge
{

constexpr char TYPESUPPORT_INTROSPECTION_LIB_NAME[] = "rosidl_typesupport_introspection_cpp";
constexpr char TYPESUPPORT_LIB_NAME[] = "rosidl_typesupport_cpp";
using rosidl_typesupport_introspection_cpp::MessageMembers;
using rosidl_typesupport_introspection_cpp::ServiceMembers;

std::shared_ptr<void> allocate_message(const MessageMembers * members)
{
  void * buffer = malloc(members->size_of_);
  if (buffer == nullptr) {
    throw std::runtime_error("Failed to allocate memory");
  }
  memset(buffer, 0, members->size_of_);
  members->init_function(buffer, rosidl_runtime_cpp::MessageInitialization::ALL);
  return std::shared_ptr<void>(buffer, free);
}

std::string get_type_introspection_symbol_name(const std::string & serviceType)
{
  const auto [pkg_name, middle_module, type_name] = extract_type_identifier(serviceType);

  return std::string(TYPESUPPORT_INTROSPECTION_LIB_NAME) + "__get_service_type_support_handle__" +
         pkg_name + "__" + (middle_module.empty() ? "srv" : middle_module) + "__" + type_name;
}

/**
 * The default symbol names for getting type support handles for services are missing from the
 * rosidl_typesupport_cpp shared libraries, see
 * https://github.com/ros2/rosidl_typesupport/issues/122
 *
 * We can however, as a (hacky) workaround, use other symbols defined in the shared library.
 * With `nm -C -D /opt/ros/humble/lib/libtest_msgs__rosidl_typesupport_cpp.so` we see that there is
 * `rosidl_service_type_support_t const*
 * rosidl_typesupport_cpp::get_service_type_support_handle<test_msgs::srv::BasicTypes>()` which
 * mangled becomes
 * `_ZN22rosidl_typesupport_cpp31get_service_type_support_handleIN9test_msgs3srv10BasicTypesEEEPK29rosidl_service_type_support_tv`
 * This is the same for galactic, humble and rolling (tested with gcc / clang)
 *
 * This function produces the mangled symbol name for a given service type.
 *
 * \param[in] service_type The service type, e.g. "test_msgs/srv/BasicTypes"
 * \return Symbol name for getting the service type support handle
 */
std::string get_service_type_support_handle_symbol_name(const std::string & service_type)
{
  const auto [pkg_name, middle_module, type_name] = extract_type_identifier(service_type);
  const auto length_prefixed_string = [](const std::string & s) {
      return std::to_string(s.size()) + s;
    };

  return "_ZN" + length_prefixed_string(TYPESUPPORT_LIB_NAME) +
         length_prefixed_string("get_service_type_support_handle") + "IN" +
         length_prefixed_string(pkg_name) +
         length_prefixed_string(middle_module.empty() ? "srv" : middle_module) +
         length_prefixed_string(type_name) + "EEEPK" +
         length_prefixed_string("rosidl_service_type_support_t") + "v";
}

GenericClient::GenericClient(
  rclcpp::node_interfaces::NodeBaseInterface * node_base,
  rclcpp::node_interfaces::NodeGraphInterface::SharedPtr node_graph,
  std::string service_name, std::string service_type,
  rcl_client_options_t & client_options)
: rclcpp::ClientBase(node_base, node_graph)
{
  const auto [pkg_name, middle_module, type_name] = extract_type_identifier(service_type);
  const auto request_type_name = service_type + "_Request";
  const auto response_type_name = service_type + "_Response";

  _type_support_lib = cobridge::get_typesupport_library(service_type, TYPESUPPORT_LIB_NAME);
  _type_introspection_lib =
    cobridge::get_typesupport_library(service_type, TYPESUPPORT_INTROSPECTION_LIB_NAME);
  if (!_type_support_lib || !_type_introspection_lib) {
    throw std::runtime_error("Failed to load shared library for service type " + service_type);
  }

  const auto type_support_symbol_name = get_service_type_support_handle_symbol_name(service_type);
  if (!_type_support_lib->has_symbol(type_support_symbol_name)) {
    throw std::runtime_error(
            "Failed to find symbol '" + type_support_symbol_name + "' in " +
            _type_support_lib->get_library_path());
  }

  const rosidl_service_type_support_t * (* get_ts)() = nullptr;
  _service_type_support_handle =
    (reinterpret_cast<decltype(get_ts)>(_type_support_lib->get_symbol(type_support_symbol_name)))();

  const auto type_introspection_symbol_name = get_type_introspection_symbol_name(service_type);

  // This will throw runtime_error if the symbol was not found.
  _type_introspection_handle = (reinterpret_cast<decltype(get_ts)>(
      _type_introspection_lib->get_symbol(type_introspection_symbol_name)))();

  _request_type_support_handle =
    cobridge::get_typesupport_handle(request_type_name, TYPESUPPORT_LIB_NAME, _type_support_lib);
  _response_type_support_handle =
    cobridge::get_typesupport_handle(response_type_name, TYPESUPPORT_LIB_NAME, _type_support_lib);

  rcl_ret_t ret = rcl_client_init(
    this->get_client_handle().get(), this->get_rcl_node_handle(),
    _service_type_support_handle, service_name.c_str(), &client_options);
  if (ret != RCL_RET_OK) {
    if (ret == RCL_RET_SERVICE_NAME_INVALID) {
      auto rcl_node_handle = this->get_rcl_node_handle();
      // this will throw on any validation problem
      rcl_reset_error();
      rclcpp::expand_topic_or_service_name(
        service_name, rcl_node_get_name(rcl_node_handle),
        rcl_node_get_namespace(rcl_node_handle), true);
    }
    rclcpp::exceptions::throw_from_rcl_error(ret, "could not create client");
  }
}

std::shared_ptr<void> GenericClient::create_response()
{
  auto srv_members = static_cast<const ServiceMembers *>(_type_introspection_handle->data);
  return allocate_message(srv_members->response_members_);
}

std::shared_ptr<rmw_request_id_t> GenericClient::create_request_header()
{
  return std::make_shared<rmw_request_id_t>();
}

void GenericClient::handle_response(
  std::shared_ptr<rmw_request_id_t> request_header,
  std::shared_ptr<void> response)
{
  std::unique_lock<std::mutex> lock(pending_requests_mutex_);
  int64_t sequence_number = request_header->sequence_number;

  auto ser_response = std::make_shared<rclcpp::SerializedMessage>();
  rmw_ret_t r = rmw_serialize(
    response.get(), _response_type_support_handle,
    &ser_response->get_rcl_serialized_message());
  if (r != RMW_RET_OK) {
    RCUTILS_LOG_ERROR_NAMED("cobridge", "Failed to serialize service response. Ignoring...");
    return;
  }

  // TODO(esteve) this should throw instead since it is not expected to happen in the first place
  if (this->pending_requests_.count(sequence_number) == 0) {
    RCUTILS_LOG_ERROR_NAMED("cobridge", "Received invalid sequence number. Ignoring...");
    return;
  }
  auto tuple = this->pending_requests_[sequence_number];
  auto call_promise = std::get<0>(tuple);
  auto callback = std::get<1>(tuple);
  auto future = std::get<2>(tuple);
  this->pending_requests_.erase(sequence_number);
  // Unlock here to allow the service to be called recursively from one of its callbacks.
  lock.unlock();

  call_promise->set_value(ser_response);
  callback(future);
}

GenericClient::SharedFuture GenericClient::async_send_request(SharedRequest request)
{
  return async_send_request(request, [](SharedFuture) {});
}

GenericClient::SharedFuture GenericClient::async_send_request(
  SharedRequest request,
  CallbackType && cb)
{
  std::lock_guard<std::mutex> lock(pending_requests_mutex_);
  int64_t sequence_number;

  auto srv_members = static_cast<const ServiceMembers *>(_type_introspection_handle->data);
  auto buf = allocate_message(srv_members->request_members_);

  const rmw_serialized_message_t * sm = &request->get_rcl_serialized_message();
  if (const auto ret = rmw_deserialize(sm, _request_type_support_handle, buf.get()) != RCL_RET_OK) {
    rclcpp::exceptions::throw_from_rcl_error(ret, "failed to desirialize request");
  }
  rcl_ret_t ret = rcl_send_request(get_client_handle().get(), buf.get(), &sequence_number);
  if (RCL_RET_OK != ret) {
    rclcpp::exceptions::throw_from_rcl_error(ret, "failed to send request");
  }

  SharedPromise call_promise = std::make_shared<Promise>();
  SharedFuture f(call_promise->get_future());
  pending_requests_[sequence_number] =
    std::make_tuple(call_promise, std::forward<CallbackType>(cb), f);
  return f;
}

}  // namespace cobridge
