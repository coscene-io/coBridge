SHELL := /bin/bash

ROS_BIN_PATH := /opt/ros/$(ROS_DISTRO)/bin

lint:
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cpplint --filter=-build/include_order .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_uncrustify .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_copyright .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cppcheck .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_xmllint .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_lint_cmake .

test:
	./ros2_entry.sh ../../build/cobridge/version_test
	./ros2_entry.sh ../../build/cobridge/smoke_test
