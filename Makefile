SHELL := /bin/bash

ROS_BIN_PATH := /opt/ros/$(ROS_DISTRO)/bin

lint:
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cpplint --filter=-build/include_order .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_uncrustify .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_copyright .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_cppcheck .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_xmllint .
	/ros_entrypoint.sh $(ROS_BIN_PATH)/ament_lint_cmake .

ros1-test:
	./ros1_entry.sh ../../devel/cobridge/version_test
	./ros1_entry.sh ../../devel/cobridge/smoke_test

ros2-test:
	./ros2_entry.sh ../../build/cobridge/version_test
	./ros2_entry.sh ../../build/cobridge/smoke_test

ros1-build:
	/ros_entrypoint.sh catkin_make

ros2-build:
	/ros_entrypoint.sh colcon build --event-handlers console_direct+



noetic-test:
	ros1-test

foxy-test:
	ros2-test

noetic-build:
	ros1-build

foxy-build:
	ros2-build

test:
	$(ROS_DISTRO)-test

build:
	$(ROS_DISTRO)-build