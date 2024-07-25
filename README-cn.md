# cos-bridge

cos-bridge 会以 ros node 的方式运行在机器人端，并通过websocket方式与云端进行交互。cos-bridge与云端建立链接后，根据云端指令可以实现订阅ros topic，调用ros service，实现实时监控机器人状态、远程下发指令等功能。


## 编译

* 安装依赖库
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
    *  将工程复制到 {your_ros2_ws}/src/ 文件夹内
    *  ` source /opt/ros/{ros_distro}/setup.bash `
    *  ``` bash 
       cd {your_ros2_ws} 
       catkin_make
       ```


* ROS2
  *  将工程复制到 {your_ros2_ws}/src/ 文件夹内
  *  根据ROS2 版本，修改CMakeLists.txt文件，line 19 - 20，依据ROS2 distro选择 add_compile_definitions 参数
  *  ` source /opt/ros/{ros_distro}/install/setup.bash `
  *  ``` bash 
       cd {your_ros2_ws} 
       colcon build --packages-select cos-bridge
     ```

## 运行
  ``` bash
  # ros 1
  roslaunch cos-bridge cos_bridge.launch
  
  # ros 2
  ros2 launch cos-bridge cos-bridge_launch.xml 
  ```

## 云端可视化
云端可视化需配合刻行 `virmesh` 组件，通过网页端实时可视化机器人端状态。

## 荣誉
最初来自 foxglove，感谢他们的出色工作。