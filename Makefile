SHELL := /bin/bash

ROS_BIN_PATH := /opt/ros/$(ROS_DISTRO)/bin

lint:
	$(ROS_BIN_PATH)/ament_cpplint --filter=-build/include_order .
	$(ROS_BIN_PATH)/ament_uncrustify .
	$(ROS_BIN_PATH)/ament_copyright .
	$(ROS_BIN_PATH)/ament_cppcheck .
	$(ROS_BIN_PATH)/ament_xmllint .
	$(ROS_BIN_PATH)/ament_lint_cmake .

test:
	../../build/cobridge/version_test
	../../build/cobridge/smoke_test
