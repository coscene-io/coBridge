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

#include <rclcpp_components/component_manager.hpp>
#include <memory>

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  size_t num_threads = 0;
  {
    // Temporary dummy node to get num_threads param.
    auto dummy_node = std::make_shared<rclcpp::Node>("dummy");
    auto num_threads_description = rcl_interfaces::msg::ParameterDescriptor{};
    num_threads_description.name = "num_threads";
    num_threads_description.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
    num_threads_description.description =
      "The number of threads to use for the ROS node executor. 0 means one thread per CPU core.";
    num_threads_description.read_only = true;
    num_threads_description.additional_constraints = "Must be a non-negative integer";
    num_threads_description.integer_range.resize(1);
    num_threads_description.integer_range[0].from_value = 0;
    num_threads_description.integer_range[0].to_value = INT32_MAX;
    num_threads_description.integer_range[0].step = 1;
    constexpr int DEFAULT_NUM_THREADS = 0;
    dummy_node->declare_parameter(
      num_threads_description.name, DEFAULT_NUM_THREADS,
      num_threads_description);
    num_threads =
      static_cast<size_t>(dummy_node->get_parameter(num_threads_description.name).as_int());
  }

  auto executor =
    rclcpp::executors::MultiThreadedExecutor::make_shared(rclcpp::ExecutorOptions{}, num_threads);

  rclcpp_components::ComponentManager component_manager(executor,
    "cobridge_component_manager");
  const auto component_resources = component_manager.get_component_resources("cobridge");

  if (component_resources.empty()) {
    RCLCPP_INFO(component_manager.get_logger(), "No loadable resources found");
    return EXIT_FAILURE;
  }

  auto component_factory = component_manager.create_component_factory(component_resources.front());
  auto node = component_factory->create_node_instance(rclcpp::NodeOptions());

  executor->add_node(node.get_node_base_interface());
  executor->spin();
  rclcpp::shutdown();

  return EXIT_SUCCESS;
}
