FROM ros:noetic

ENV ROS_DISTRO=noetic
RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 4B63CF8FDE49746E98FA01DDAD19BAB3CBF125EA
# Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends nlohmann-json3-dev  \
    libasio-dev ros-${ROS_DISTRO}-cv-bridge zip \
    libwebsocketpp-dev ros-${ROS_DISTRO}-resource-retriever ros-${ROS_DISTRO}-ros-babel-fish

RUN rm -rf /var/lib/apt/lists/*