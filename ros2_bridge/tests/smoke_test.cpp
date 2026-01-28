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
#include <filesystem>

#include <gtest/gtest.h>
#include <rclcpp_components/component_manager.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/set_bool.hpp>
#include <websocketpp/config/asio_client.hpp>

#include <json.hpp>
#include <test/test_client.hpp>
#include <websocket_client.hpp>

#include <chrono>
#include <future>
#include <thread>
#include <string>
#include <vector>
#include <memory>

constexpr char URI[] = "ws://localhost:21274";

// Binary representation of std_msgs/msg/String for "hello world"
constexpr uint8_t HELLO_WORLD_BINARY[] = {0, 1, 0, 0, 12, 0, 0, 0, 104, 101,
  108, 108, 111, 32, 119, 111, 114, 108, 100, 0};

constexpr auto THREE_SECOND = std::chrono::seconds(3);
constexpr auto DEFAULT_TIMEOUT = std::chrono::seconds(10);

using json = nlohmann::json;

void CompareJsonWithoutFields(
  const std::string & jsonStr1, const std::string & jsonStr2,
  const std::vector<std::string> & keysToErase = {"sessionId", "metadata"})
{
  json obj1 = json::parse(jsonStr1);
  json obj2 = json::parse(jsonStr2);

  for (const auto & key : keysToErase) {
    obj1.erase(key);
    obj2.erase(key);
  }

  EXPECT_EQ(obj1, obj2);
}

class ParameterTest : public ::testing::Test
{
public:
  using PARAM_1_TYPE = std::string;
  inline static const std::string NODE_1_NAME = "node_1";
  inline static const std::string PARAM_1_NAME = "string_param";
  inline static const PARAM_1_TYPE PARAM_1_DEFAULT_VALUE = "hello";
  inline static const std::string DELETABLE_PARAM_NAME = "deletable_param";

  using PARAM_2_TYPE = std::vector<int64_t>;
  inline static const std::string NODE_2_NAME = "node_2";
  inline static const std::string PARAM_2_NAME = "int_array_param";
  inline static const PARAM_2_TYPE PARAM_2_DEFAULT_VALUE = {1, 2, 3};

  using PARAM_3_TYPE = double;
  inline static const std::string PARAM_3_NAME = "float_param";
  inline static const PARAM_3_TYPE PARAM_3_DEFAULT_VALUE = 1.123;

  using PARAM_4_TYPE = std::vector<double>;
  inline static const std::string PARAM_4_NAME = "float_array_param";
  inline static const PARAM_4_TYPE PARAM_4_DEFAULT_VALUE = {1.1, 2.2, 3.3};

protected:
  void SetUp() override
  {
    auto nodeOptions = rclcpp::NodeOptions();
    nodeOptions.allow_undeclared_parameters(true);
    _paramNode1 = rclcpp::Node::make_shared(NODE_1_NAME, nodeOptions);
    auto p1Param = rcl_interfaces::msg::ParameterDescriptor{};
    p1Param.name = PARAM_1_NAME;
    p1Param.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
    p1Param.read_only = false;
    _paramNode1->declare_parameter(p1Param.name, PARAM_1_DEFAULT_VALUE, p1Param);
    _paramNode1->set_parameter(rclcpp::Parameter(DELETABLE_PARAM_NAME, true));

    _paramNode2 = rclcpp::Node::make_shared(NODE_2_NAME);
    auto p2Param = rcl_interfaces::msg::ParameterDescriptor{};
    p2Param.name = PARAM_2_NAME;
    p2Param.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER_ARRAY;
    p2Param.read_only = false;
    _paramNode2->declare_parameter(p2Param.name, PARAM_2_DEFAULT_VALUE, p2Param);
    _paramNode2->declare_parameter(PARAM_3_NAME, PARAM_3_DEFAULT_VALUE);
    _paramNode2->declare_parameter(PARAM_4_NAME, PARAM_4_DEFAULT_VALUE);

    _executor.add_node(_paramNode1);
    _executor.add_node(_paramNode2);
    _executorThread = std::thread(
      [this]() {
        _executor.spin();
      });

    _wsClient = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
    ASSERT_EQ(std::future_status::ready, _wsClient->connect(URI).wait_for(DEFAULT_TIMEOUT));

    _wsClient->login("test user", "test-user-id-0000");
  }

  void TearDown() override
  {
    _executor.cancel();
    _executorThread.join();
  }

  rclcpp::executors::SingleThreadedExecutor _executor;
  rclcpp::Node::SharedPtr _paramNode1;
  rclcpp::Node::SharedPtr _paramNode2;
  std::thread _executorThread;
  std::shared_ptr<cobridge_base::Client<websocketpp::config::asio_client>> _wsClient;
};

class ServiceTest : public ::testing::Test
{
public:
  inline static const std::string SERVICE_NAME = "/foo_service";

protected:
  void SetUp() override
  {
    _node = rclcpp::Node::make_shared("node");
    _service = _node->create_service<std_srvs::srv::SetBool>(
      SERVICE_NAME, [&](std::shared_ptr<std_srvs::srv::SetBool::Request> req,
      std::shared_ptr<std_srvs::srv::SetBool::Response> res) {
        res->message = "hello";
        res->success = req->data;
      });

    _executor.add_node(_node);
    _executorThread = std::thread(
      [this]() {
        _executor.spin();
      });
  }

  void TearDown() override
  {
    _executor.cancel();
    _executorThread.join();
  }

  rclcpp::executors::SingleThreadedExecutor _executor;
  rclcpp::Node::SharedPtr _node;
  rclcpp::ServiceBase::SharedPtr _service;
  std::thread _executorThread;
  std::shared_ptr<cobridge_base::Client<websocketpp::config::asio_client>> _wsClient;
};

class ExistingPublisherTest : public ::testing::Test
{
public:
  inline static const std::string TOPIC_NAME = "/some_topic";

protected:
  void SetUp() override
  {
    _node = rclcpp::Node::make_shared("node");
    _publisher =
      _node->create_publisher<std_msgs::msg::String>(TOPIC_NAME, rclcpp::SystemDefaultsQoS());
    _executor.add_node(_node);
    _executorThread = std::thread(
      [this]() {
        _executor.spin();
      });
  }

  void TearDown() override
  {
    _executor.cancel();
    _executorThread.join();
  }

  rclcpp::executors::SingleThreadedExecutor _executor;
  rclcpp::Node::SharedPtr _node;
  rclcpp::PublisherBase::SharedPtr _publisher;
  std::thread _executorThread;
};

template<class T>
std::shared_ptr<rclcpp::SerializedMessage> serializeMsg(const T * msg)
{
  using rosidl_typesupport_cpp::get_message_type_support_handle;
  auto type_support_hdl = get_message_type_support_handle<T>();
  auto result = std::make_shared<rclcpp::SerializedMessage>();
  rmw_ret_t ret = rmw_serialize(msg, type_support_hdl, &result->get_rcl_serialized_message());
  EXPECT_EQ(ret, RMW_RET_OK);
  return result;
}

template<class T>
std::shared_ptr<T> deserializeMsg(const rcl_serialized_message_t * msg)
{
  using rosidl_typesupport_cpp::get_message_type_support_handle;
  auto type_support_hdl = get_message_type_support_handle<T>();
  auto result = std::make_shared<T>();
  rmw_ret_t ret = rmw_deserialize(msg, type_support_hdl, result.get());
  EXPECT_EQ(ret, RMW_RET_OK);
  return result;
}

TEST(SmokeTest, testMultiConnection) {
  auto client_0 = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  auto client0_login_future = cobridge_base::wait_for_login(client_0, "login");
  EXPECT_EQ(std::future_status::ready, client_0->connect(URI).wait_for(DEFAULT_TIMEOUT));
  EXPECT_EQ(std::future_status::ready, client0_login_future.wait_for(THREE_SECOND));
  CompareJsonWithoutFields(
    "{\"op\":\"login\",\"userId\":\"\",\"username\":\"\"}",
    client0_login_future.get(),
    {"infoPort", "lanCandidates", "macAddr", "linkType"}
  );
  client_0->login("user_0", "test-user-id-0000");

  auto client_1 = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  auto client1_login_future = cobridge_base::wait_for_login(client_1, "login");
  EXPECT_EQ(std::future_status::ready, client_1->connect(URI).wait_for(DEFAULT_TIMEOUT));
  EXPECT_EQ(std::future_status::ready, client1_login_future.wait_for(THREE_SECOND));
  CompareJsonWithoutFields(
    "{\"op\":\"login\",\"userId\":\"test-user-id-0000\","
    "\"username\":\"user_0\"}", client1_login_future.get(),
    {"infoPort", "lanCandidates", "macAddr", "linkType"});

  auto client0_kicked_future = cobridge_base::wait_for_kicked(client_0);
  auto server_info_future = cobridge_base::wait_for_login(client_1, "serverInfo");
  client_1->login("user_1", "test-user-id-0001");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  EXPECT_EQ(std::future_status::ready, client0_kicked_future.wait_for(THREE_SECOND));
  EXPECT_EQ(std::future_status::ready, server_info_future.wait_for(THREE_SECOND));
  EXPECT_EQ(
    "{\"message\":\"The client was forcibly disconnected by the server.\",\"op\":\"kicked\","
    "\"userId\":\"test-user-id-0001\",\"username\":\"user_1\"}", client0_kicked_future.get());
  client_0->close();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  CompareJsonWithoutFields(
    "{\"capabilities\":[\"clientPublish\",\"connectionGraph\","
    "\"parametersSubscribe\",\"parameters\",\"services\",\"assets\",\"messageTime\"],"
    "\"metadata\":{\"ROS_DISTRO\":\"foxy\"},\"name\":\"cobridge\","
    "\"op\":\"serverInfo\",\"sessionId\":\"1727148359\","
    "\"supportedEncodings\":[\"cdr\"]}", server_info_future.get());
}

TEST(SmokeTest, testSubscription) {
  // Publish a string message on a latched ros topic
  const std::string topic_name = "/pub_topic";
  std_msgs::msg::String ros_msg;
  ros_msg.data = "hello world";

  auto node = rclcpp::Node::make_shared("tester");
  rclcpp::QoS qos = rclcpp::QoS{rclcpp::KeepLast(1lu)};
  qos.reliable();
  qos.transient_local();
  auto pub = node->create_publisher<std_msgs::msg::String>(topic_name, qos);
  pub->publish(ros_msg);

  // Connect a few clients and make sure that they receive the correct message
  const auto client_count = 3;
  for (auto i = 0; i < client_count; ++i) {
    // Set up a client and subscribe to the channel.
    auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
    auto channel_future = cobridge_base::wait_for_channel(client, topic_name);
    ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));
    client->login("test user", "test-user-id-0000");
    ASSERT_EQ(std::future_status::ready, channel_future.wait_for(THREE_SECOND));
    const cobridge_base::Channel channel = channel_future.get();
    const cobridge_base::SubscriptionId subscription_id = 1;

    // Subscribe to the channel and confirm that the promise resolves
    auto msg_future = cobridge_base::wait_for_channel_msg(client.get(), subscription_id);
    client->subscribe({{subscription_id, channel.id}});
    ASSERT_EQ(std::future_status::ready, msg_future.wait_for(DEFAULT_TIMEOUT));
    const auto msg_data = msg_future.get();
    ASSERT_EQ(sizeof(HELLO_WORLD_BINARY), msg_data.size());
    EXPECT_EQ(0, std::memcmp(HELLO_WORLD_BINARY, msg_data.data(), msg_data.size()));

    // Unsubscribe from the channel again.
    client->unsubscribe({subscription_id});
  }
}

TEST(SmokeTest, testPublishing) {
  cobridge_base::ClientAdvertisement advertisement;
  advertisement.channel_id = 1;
  advertisement.topic = "/foo";
  advertisement.encoding = "cdr";
  advertisement.schema_name = "std_msgs/String";

  // Set up a ROS node with a subscriber
  std::promise<std::string> msg_promise;
#ifdef ROS2_VERSION_FOXY
  auto msg_future = msg_promise.get_future().share();
#else
  auto msg_future = msg_promise.get_future();
#endif
  auto node = rclcpp::Node::make_shared("tester");
  auto sub = node->create_subscription<std_msgs::msg::String>(
    advertisement.topic, 10, [&msg_promise](const std_msgs::msg::String::SharedPtr msg) {
      msg_promise.set_value(msg->data);
    });
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);

  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));

  client->login("test user", "test-user-id-0000");
  client->advertise({advertisement});

  // Wait until the advertisement got advertised as channel by the server
  auto channelFuture = cobridge_base::wait_for_channel(client, advertisement.topic);
  ASSERT_EQ(std::future_status::ready, channelFuture.wait_for(THREE_SECOND));

  // Publish the message and unadvertise again
  client->publish(advertisement.channel_id, HELLO_WORLD_BINARY, sizeof(HELLO_WORLD_BINARY));
#ifdef ROS2_VERSION_FOXY
  const auto ret = executor.spin_until_future_complete(msg_future, THREE_SECOND);
#else
  const auto ret = executor.spin_until_future_complete(msg_future, THREE_SECOND);
#endif
  client->unadvertise({advertisement.channel_id});
  ASSERT_EQ(rclcpp::FutureReturnCode::SUCCESS, ret);
  EXPECT_EQ("hello world", msg_future.get());
}

TEST_F(ExistingPublisherTest, testPublishingWithExistingPublisher) {
  cobridge_base::ClientAdvertisement advertisement;
  advertisement.channel_id = 1;
  advertisement.topic = TOPIC_NAME;
  advertisement.encoding = "cdr";
  advertisement.schema_name = "std_msgs/String";

  // Set up a ROS node with a subscriber
  std::promise<std::string> msg_promise;
#ifdef ROS2_VERSION_FOXY
  auto msg_future = msg_promise.get_future().share();
#else
  auto msg_future = msg_promise.get_future();
#endif
  auto node = rclcpp::Node::make_shared("tester");
  auto sub = node->create_subscription<std_msgs::msg::String>(
    advertisement.topic, 10, [&msg_promise](const std_msgs::msg::String::SharedPtr msg) {
      msg_promise.set_value(msg->data);
    });
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);

  // Set up the client, advertise and publish the binary message
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));

  client->login("test user", "test-user-id-0000");
  client->advertise({advertisement});

  // Wait until the advertisement got advertised as channel by the server
  auto channelFuture = cobridge_base::wait_for_channel(client, advertisement.topic);
  ASSERT_EQ(std::future_status::ready, channelFuture.wait_for(THREE_SECOND));

  // Publish the message and unadvertise again
  client->publish(advertisement.channel_id, HELLO_WORLD_BINARY, sizeof(HELLO_WORLD_BINARY));

#ifdef ROS2_VERSION_FOXY
  const auto ret = executor.spin_until_future_complete(msg_future, THREE_SECOND);
#else
  const auto ret = executor.spin_until_future_complete(msg_future, THREE_SECOND);
#endif
  client->unadvertise({advertisement.channel_id});

  ASSERT_EQ(rclcpp::FutureReturnCode::SUCCESS, ret);
  EXPECT_EQ("hello world", msg_future.get());
}

TEST_F(ParameterTest, testGetAllParams) {
  const std::string requestId = "req-testGetAllParams";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->get_parameters({}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_GE(params.size(), 2UL);
}

TEST_F(ParameterTest, testGetNonExistingParameters) {
  const std::string requestId = "req-testGetNonExistingParameters";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->get_parameters(
    {"/foo_1.non_existing_parameter", "/foo_2.non_existing.nested_parameter"},
    optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_TRUE(params.empty());
}

TEST_F(ParameterTest, testGetParameters) {
  const auto p1 = NODE_1_NAME + "." + PARAM_1_NAME;
  const auto p2 = NODE_2_NAME + "." + PARAM_2_NAME;

  const std::string requestId = "req-testGetParameters";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->get_parameters({p1, p2}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(2UL, params.size());
  auto p1Iter = std::find_if(
    params.begin(), params.end(), [&p1](const auto & param) {
      return param.get_name() == p1;
    });
  auto p2Iter = std::find_if(
    params.begin(), params.end(), [&p2](const auto & param) {
      return param.get_name() == p2;
    });
  ASSERT_NE(p1Iter, params.end());
  EXPECT_EQ(PARAM_1_DEFAULT_VALUE, p1Iter->get_value().getValue<PARAM_1_TYPE>());
  ASSERT_NE(p2Iter, params.end());

  std::vector<int64_t> int_array_val;
  const auto array_params =
    p2Iter->get_value().getValue<std::vector<cobridge_base::ParameterValue>>();
  for (const auto & paramValue : array_params) {
    int_array_val.push_back(paramValue.getValue<int64_t>());
  }
  EXPECT_EQ(int_array_val, PARAM_2_DEFAULT_VALUE);
}

TEST_F(ParameterTest, testSetParameters) {
  const auto p1 = NODE_1_NAME + "." + PARAM_1_NAME;
  const auto p2 = NODE_2_NAME + "." + PARAM_2_NAME;
  const PARAM_1_TYPE newP1value = "world";
  const std::vector<cobridge_base::ParameterValue> newP2value =
  {cobridge_base::ParameterValue(4),
    cobridge_base::ParameterValue(5),
    cobridge_base::ParameterValue(6)
  };

  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(p1, cobridge_base::ParameterValue(newP1value)),
    cobridge_base::Parameter(p2, cobridge_base::ParameterValue(newP2value)),
  };

  _wsClient->set_parameters(parameters);
  const std::string requestId = "req-testSetParameters";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->get_parameters({p1, p2}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(2UL, params.size());
  auto p1Iter = std::find_if(
    params.begin(), params.end(), [&p1](const auto & param) {
      return param.get_name() == p1;
    });
  auto p2Iter = std::find_if(
    params.begin(), params.end(), [&p2](const auto & param) {
      return param.get_name() == p2;
    });
  ASSERT_NE(p1Iter, params.end());
  EXPECT_EQ(newP1value, p1Iter->get_value().getValue<PARAM_1_TYPE>());
  ASSERT_NE(p2Iter, params.end());

  std::vector<int64_t> int_array_val;
  const auto array_params = p2Iter->get_value()
    .getValue<std::vector<cobridge_base::ParameterValue>>();
  for (const auto & paramValue : array_params) {
    int_array_val.push_back(paramValue.getValue<int64_t>());
  }
  const std::vector<int64_t> expected_value = {4, 5, 6};
  EXPECT_EQ(int_array_val, expected_value);
}

TEST_F(ParameterTest, testSetParametersWithReqId) {
  const auto p1 = NODE_1_NAME + "." + PARAM_1_NAME;
  const PARAM_1_TYPE newP1value = "world";
  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(p1, cobridge_base::ParameterValue(newP1value)),
  };

  const std::string requestId = "req-testSetParameters";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->set_parameters(parameters, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(1UL, params.size());
}

TEST_F(ParameterTest, testSetFloatParametersWithIntegers) {
  const auto floatParamName = NODE_2_NAME + "." + PARAM_3_NAME;
  const auto floatArrayParamName = NODE_2_NAME + "." + PARAM_4_NAME;
  const int64_t floatParamVal = 10;
  const std::vector<int64_t> floatArrayParamVal = {3, 2, 1};
  const std::string requestId = "req-testSetFloatParametersWithIntegers";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  const nlohmann::json::array_t parameters = {
    {{"name", floatParamName}, {"value", floatParamVal}, {"type", "float64"}},
    {{"name", floatArrayParamName}, {"value", floatArrayParamVal}, {"type", "float64_array"}},
  };
  _wsClient->send_text(
    nlohmann::json{{"op", "setParameters"}, {"id", requestId},
      {"parameters", parameters}}.dump());
  ASSERT_EQ(std::future_status::ready, future.wait_for(THREE_SECOND));
  std::vector<cobridge_base::Parameter> params = future.get();

  {
    const auto param =
      std::find_if(
      params.begin(), params.end(), [floatParamName](const cobridge_base::Parameter & p) {
        return p.get_name() == floatParamName;
      });
    ASSERT_NE(param, params.end());
    EXPECT_EQ(param->get_type(), cobridge_base::ParameterType::PARAMETER_DOUBLE);
    EXPECT_NEAR(
      param->get_value().getValue<double>(),
      static_cast<double>(floatParamVal), 1e-9);
  }
  {
    const auto param = std::find_if(
      params.begin(), params.end(),
      [floatArrayParamName](const cobridge_base::Parameter & p) {
        return p.get_name() == floatArrayParamName;
      });
    ASSERT_NE(param, params.end());
    EXPECT_EQ(param->get_type(), cobridge_base::ParameterType::PARAMETER_ARRAY);
    const auto paramValue = param->get_value()
      .getValue<std::vector<cobridge_base::ParameterValue>>();
    ASSERT_EQ(paramValue.size(), floatArrayParamVal.size());
    for (size_t i = 0; i < paramValue.size(); ++i) {
      EXPECT_NEAR(
        paramValue[i].getValue<double>(), static_cast<double>(floatArrayParamVal[i]),
        1e-9);
    }
  }
}

TEST_F(ParameterTest, testUnsetParameter) {
  const auto p1 = NODE_1_NAME + "." + DELETABLE_PARAM_NAME;
  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(p1),
  };

  const std::string requestId = "req-testUnsetParameter";
  auto future = cobridge_base::wait_for_parameters(_wsClient, requestId);
  _wsClient->set_parameters(parameters, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(0UL, params.size());
}

TEST_F(ParameterTest, testParameterSubscription) {
  const auto p1 = NODE_1_NAME + "." + PARAM_1_NAME;

  _wsClient->subscribe_parameter_updates({p1});
  auto future = cobridge_base::wait_for_parameters(_wsClient);
  _wsClient->set_parameters(
    {cobridge_base::Parameter(p1, cobridge_base::ParameterValue("foo"))}
  );
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  ASSERT_EQ(1UL, params.size());
  EXPECT_EQ(params.front().get_name(), p1);

  _wsClient->unsubscribe_parameter_updates({p1});
  _wsClient->set_parameters(
    {cobridge_base::Parameter(p1, cobridge_base::ParameterValue("bar"))}
  );

  future = cobridge_base::wait_for_parameters(_wsClient);
  ASSERT_EQ(std::future_status::timeout, future.wait_for(THREE_SECOND));
}

TEST_F(ServiceTest, testCallService) {
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();

  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));

  client->login("test user", "test-user-id-0000");
  auto service_future = cobridge_base::wait_for_service(client, SERVICE_NAME);
  ASSERT_EQ(std::future_status::ready, service_future.wait_for(DEFAULT_TIMEOUT));

  const cobridge_base::Service service = service_future.get();
  std_srvs::srv::SetBool::Request request_msg;
  request_msg.data = true;
  const auto serialized_request = serializeMsg(&request_msg);
  const auto & ser_request_msg = serialized_request->get_rcl_serialized_message();

  cobridge_base::ServiceRequest request;
  request.service_id = service.id;
  request.call_id = 123lu;
  request.encoding = "cdr";
  request.serv_data.resize(ser_request_msg.buffer_length);
  std::memcpy(request.serv_data.data(), ser_request_msg.buffer, ser_request_msg.buffer_length);

  auto future = cobridge_base::wait_for_service_response(client);
  client->send_service_request(request);
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  cobridge_base::ServiceResponse response;
  EXPECT_NO_THROW(response = future.get());
  EXPECT_EQ(response.service_id, request.service_id);
  EXPECT_EQ(response.call_id, request.call_id);
  EXPECT_EQ(response.encoding, request.encoding);

  rclcpp::SerializedMessage serialized_response_msg(response.serv_data.size());
  auto & ser_msg = serialized_response_msg.get_rcl_serialized_message();
  std::memcpy(ser_msg.buffer, response.serv_data.data(), response.serv_data.size());
  ser_msg.buffer_length = response.serv_data.size();
  const auto res_msg = deserializeMsg<std_srvs::srv::SetBool::Response>(&ser_msg);

  EXPECT_EQ(res_msg->message, "hello");
  EXPECT_EQ(res_msg->success, request_msg.data);
}

TEST(SmokeTest, receiveMessagesOfMultipleTransientLocalPublishers) {
  const std::string topic_name = "/latched";
  auto node = rclcpp::Node::make_shared("node");
  rclcpp::QoS qos = rclcpp::QoS(rclcpp::KeepLast(1));
  qos.transient_local();
  qos.reliable();

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  auto spinnerThread = std::thread(
    [&executor]() {
      executor.spin();
    });

  constexpr size_t nPubs = 15;
  std::vector<rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> pubs;
  for (size_t i = 0; i < nPubs; ++i) {
    auto pub = pubs.emplace_back(node->create_publisher<std_msgs::msg::String>(topic_name, qos));
    std_msgs::msg::String msg;
    msg.data = "Hello";
    pub->publish(msg);
  }

  // Set up a client and subscribe to the channel.
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  auto channel_future = cobridge_base::wait_for_channel(client, topic_name);
  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));
  client->login("test user", "test-user-id-0000");
  ASSERT_EQ(std::future_status::ready, channel_future.wait_for(THREE_SECOND));
  const cobridge_base::Channel channel = channel_future.get();
  const cobridge_base::SubscriptionId subscription_id = 1;

  // Set up binary message handler to resolve the promise when all nPub msg have been received
  std::promise<void> promise;
  std::atomic<size_t> n_received_messages = 0;
  client->set_binary_message_handler(
    [&promise, &n_received_messages](const uint8_t *, size_t) {
      if (++n_received_messages == nPubs) {
        promise.set_value();
      }
    });

  // Subscribe to the channel and confirm that the promise resolves
  client->subscribe({{subscription_id, channel.id}});
  EXPECT_EQ(std::future_status::ready, promise.get_future().wait_for(DEFAULT_TIMEOUT));
  EXPECT_EQ(n_received_messages, nPubs);
  client->unsubscribe({subscription_id});

  pubs.clear();
  executor.remove_node(node);
  executor.cancel();
  spinnerThread.join();
}

TEST(FetchAssetTest, fetchExistingAsset) {
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  EXPECT_EQ(std::future_status::ready, client->connect(URI).wait_for(DEFAULT_TIMEOUT));

  client->login("test user", "test-user-id-0000");

  const auto millis_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());
  const auto tmp_file_path =
    std::filesystem::temp_directory_path() / std::to_string(millis_since_epoch.count());
  constexpr char content[] = "Hello, world";
  FILE * tmp_asset_file = std::fopen(tmp_file_path.c_str(), "w");
  std::fputs(content, tmp_asset_file);
  std::fclose(tmp_asset_file);

  const std::string uri = std::string("file://") + tmp_file_path.string();
  const uint32_t request_id = 123;

  auto future = cobridge_base::wait_for_fetch_asset_response(client);
  client->fetch_asset(uri, request_id);
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  const cobridge_base::FetchAssetResponse response = future.get();

  EXPECT_EQ(response.request_id, request_id);
  EXPECT_EQ(response.status, cobridge_base::FetchAssetStatus::Success);
  // +1 since NULL terminator is not written to file.
  ASSERT_EQ(response.data.size() + 1ul, sizeof(content));
  EXPECT_EQ(0, std::memcmp(content, response.data.data(), response.data.size()));
  std::remove(tmp_file_path.c_str());
}

TEST(FetchAssetTest, fetchNonExistingAsset) {
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  EXPECT_EQ(std::future_status::ready, client->connect(URI).wait_for(DEFAULT_TIMEOUT));

  client->login("test user", "test-user-id-0000");

  const std::string asset_id = "file:///foo/bar";
  const uint32_t request_id = 456;

  auto future = cobridge_base::wait_for_fetch_asset_response(client);
  client->fetch_asset(asset_id, request_id);
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  const cobridge_base::FetchAssetResponse response = future.get();

  EXPECT_EQ(response.request_id, request_id);
  EXPECT_EQ(response.status, cobridge_base::FetchAssetStatus::Error);
  EXPECT_FALSE(response.error_message.empty());
}

// Run all the tests that were declared with TEST()
int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  RCLCPP_INFO(rclcpp::get_logger("smoke-test"), "RCLCPP init function was called");

  const size_t numThreads = 2;
  auto executor =
    rclcpp::executors::MultiThreadedExecutor::make_shared(rclcpp::ExecutorOptions{}, numThreads);

  rclcpp_components::ComponentManager componentManager(executor, "cobridge_component_manager");
  const auto componentResources = componentManager.get_component_resources("cobridge");

  if (componentResources.empty()) {
    RCLCPP_INFO(componentManager.get_logger(), "No loadable resources found");
    return EXIT_FAILURE;
  }

  auto componentFactory = componentManager.create_component_factory(componentResources.front());
  rclcpp::NodeOptions nodeOptions;
  // Explicitly allow file:// asset URIs for testing purposes.
  nodeOptions.append_parameter_override(
    "asset_uri_allowlist",
    std::vector<std::string>({"file://.*"}));
  auto node = componentFactory->create_node_instance(nodeOptions);
  executor->add_node(node.get_node_base_interface());

  std::thread spinnerThread([&executor]() {
      executor->spin();
    });

  const auto testResult = RUN_ALL_TESTS();
  executor->cancel();
  spinnerThread.join();

  RCLCPP_INFO(rclcpp::get_logger("smoke-test"), "RCLCPP shutdown function was called");
  rclcpp::shutdown();

  return testResult;
}
