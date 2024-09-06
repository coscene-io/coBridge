#!/bin/bash
set -e

# setup ros2 environment
source "../../devel/setup.bash" --
exec "$@"