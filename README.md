# cobridge

cobridge runs as a ros node on the robot side, and interacts with the cloud via websocket. cos-bridge establishes a link with the cloud to subscribe to a ros topic and invoke a ros service according to cloud instructions.
After cobridge establishes a link with the cloud, it can subscribe to ros topic and call ros service according to the instructions from the cloud, so as to real-time monitor the status of the robot and remotely issue commands.

## Compile

* Install deps 
    ``` bash
  apt install -y nlohmann-json3-dev
  apt install -y libasio-dev 
  apt install -y libboost-all-dev
  apt install -y libssl-dev 
  apt install -y libwebsocketpp-dev
  apt install -y ros-${ROS_DISTRO}-cv-bridge
  apt install -y ros-${ROS_DISTRO}-resource-retriever
    ```

* ROS1
    *  copy this project into {your_ros_ws}/src/
    *  ` source /opt/ros/{ros_distro}/setup.bash `
    *  ``` bash 
       cd {your_ros_ws} 
       catkin_make
       ```


* ROS2
  *  copy this project into {your_ros2_ws}/src/
  *  Modify the CMakeLists.txt file, line 19 - 20, according to the ROS2 version, and select the add_compile_definitions parameter according to the ROS2 distro.
  *  ` source /opt/ros/{ros_distro}/install/setup.bash `
  *  ``` bash 
       cd {your_ros2_ws} 
       colcon build --packages-select cos-bridge
     ```

## Run
  ``` bash
  # ros 1
  roslaunch cos-bridge cos_bridge.launch
  
  # ros 2
  ros2 launch cos-bridge cos-bridge_launch.xml 
  ```

## Cloud Visualization 
The cloud visualisation needs to be coupled with the carve line `virmesh` component to visualise the state of the robot side in real time via the web side.

## Credits
originally from foxglove, thanks for their wonderful work. 
