#!/bin/bash
#
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
# Build script for two docker images:
#   - Kria Runtime Image - based on Vitis AI Runtime 3.5 and VVAS 3.0
#   - APP Image - Final application based on the ROS2-Humble and GStreamer
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa
#


# Define the names of the images
KRIA_IMAGE="kria-image:3.5"
APP_IMAGE="wild-sight-ai:1.0"

# Define the names of dockerfiles
KRIA_DOCKERFILE="dockerfiles/kria-image-docker"
APP_DOCKERFILE="dockerfiles/wild-sight-docker"

# Building Application Image
if ! docker image inspect $APP_IMAGE > /dev/null 2>&1; then

    # Building Kria Image 
    if ! docker image inspect $KRIA_IMAGE > /dev/null 2>&1; then
        echo "$KRIA_IMAGE does not exist. Building $KRIA_IMAGE..."
        docker build --network host --build-arg BUILD_DATE="$(date -u +'%Y/%m/%d %H:%M')" -f $KRIA_DOCKERFILE . -t $KRIA_IMAGE
    else
      echo "$KRIA_IMAGE exists already."
    fi

    echo "Building $APP_IMAGE..."
    docker build --network host --build-arg BUILD_DATE="$(date -u +'%Y/%m/%d %H:%M')" -f $APP_DOCKERFILE . -t $APP_IMAGE
else
  echo "$APP_IMAGE exists already."
fi

if  docker image inspect $APP_IMAGE > /dev/null 2>&1 ; then
  echo "The Application Image is ready for use. Type ./run.sh to run it.";
else
  echo "ERROR: Can't build the $APP_IMAGE image";
fi
