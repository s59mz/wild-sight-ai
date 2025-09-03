/*
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
# GStreamer Pipeline
# Camera Orientation custom user data 
#   - Put this struct in the reserved_1 pointer field
#   - The GStreamer probe populates Inference Metadata with this struct
#     with the data read from the camera Inclinometer, so the VVAS Draw
#     Filter can show the Camera's Azimuth and Elevation Status
#     in the video stream.
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
*/

typedef struct _CameraOrientation
{
  float azimuth;
  float elevation;
} CameraOrientation;

