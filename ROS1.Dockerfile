ARG ROS_DISTRIBUTION=noetic
FROM ros:$ROS_DISTRIBUTION-ros-base

# Install clang and set as default compiler.
RUN apt-get update && apt-get install -y --no-install-recommends \
  clang \
  && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++

# Set environment and working directory
ENV ROS_WS=/ros1_ws
WORKDIR $ROS_WS

# Add package.xml so we can install package dependencies.
COPY package.xml src/cobridge/

# Install rosdep dependencies
RUN . /opt/ros/$ROS_DISTRO/setup.sh && \
    apt-get update && rosdep update --include-eol-distros && rosdep install -y \
      --from-paths \
        src \
      --ignore-src \
    && rm -rf /var/lib/apt/lists/*

# Add common files and ROS 1 source code
COPY CMakeLists.txt src/cobridge/CMakeLists.txt
COPY cobridge_base src/cobridge/cobridge_base
COPY nodelets.xml src/cobridge/nodelets.xml
COPY ros1_bridge src/cobridge/ros1_bridge

ARG USE_ASIO_STANDALONE=ON

## Build the Catkin workspace
RUN . /opt/ros/$ROS_DISTRO/setup.sh \
  && catkin_make -DUSE_ASIO_STANDALONE=$USE_ASIO_STANDALONE

## source workspace from entrypoint
RUN sed --in-place \
      's|^source .*|source "$ROS_WS/devel/setup.bash"|' \
      /ros_entrypoint.sh

# Run cobridge
CMD ["rosrun", "cobridge", "cobridge"]
