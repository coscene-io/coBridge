#!/bin/bash
set -e

# setup ros2 environment
echo "source env: $ROS_WS/devel/setup.bash"
source "$ROS_WS/devel/setup.bash" --
exec "$@"