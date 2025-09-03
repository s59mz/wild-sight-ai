#
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
# Eagle Eye C++ ROS2 Package
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#


cmake_minimum_required(VERSION 3.8)
project(wild_sight_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(wild_sight_interfaces REQUIRED)

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

include_directories(include ${GST_INCLUDE_DIRS})

add_executable(gstreamer_pipeline src/gstreamer_pipeline.cpp)
ament_target_dependencies(gstreamer_pipeline wild_sight_interfaces rclcpp)

# Link GStreamer libraries
target_link_libraries(gstreamer_pipeline ${GST_LIBRARIES})

install(TARGETS
        gstreamer_pipeline
        DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include/
)

ament_package()
