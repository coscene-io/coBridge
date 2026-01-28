SHELL := /bin/bash

ROS1_DISTRO := indigo melodic noetic
ROS2_DISTRO := foxy humble jazzy

ROS_WS := $(shell pwd)
export ROS_WS

ROS_BIN_PATH := /opt/ros/$(ROS_DISTRO)/bin

lint:
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cpplint --filter=-build/include_order cobridge_base ros1_bridge ros2_bridge
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_uncrustify cobridge_base ros1_bridge ros2_bridge
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_copyright cobridge_base ros1_bridge ros2_bridge
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cppcheck cobridge_base ros1_bridge ros2_bridge
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_xmllint cobridge_base ros1_bridge ros2_bridge
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_lint_cmake .

test:
ifeq ($(findstring $(ROS_DISTRO), $(ROS1_DISTRO)), $(ROS_DISTRO))
	/ros_entrypoint.sh catkin_make run_tests
else ifeq ($(findstring $(ROS_DISTRO), $(ROS2_DISTRO)), $(ROS_DISTRO))
	src/cobridge/ros2_entry.sh build/cobridge/version_test
	src/cobridge/ros2_entry.sh build/cobridge/smoke_test
else
	$(error Unsupported ROS_DISTRO: $(ROS_DISTRO))
endif

build:
ifeq ($(findstring $(ROS_DISTRO), $(ROS1_DISTRO)), $(ROS_DISTRO))
	/ros_entrypoint.sh catkin_make install
else ifeq ($(findstring $(ROS_DISTRO), $(ROS2_DISTRO)), $(ROS_DISTRO))
	/ros_entrypoint.sh colcon build --event-handlers console_direct+
else
	$(error Unsupported ROS_DISTRO: $(ROS_DISTRO))
endif
