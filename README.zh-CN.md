# cobridge

cobridge 会以 ros node 的方式运行在机器人端，并通过 websocket 方式与云端进行交互。cobridge 与云端建立链接后，根据云端指令可以实现订阅 ros topic，调用 ros service，实现实时监控机器人状态、远程下发指令等功能。

## 已适配版本

| ROS 版本 | 发型版本 | Ubuntu 版本  | 状态      |
| -------- | -------- | ------------ | --------- |
| ROS 1    | melodic  | 18.04 Bionic | ✅ 已支持 |
| ROS 1    | noetic   | 20.04 Focal  | ✅ 已支持 |
| ROS 2    | foxy     | 20.04 Focal  | ✅ 已支持 |
| ROS 2    | humble   | 22.04 Jammy  | ✅ 已支持 |
| ROS 2    | jazzy    | 24.04 Noble  | ✅ 已支持 |

## 从 APT 安装

- 导入公钥

  ```bash
  curl -fsSL https://apt.coscene.cn/coscene.gpg | sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/coscene.gpg
  ```

- 添加源

  ```bash
  echo "deb [signed-by=/etc/apt/trusted.gpg.d/coscene.gpg] https://apt.coscene.cn $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/coscene.list
  ```

- 更新 apt 并安装

  ```bash
  sudo apt update
  # 注意: 如果 ROS_DISTRO 没有在你的环境变量里面，${ROS_DISTRO} 需要被 'noetic', 'foxy', 'humble' or 'jazzy'替换
  sudo apt install ros-${ROS_DISTRO}-cobridge -y
  ```

- 运行 coBridge

  ```bash
  source /opt/ros/${ROS_DISTRO}/setup.bash

  # for ros 1 distribution
  roslaunch cobridge cobridge.launch

  # for ros 2 distribution
  ros2 launch cobridge cobridge_launch.xml
  ```

## 自编译 (推荐)

- 安装依赖库

  ```bash
  # for ROS 1 distribution
  sudo apt install -y \
    libasio-dev \
    ros-${ROS_DISTRO}-resource-retriever \
    ros-${ROS_DISTRO}-ros-babel-fish

  # for ROS 2 distribution
  sudo apt install -y \
      libasio-dev \
      ros-${ROS_DISTRO}-resource-retriever
  ```

- ROS1

  ```bash
  # 将工程复制到 {your_ros_ws}/src/ 文件夹内
  cp -r {this_repo} {your_ros_ws}/src/

  source /opt/ros/{ros_distro}/setup.bash

  cd {your_ros2_ws}

  ./patch_apply.sh

  catkin_make install
  ```

- ROS2

  ```bash
   # 将工程复制到 {your_ros2_ws}/src/ 文件夹内
   cp -r {this_repo} {your_ros_ws}/src/

   source /opt/ros/{ros_distro}/setup.bash

   cd {your_ros2_ws}

  ./patch_apply.sh

   colcon build --packages-select cobridge
  ```

## 运行

```bash
# ros 1
roslaunch cobridge cobridge.launch

# ros 2
ros2 launch cobridge cobridge_launch.xml
```

## 延迟和性能报告

### 测试环境

| 项目     | 配置                                                 |
|--------|----------------------------------------------------|
| ROS 版本 | ROS2 Humble                                        |
| 操作系统   | Ubuntu 22.04                                       |
| CPU    | 11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz * 8 |
| 内存     | 16 GB                                              |

### 延迟测试

#### 端到端延迟（Topic 订阅）

| 消息类型                    | 消息大小                           | 发布频率 (Hz) | 平均延迟 - colink (ms) | 平均延迟 - 局域网 (ms) |
|-------------------------|--------------------------------|-----------|--------------------|-----------------|
| sensor_msgs/Image       | 640*480, 16UC1, 约 600 KB/frame | 10        | 100                | 20              |
| sensor_msgs/PointCloud2 | 约 500 KB/frame                 | 10        | 100                | 15              |
| nav_msgs/Odometry       | 713 Bytes/frame                | 100       | 20                 | 20              |

#### Service 调用延迟

| Service 类型     | 请求大小     | 响应大小     | 平均延迟 (ms) |
|----------------|----------|----------|-----------|
| std_srvs/Empty | 0        | 0        | 20        |
| 自定义消息(定义如下)    | 56 Bytes | 73 Bytes | 55        |
```text
string data
---
bool success
string data
```

### 性能测试

#### 资源使用
* 场景 01:
  ```text
  1路 sensor_msg/Image, 1280 * 720, 8UC3, 30 fps
  1路 sensor_msg/CompressedImage, 1280 * 720, JPEG, 30 fps
  1路 /tf, 200Hz
  ```
  
  | 场景     | CPU 使用率 (%) | 内存使用 (MB) | 网络带宽 (Mbps) | 消息丢失率 (%) |
  |--------|-------------|-----------|-------------|-----------|
  | colink | 5%          | 120+      | 80+         | 90        | 
  | 局域网    | 5%          | 120+      | 240+        | 65        |

* 场景 02:
  ```text
  8路 foxglove_msg/CompressedVideo, 2000000 bitrate
  1路 /tf
  1路 sensor_msg/PointCloud2, 900*96, 24 point step
  ```
  | 场景     | CPU 使用率 (%) | 内存使用 (MB) | 网络带宽 (Mbps) | 消息丢失率 (%) |
  |--------|-------------|-----------|-------------|-----------|
  | colink | 5%          | 120+      | 80+         | 50        | 
  | 局域网    | 5%          | 120+      | 160+        | 0         |


## 云端可视化

云端可视化需配合刻行 `coLink` 组件，通过网页端实时可视化机器人端状态。

## 荣誉

最初来自 foxglove，感谢他们的出色工作。
