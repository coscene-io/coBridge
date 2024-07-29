//////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////

#include "generic_subscription.hpp"

#include <memory>
#include <string>
#include <utility>
//#include <sensor_msgs/msg/image.hpp>
//#include <sensor_msgs/msg/camera_info.hpp>
//#include <cv_bridge/cv_bridge.h>
//#include <opencv2/imgproc/imgproc.hpp>

#include "rclcpp/any_subscription_callback.hpp"
#include "rclcpp/subscription.hpp"

//#define FRAME_LIMIT 1
//#define OUTPUT_WIDTH 320
//#define OUTPUT_HEIGHT 240

namespace {
    rcl_subscription_options_t get_subscription_options(const rclcpp::QoS &qos) {
        auto options = rcl_subscription_get_default_options();
        options.qos = qos.get_rmw_qos_profile();
        return options;
    }
}  // unnamed namespace

namespace cobridge {
    GenericSubscription::GenericSubscription(
            rclcpp::node_interfaces::NodeBaseInterface *node_base,
            const rosidl_message_type_support_t &ts,
            const std::string &topic_name,
            const std::string &topic_type,
            const rclcpp::QoS &qos,
            std::function<void(std::shared_ptr<rclcpp::SerializedMessage>, uint64_t timestamp)> callback)
            : SubscriptionBase(node_base, ts, topic_name, get_subscription_options(qos), true),
              _default_allocator(rcutils_get_default_allocator()),
              _callback(std::move(callback)),
              _qos(qos),
              _last_frame_timestamp(0),
              _message_type(topic_type),
              _topic_name(topic_name) {
        //if (topic_type == "sensor_msgs/msg/Image" || topic_type == "sensor_msgs/msg/CameraInfo") {
        //    use_down_sample_ = true;
        //} else {
        //    use_down_sample_ = false;
        //}
    }

    std::shared_ptr<void> GenericSubscription::create_message() {
        return create_serialized_message();
    }

    std::shared_ptr<rclcpp::SerializedMessage> GenericSubscription::create_serialized_message() {
        return borrow_serialized_message(0);
    }

    void GenericSubscription::handle_message(
            std::shared_ptr<void> &message, const rclcpp::MessageInfo &message_info) {
        // TODO: in ZhiYuan data, there is no received timestamp and source timestamp,
        //       so, down sample can not work as expected. code below will be back in future
        //if (use_down_sample_) {
        //    int64_t cur_timestamp = message_info.get_rmw_message_info().received_timestamp;
        //    if (cur_timestamp - last_frame_timestamp_ < 1000000000 / FRAME_LIMIT) {
        //        return;
        //    }
        //    last_frame_timestamp_ = cur_timestamp;

        //    if (message_type_ == "sensor_msgs/msg/Image") {
        //        sensor_msgs::msg::Image received_image;
        //        auto deserializer = rclcpp::Serialization<sensor_msgs::msg::Image>();
        //        deserializer.deserialize_message(
        //                std::static_pointer_cast<rclcpp::SerializedMessage>(message).get(), &received_image);
        //        //received_image.header.stamp. - last_frame_timestamp_
        //        if (received_image.width * received_image.height < OUTPUT_HEIGHT * OUTPUT_HEIGHT) {
        //            auto typed_message = std::static_pointer_cast<rclcpp::SerializedMessage>(message);
        //            callback_(typed_message, static_cast<uint64_t>(message_info.get_rmw_message_info().source_timestamp));
        //        }

        //        cv_bridge::CvImagePtr srcImage = cv_bridge::toCvCopy(received_image, received_image.encoding);
        //        cv_bridge::CvImagePtr dstImage = std::make_shared<cv_bridge::CvImage>();

        //        cv::resize(srcImage->image, dstImage->image, cv::Size(OUTPUT_WIDTH, OUTPUT_HEIGHT),
        //                   0, 0, cv::INTER_LINEAR);

        //        auto outputImg = dstImage->toImageMsg().get();
        //        outputImg->encoding = received_image.encoding;
        //        outputImg->header = received_image.header;

        //        rclcpp::SerializedMessage outputMsg;
        //        deserializer.serialize_message(outputImg, &outputMsg);
        //        callback_(std::make_shared<rclcpp::SerializedMessage>(outputMsg), static_cast<uint64_t>(cur_timestamp));
        //    } else {
        //        sensor_msgs::msg::CameraInfo cameraInfo;
        //        auto deserializer = rclcpp::Serialization<sensor_msgs::msg::CameraInfo>();
        //        deserializer.deserialize_message(
        //                std::static_pointer_cast<rclcpp::SerializedMessage>(message).get(), &cameraInfo);

        //        float scale_x = static_cast<float>(OUTPUT_WIDTH) / static_cast<float>(cameraInfo.width);
        //        float scale_y = static_cast<float>(OUTPUT_HEIGHT) / static_cast<float>(cameraInfo.height);

        //        cameraInfo.k[0] *= scale_x;
        //        cameraInfo.k[2] *= scale_x;

        //        cameraInfo.k[4] *= scale_y;
        //        cameraInfo.k[5] *= scale_y;

        //        cameraInfo.width = OUTPUT_WIDTH;
        //        cameraInfo.height = OUTPUT_HEIGHT;

        //        rclcpp::SerializedMessage outputMsg;
        //        deserializer.serialize_message(&cameraInfo, &outputMsg);
        //        callback_(std::make_shared<rclcpp::SerializedMessage>(outputMsg), static_cast<uint64_t>(cur_timestamp));
        //    }
        //} else {
            auto typed_message = std::static_pointer_cast<rclcpp::SerializedMessage>(message);
            _callback(typed_message, static_cast<uint64_t>(message_info.get_rmw_message_info().source_timestamp));
        //}
    }

    void GenericSubscription::handle_loaned_message(
            void *message, const rclcpp::MessageInfo &message_info) {
        (void) message;
        (void) message_info;
    }

#ifdef ROS2_VERSION_HUMBLE
    void GenericSubscription::handle_serialized_message(
            const std::shared_ptr<rclcpp::SerializedMessage> & serialized_message,
            const rclcpp::MessageInfo & message_info) {
        callback_(serialized_message, static_cast<uint64_t>(message_info.get_rmw_message_info().source_timestamp));
    }
#endif

    void GenericSubscription::return_message(std::shared_ptr<void> &message) {
        auto typed_message = std::static_pointer_cast<rclcpp::SerializedMessage>(message);
        return_serialized_message(typed_message);
    }

    void GenericSubscription::return_serialized_message(
            std::shared_ptr<rclcpp::SerializedMessage> &message) {
        message.reset();
    }

    const rclcpp::QoS &GenericSubscription::qos_profile() const {
        return _qos;
    }

    std::shared_ptr<rclcpp::SerializedMessage>
    GenericSubscription::borrow_serialized_message(size_t capacity) {
        return std::make_shared<rclcpp::SerializedMessage>(capacity);
    }

}
