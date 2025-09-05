#!/bin/bash
#
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
# Application's Startup Script in a Docker container
#   - Load the app firmware and launch docker container first
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#
#
#   NOTE: This script runs on Kria board in a application's Docker container only
#


# Default parameters
default_camera_url="rtsp://192.168.1.11:554/stream1"
serial_port="/dev/ttyUL0"

# Check if a camera_url parameter is passed
if [ -z "$1" ]; then
    # No parameter passed, use the default camera_url
    camera_url=$default_camera_url
else
    # Parameter passed, use it as camera_url
    camera_url=$1
fi

# Source the ROS2 setup script
source install/setup.bash

# Start the ROS2 launch file with the specified camera_url
ros2 launch wild_sight_bringup follow_cam.launch.xml camera_url:=$camera_url serial_port:=$serial_port

