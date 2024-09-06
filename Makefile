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

test:
ifeq ($(ROS_DISTRO),ros1)
	$(MAKE) ros1-test
else ifeq ($(TEST_TYPE),ros2)
	$(MAKE) ros2-test
else
	$(error Please specify TEST_TYPE as ros1 or ros2)
endif