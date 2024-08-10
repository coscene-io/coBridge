ROS1_DISTRIBUTIONS := melodic noetic
ROS2_DISTRIBUTIONS := foxy humble iron rolling

define generate_ros1_targets
.PHONY: $(1)
$(1):
	docker build -t $(1)_bridge --pull -f ROS1.Dockerfile --build-arg ROS_DISTRIBUTION=$(1) .

.PHONY: $(1)-test
$(1)-test: $(1)
	docker run -t --rm $(1)_bridge bash -c "catkin_make run_tests && catkin_test_results"

.PHONY: $(1)-boost-asio
$(1)-boost-asio:
	docker build -t $(1)_bridge_boost_asio --pull -f ROS1.Dockerfile --build-arg ROS_DISTRIBUTION=$(1) --build-arg USE_ASIO_STANDALONE=OFF .

.PHONY: $(1)-test-boost-asio
$(1)-test-boost-asio: $(1)-boost-asio
	docker run -t --rm $(1)_bridge_boost_asio bash -c "catkin_make run_tests && catkin_test_results"
endef

define generate_ros2_targets
.PHONY: $(1)
$(1):
	docker build -t $(1)_bridge --pull -f ROS2.Dockerfile --build-arg ROS_DISTRIBUTION=$(1) .

.PHONY: $(1)-test
$(1)-test: $(1)
	docker run -t --rm $(1)_bridge colcon test --event-handlers console_cohesion+ --return-code-on-test-failure

.PHONY: $(1)-boost-asio
$(1)-boost-asio:
	docker build -t $(1)_bridge-boost-asio --pull -f ROS2.Dockerfile --build-arg ROS_DISTRIBUTION=$(1) --build-arg USE_ASIO_STANDALONE=OFF .

.PHONY: $(1)-test-boost-asio
$(1)-test-boost-asio: $(1)-boost-asio
	docker run -t --rm $(1)_bridge-boost-asio colcon test --event-handlers console_cohesion+ --return-code-on-test-failure
endef

$(foreach distribution,$(ROS1_DISTRIBUTIONS),$(eval $(call generate_ros1_targets,$(strip $(distribution)))))
$(foreach distribution,$(ROS2_DISTRIBUTIONS),$(eval $(call generate_ros2_targets,$(strip $(distribution)))))


default: ros2

.PHONY: ros1
ros1:
	docker build -t ros1_bridge --pull -f ROS1.Dockerfile .

.PHONY: ros2
ros2:
	docker build -t ros2_bridge --pull -f ROS2.Dockerfile .

.PHONY: rosdev
rosdev:
	docker build -t rosdev_bridge --pull -f .devcontainer/Dockerfile .

clean:
	docker rmi $(docker images --filter=reference="*_bridge" -q)

.PHONY: lint
lint: rosdev
	docker run -t --rm -v $(CURDIR):/src rosdev_bridge python3 /src/scripts/format.py /src