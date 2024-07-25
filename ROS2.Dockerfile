FROM ros:foxy

ENV ROS_DISTRO=foxy
RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 4B63CF8FDE49746E98FA01DDAD19BAB3CBF125EA

RUN apt-get update && apt-get install -y --no-install-recommends nlohmann-json3-dev  \
    libasio-dev libboost-all-dev ros-${ROS_DISTRO}-cv-bridge\
    libssl-dev libwebsocketpp-dev \
    zip unzip ros-${ROS_DISTRO}-resource-retriever
RUN rm -rf /var/lib/apt/lists/*

RUN mkdir -p /ros2_ws/src
COPY ../../cos-bridge /ros2_ws/src