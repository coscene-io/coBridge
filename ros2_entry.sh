#!/bin/bash
set -e

# setup workspace environment
source "../install/setup.bash" --
exec "$@"