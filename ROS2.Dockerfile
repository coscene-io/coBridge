ARG ROS_DISTRIBUTION=humble
FROM ros:$ROS_DISTRIBUTION-ros-base

# Install clang and set as default compiler.
RUN apt-get update && apt-get install -y --no-install-recommends \
  clang \
  && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++

# Set environment and working directory
ENV ROS_WS=/ros2_ws
WORKDIR $ROS_WS

# Install system dependencies
RUN apt-get clean && apt-get update && apt-get install -y --no-install-recommends \
  nlohmann-json3-dev \
  libasio-dev \
  libboost-all-dev \
  libssl-dev \
  libwebsocketpp-dev \
  ros-${ROS_DISTRO}-cv-bridge \
  ros-${ROS_DISTRO}-resource-retriever \
  && rm -rf /var/lib/apt/lists/*

# Add package.xml so we can install package dependencies.
COPY package.xml src/cobridge/

# Install rosdep dependencies
RUN . /opt/ros/$ROS_DISTRO/setup.sh && \
    apt-get clean && apt-get update && rosdep update --include-eol-distros && rosdep install -y \
      --from-paths \
        src \
      --ignore-src \
    && rm -rf /var/lib/apt/lists/*

# Add common files and ROS 2 source code
COPY CMakeLists.txt src/cobridge/CMakeLists.txt
COPY cobridge_base src/cobridge/cobridge_base
COPY ros2_bridge src/cobridge/ros2_bridge

ARG USE_ASIO_STANDALONE=ON

# Build the ROS 2 workspace
RUN . /opt/ros/$ROS_DISTRO/setup.sh \
  && colcon build --event-handlers console_direct+ --cmake-args -DUSE_ASIO_STANDALONE=$USE_ASIO_STANDALONE

# source workspace from entrypoint
RUN sed --in-place \
      's|^source .*|source "$ROS_WS/install/setup.bash"|' \
      /ros_entrypoint.sh

# Run foxglove_bridge
CMD ["ros2", "run", "cobridge", "cobridge"]