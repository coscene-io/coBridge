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
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>
#include <ros/ros.h>
#include <std_msgs/builtin_string.h>
#include <std_srvs/SetBool.h>
#include <websocketpp/config/asio_client.hpp>

#include <json.hpp>
#include <test/test_client.hpp>
#include <websocket_client.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#define URI "ws://localhost:9876"

constexpr uint8_t HELLO_WORLD_BINARY[] = {11, 0, 0, 0, 104, 101, 108, 108,
  111, 32, 119, 111, 114, 108, 100};

constexpr auto THREE_SECOND = std::chrono::seconds(3);
constexpr auto DEFAULT_TIMEOUT = std::chrono::seconds(8);

#define PARAM_1_NAME "/node_1/string_param"
#define PARAM_1_DEFAULT_VALUE "hello"
#define PARAM_2_NAME "/node_2/int_array_param"
#define SERVICE_NAME "/foo_service"

#define PARAM_2_DEFAULT_VALUE_INIT {1.2, 2.1, 3.3}

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
  using PARAM_2_TYPE = std::vector<double>;

protected:
  void SetUp() override
  {
    nh_ = ros::NodeHandle();
    nh_.setParam(PARAM_1_NAME, std::string(PARAM_1_DEFAULT_VALUE));

    const std::vector<double> param2_default = PARAM_2_DEFAULT_VALUE_INIT;
    nh_.setParam(PARAM_2_NAME, param2_default);

    client_ = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
    ASSERT_EQ(std::future_status::ready, client_->connect(URI).wait_for(DEFAULT_TIMEOUT));

    client_->login("test user", "test-user-id-0000");
  }

  ros::NodeHandle nh_;
  std::shared_ptr<cobridge_base::Client<websocketpp::config::asio_client>> client_;
};


class ServiceTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    _nh = ros::NodeHandle();
    _service = _nh.advertiseService<std_srvs::SetBool::Request, std_srvs::SetBool::Response>(
      SERVICE_NAME, &ServiceTest::serviceCallback);
  }

private:
  ros::NodeHandle _nh;
  ros::ServiceServer _service;

  static bool serviceCallback(std_srvs::SetBool::Request & req, std_srvs::SetBool::Response & res)
  {
    res.message = "hello";
    res.success = req.data;
    return true;
  }
};

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
  EXPECT_EQ(std::future_status::ready, client0_kicked_future.wait_for(THREE_SECOND));
  EXPECT_EQ(std::future_status::ready, server_info_future.wait_for(THREE_SECOND));
  EXPECT_EQ(
    "{\"message\":\"The client was forcibly disconnected by the server.\",\"op\":\"kicked\","
    "\"userId\":\"test-user-id-0001\",\"username\":\"user_1\"}", client0_kicked_future.get());
  CompareJsonWithoutFields(
    "{\"capabilities\":[\"clientPublish\",\"connectionGraph\","
    "\"parametersSubscribe\",\"parameters\",\"services\",\"assets\",\"messageTime\"],"
    "\"metadata\":{\"ROS_DISTRO\":\"foxy\"},\"name\":\"cobridge\","
    "\"op\":\"serverInfo\",\"sessionId\":\"1727148359\","
    "\"supportedEncodings\":[\"ros1\"]}", server_info_future.get());
}

TEST(SmokeTest, testSubscription) {
  // Publish a string message on a latched ros topic
  const std::string topic_name = "/pub_topic";
  ros::NodeHandle nh;
  auto pub = nh.advertise<std_msgs::String>(topic_name, 10, true);
  pub.publish(std::string("hello world"));

  // Set up a client and subscribe to the channel.
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  auto channel_future = cobridge_base::wait_for_channel(client, topic_name);
  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));
  client->login("test user", "test-user-id-0000");
  ASSERT_EQ(std::future_status::ready, channel_future.wait_for(DEFAULT_TIMEOUT));
  const cobridge_base::Channel channel = channel_future.get();
  const cobridge_base::SubscriptionId subscription_id = 1;

  // Subscribe to the channel and confirm that the promise resolves
  auto msg_future = cobridge_base::wait_for_channel_msg(client.get(), subscription_id);
  client->subscribe({{subscription_id, channel.id}});
  ASSERT_EQ(std::future_status::ready, msg_future.wait_for(THREE_SECOND));
  const auto msg_data = msg_future.get();
  ASSERT_EQ(sizeof(HELLO_WORLD_BINARY), msg_data.size());
  EXPECT_EQ(0, std::memcmp(HELLO_WORLD_BINARY, msg_data.data(), msg_data.size()));

  // Unsubscribe from the channel again.
  client->unsubscribe({subscription_id});
}

TEST(SmokeTest, testPublishing) {
  // cobridge_base::Client<websocketpp::config::asio_client> client;
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();

  cobridge_base::ClientAdvertisement advertisement;
  advertisement.channel_id = 1;
  advertisement.topic = "/foo";
  advertisement.encoding = "ros1";
  advertisement.schema_name = "std_msgs/String";

  // Set up a ROS node with a subscriber
  ros::NodeHandle nh;
  std::promise<std::string> msg_promise;
  auto msg_future = msg_promise.get_future();
  auto subscriber = nh.subscribe<std_msgs::String>(
    advertisement.topic, 10, [&msg_promise](const std_msgs::String::ConstPtr & msg) {
      msg_promise.set_value(msg->data);
    });

  // Set up the client, advertise and publish the binary message
  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(DEFAULT_TIMEOUT));
  client->login("test user", "test-user-id-0000");
  client->advertise({advertisement});

  auto channelFuture = cobridge_base::wait_for_channel(client, advertisement.topic);
  ASSERT_EQ(std::future_status::ready, channelFuture.wait_for(THREE_SECOND));

  client->publish(advertisement.channel_id, HELLO_WORLD_BINARY, sizeof(HELLO_WORLD_BINARY));

  // Ensure that we have received the correct message via our ROS subscriber
  const auto msg_result = msg_future.wait_for(THREE_SECOND);
  ASSERT_EQ(std::future_status::ready, msg_result);
  EXPECT_EQ("hello world", msg_future.get());
  client->unadvertise({advertisement.channel_id});
}

TEST_F(ParameterTest, testGetAllParams) {
  const std::string requestId = "req-testGetAllParams";
  auto future = cobridge_base::wait_for_parameters(client_, requestId);
  client_->get_parameters({}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_GE(params.size(), 2UL);
}

TEST_F(ParameterTest, testGetNonExistingParameters) {
  const std::string request_id = "req-testGetNonExistingParameters";
  auto future = cobridge_base::wait_for_parameters(client_, request_id);
  client_->get_parameters(
    {"/foo_1/non_existing_parameter", "/foo_2/non_existing/nested_parameter"},
    optional<std::string>(request_id));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_TRUE(params.empty());
}

TEST_F(ParameterTest, testGetParameters) {
  const std::string requestId = "req-testGetParameters";
  auto future = cobridge_base::wait_for_parameters(client_, requestId);
  client_->get_parameters({PARAM_1_NAME, PARAM_2_NAME}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(2UL, params.size());
  auto p1Iter = std::find_if(
    params.begin(), params.end(), [](const cobridge_base::Parameter & param) {
      return param.get_name() == PARAM_1_NAME;
    });
  auto p2Iter = std::find_if(
    params.begin(), params.end(), [](const cobridge_base::Parameter & param) {
      return param.get_name() == PARAM_2_NAME;
    });
  ASSERT_NE(p1Iter, params.end());
  EXPECT_EQ(PARAM_1_DEFAULT_VALUE, p1Iter->get_value().getValue<PARAM_1_TYPE>());
  ASSERT_NE(p2Iter, params.end());

  std::vector<double> double_array_val;
  const auto array_params =
    p2Iter->get_value().getValue<std::vector<cobridge_base::ParameterValue>>();
  for (const auto & paramValue : array_params) {
    double_array_val.push_back(paramValue.getValue<double>());
  }
  const std::vector<double> expected_value = PARAM_2_DEFAULT_VALUE_INIT;
  EXPECT_EQ(double_array_val, expected_value);
}

TEST_F(ParameterTest, testSetParameters) {
  const PARAM_1_TYPE newP1value = "world";
  const std::vector<cobridge_base::ParameterValue> newP2value = {
    cobridge_base::ParameterValue(4.1), cobridge_base::ParameterValue(5.5),
    cobridge_base::ParameterValue(6.6)};

  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(PARAM_1_NAME, cobridge_base::ParameterValue(newP1value)),
    cobridge_base::Parameter(PARAM_2_NAME, cobridge_base::ParameterValue(newP2value)),
  };

  client_->set_parameters(parameters);
  const std::string requestId = "req-testSetParameters";
  auto future = cobridge_base::wait_for_parameters(client_, requestId);
  client_->get_parameters({PARAM_1_NAME, PARAM_2_NAME}, optional<std::string>(requestId));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(2UL, params.size());
  auto p1Iter = std::find_if(
    params.begin(), params.end(), [](const cobridge_base::Parameter & param) {
      return param.get_name() == PARAM_1_NAME;
    });
  auto p2Iter = std::find_if(
    params.begin(), params.end(), [](const cobridge_base::Parameter & param) {
      return param.get_name() == PARAM_2_NAME;
    });
  ASSERT_NE(p1Iter, params.end());
  EXPECT_EQ(newP1value, p1Iter->get_value().getValue<PARAM_1_TYPE>());
  ASSERT_NE(p2Iter, params.end());

  std::vector<double> double_array_val;
  const auto array_params =
    p2Iter->get_value().getValue<std::vector<cobridge_base::ParameterValue>>();
  for (const auto & paramValue : array_params) {
    double_array_val.push_back(paramValue.getValue<double>());
  }
  const std::vector<double> expected_value = {4.1, 5.5, 6.6};
  EXPECT_EQ(double_array_val, expected_value);
}

TEST_F(ParameterTest, testSetParametersWithReqId) {
  const PARAM_1_TYPE newP1value = "world";
  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(PARAM_1_NAME, cobridge_base::ParameterValue(newP1value)),
  };

  const std::string request_id = "req-testSetParameters";
  auto future = cobridge_base::wait_for_parameters(client_, request_id);
  client_->set_parameters(parameters, optional<std::string>(request_id));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(1UL, params.size());
}

TEST_F(ParameterTest, testUnsetParameter) {
  const std::vector<cobridge_base::Parameter> parameters = {
    cobridge_base::Parameter(PARAM_1_NAME),
  };

  const std::string request_id = "req-testUnsetParameter";
  auto future = cobridge_base::wait_for_parameters(client_, request_id);
  client_->set_parameters(parameters, optional<std::string>(request_id));
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  EXPECT_EQ(0UL, params.size());
}

TEST_F(ParameterTest, testParameterSubscription) {
  auto future = cobridge_base::wait_for_parameters(client_);

  client_->subscribe_parameter_updates({PARAM_1_NAME});
  client_->set_parameters(
    {cobridge_base::Parameter(
        PARAM_1_NAME,
        cobridge_base::ParameterValue("foo"))});
  ASSERT_EQ(std::future_status::ready, future.wait_for(DEFAULT_TIMEOUT));
  std::vector<cobridge_base::Parameter> params = future.get();

  ASSERT_EQ(1UL, params.size());
  EXPECT_EQ(params.front().get_name(), PARAM_1_NAME);

  client_->unsubscribe_parameter_updates({PARAM_1_NAME});
  client_->set_parameters(
    {cobridge_base::Parameter(
        PARAM_1_NAME,
        cobridge_base::ParameterValue("bar"))});

  future = cobridge_base::wait_for_parameters(client_);
  ASSERT_EQ(std::future_status::timeout, future.wait_for(THREE_SECOND));
}

TEST_F(ServiceTest, testCallService) {
  // Connect a few clients (in parallel) and make sure that they can all call the service
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();

  ASSERT_EQ(std::future_status::ready, client->connect(URI).wait_for(THREE_SECOND));

  client->login("test user", "test-user-id-0000");
  auto service_future = cobridge_base::wait_for_service(client, SERVICE_NAME);
  ASSERT_EQ(std::future_status::ready, service_future.wait_for(DEFAULT_TIMEOUT));
  const cobridge_base::Service service = service_future.get();

  cobridge_base::ServiceRequest request;
  request.service_id = service.id;
  request.call_id = 123lu;
  request.encoding = "ros1";
  request.serv_data = {1};  // Serialized boolean "True"

  const std::vector<uint8_t> expectedSerializedResponse = {1, 5, 0, 0, 0, 104, 101, 108, 108, 111};

  std::future<cobridge_base::ServiceResponse> future =
    cobridge_base::wait_for_service_response(client);
  client->send_service_request(request);

  ASSERT_EQ(std::future_status::ready, future.wait_for(THREE_SECOND));
  cobridge_base::ServiceResponse response;
  EXPECT_NO_THROW(response = future.get());
  EXPECT_EQ(response.service_id, request.service_id);
  EXPECT_EQ(response.call_id, request.call_id);
  EXPECT_EQ(response.encoding, request.encoding);
  EXPECT_EQ(response.serv_data, expectedSerializedResponse);
}

TEST(FetchAssetTest, fetchExistingAsset) {
  auto client = std::make_shared<cobridge_base::Client<websocketpp::config::asio_client>>();
  EXPECT_EQ(std::future_status::ready, client->connect(URI).wait_for(DEFAULT_TIMEOUT));

  client->login("test user", "test-user-id-0000");

  const auto millis_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());
  const auto tmp_file_path =
    boost::filesystem::temp_directory_path() / std::to_string(millis_since_epoch.count());
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
  ros::init(argc, argv, "tester");
  ros::NodeHandle nh;

  // Give the server some time to start
  std::this_thread::sleep_for(std::chrono::seconds(2));

  ros::AsyncSpinner spinner(1);
  spinner.start();
  const auto testResult = RUN_ALL_TESTS();
  spinner.stop();

  return testResult;
}
