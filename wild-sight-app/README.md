# Wild-Sight-AI Application

## Overview

The Wild-Sight-AI application is designed to run within a Docker container on the **KRIA™ KR260 board**, leveraging the AMD **Smartcam** demo application as a foundation. This application includes enhancements such as an updated camera status line, reflecting real-time camera orientation.

## Directory Structure

The repository contains the following directory structure:
```
wild-sight-app/
├── cmake
│   └── FindGStreamer.cmake
├── CMakeLists.txt
├── config
│   └── facedetect
│       ├── aiinference.json
│       ├── drawresult.json
│       └── preprocess.json
├── models
│   └── kr260
│       └── megadetector
│           ├── megadetector.prototxt
│           ├── megadetector.xmodel
│           └── md5sum.txt
├── README
└── src
    ├── vvas_airender.cpp
    ├── vvas_airender.hpp
    └── vvas_xpp_pipeline.c
```

## Key Components

-	`src/vvas_airender.cpp`: This file has been modified to include the camera status line, which displays real-time camera orientation on the screen
-	`config/facedetect/*.json`: These configuration files have updated paths for firmware and the VVAS library.
- `CMakeLists.txt`: Adapted for the updated configuration and paths.

The other files and models are directly taken from the official **Smartcam** demo application by AMD and remain unmodified.

## Building the Application

### Prerequisites

-	**Kria Board**: The application must be built directly on the **KRIA™ KR26 board**.
- **Docker**: The build process is fully automated via Docker. Ensure you are using the xilinx/kria-developer Docker image.

### Build Process

1. **Prepare the KRIA Board**: Ensure that the KRIA™ KR260 board is set up with the appropriate Ubuntu image and Docker.
2. **Clone the Repository**: Obtain the Wild-Sight-AI repository.
3. **Build the Application**:
   - The build process is fully automated and configured in the Dockerfile located in the parent directory. Follow the instructions in the Docker setup to build the application directly on the KRIA board.

### Dockerfile

The Dockerfile is designed to automate the build process for the Wild-Sight-AI application within the Docker container. It ensures that all dependencies are correctly installed and configured.

## Usage

Once built, you can run the Wild-Sight-AI application within the Docker container on the KRIA board. The application will provide real-time face tracking and display the camera status line as described.

For detailed instructions on running the application, refer to the project documentation published on [Hackster.io](https://www.hackster.io/matjaz4/eagleeye-ai-smart-following-camera-with-face-recognition-1b0f65).

## Additional Notes

- This application is tailored for the KRIA™ KR260 board and is not intended to be built on a host PC.
- The models directory remains unchanged from the Smartcam demo application and includes pre-trained models required for face detection.

