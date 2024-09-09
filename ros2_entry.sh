#!/bin/bash
set -e

# setup workspace environment
echo "source env: $ROS_WS/install/setup.bash"
source "$ROS_WS/install/setup.bash" --
exec "$@"