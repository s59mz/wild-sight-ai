# Wild-Sight-AI

Wild-Sight-AI is a project designed for the Kria KR260 board that enables AI-driven camera tracking and animal detection. The project integrates custom hardware and software, including a RS-485 PMOD module for camera rotator control and ROS2 nodes for real-time processing and communication. Follow our comprehensive guide on [Hackster.io](https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa) to build, test, and deploy the system, transforming your camera into an intelligent, autonomous tracking device.

# ARM AI Developer Challenge: New CPU-Based Snapshot Inference

This branch includes additional features developed specifically for the **ARM AI Developer Challenge 2025**.
The key enhancement is a new **snapshot-based inference pipeline** that performs **full-precision animal detection and classification entirely on the Arm Cortex-A53 CPU**, without using the DPU or any hardware accelerator.

## Snapshot Capture

A physical **push button**, connected through the **PMOD interface**, triggers a ROS 2 topic that instructs the GStreamer pipeline to capture the current video frame.
The snapshot is saved as a .jpg file to the KR260 SD card.

## CPU-Only Inference (High Accuracy Mode)

After the snapshot is captured, a ROS 2 Action node launches a **CPU-only inference workflow** using:
	•	**MegaDetector** v5 (.pt) — full 32-bit precision model
	•	**SpeciesNet** (.onnx) — full 32-bit precision classifier
	•	PyTorch + ONNX Runtime, running natively on the **Cortex-A53 cores**

This mode prioritizes **accuracy over speed**, and therefore takes around **2 minutes per inference** on the embedded CPU.
The resulting annotated image (with bounding boxes and class names) is written back to the SD card and can be accessed through the device’s shared storage.

## Purpose of This Feature

This new pipeline demonstrates that **wildlife detection and species classification can run fully on Arm CPU**, without the PL/DPU and without cloud services — a key requirement and focus of the ARM AI Developer Challenge.

# Requirements

1. **Kria KR260 Board**: Ensure that your KRIA™ KR260 board has the official Ubuntu image with updatet zocl v2.15 kernel module installed and Docker set up. The board should be prepared for running official demo applications from AMD, such as the **Smartcam demo** application.

2. **IP Camera**: You will need an IP camera that supports RTSP streaming with a resolution of 1920x1080. The camera should be connected to the same local network as the Kria board. A recommended camera is the [SIMICAM 4k Video Cam](https://a.aliexpress.com/_EznpRub) or similar.

3. **Pan-Tilt Rotator**: A Pan-Tilt Camera Rotator that supports RS-485 and the Pelco-P/D protocol is required for rotating the camera. A recommended rotator is the [PTZ Rotator](https://a.aliexpress.com/_EvhGQMB).

4. **RS-485 PMOD Module**: Required for camera rotator control. The Camera Rotator should support the Pelco-D protocol for Pan-Tilt through RS-485 interanimal. The module can be found [here](https://github.com/s59mz/kicad-pmod_rs485).

5. **Network Connection**: Connect the Ethernet cable to your local network with DHCP enabled.

## Getting the Application Package

1. **Clone the Repository**:

    ```bash
    git clone https://github.com/s59mz/wild-sight-ai.git
    cd wild-sight-ai
    ```

## Install Firmware Binaries

1. Install the firmware binaries:

    ```bash
    cp fpga-firmware/firmware-kr260-wild-sight.deb /tmp
    sudo apt install /tmp/firmware-kr260-wild-sight.deb
    ```

2. Dynamically load the firmware package:

    * Switch to the kr260-wild-sight platform:

      ```bash
      sudo xmutil unloadapp
      sudo xmutil loadapp kr260-wild-sight
      ```
    
    * Show the list and status of available acceleration platforms:

      ```bash
      sudo xmutil listapps
      ```

3. Disable the desktop environment:

      ```bash
      sudo xmutil desktop_disable
      ```

## Building the Docker Image

1. **Update the RTSP IP Camera URL**:

    Edit the `run_app.sh` script file in the `ros2_ws` directory and update the `default_camera_url` parameter:

    ```bash
    vi ros2_ws/run_app.sh

    # Update line #18 with your IP camera URL:
    default_camera_url="rtsp://192.168.1.11:554/stream1"
    ```

2. **On First Run Only: Build the Docker Image**:

    The build process will take about 2 hours on the Kria board. This cannot be built on a host PC unless you can build docker images for arm64 architecture.

    ```bash
    ./build.sh
    ```

## Launching the Docker Image

1. **Launch the Docker Image**:

    ```bash
    ./run.sh
    ```

    This will start the Wild-Sight-AI Docker image in a new container:

    ```bash
    root@xlnx-docker/#
    ```

2. **On First Run only: Build the ROS2 Packages**:

    To build the ROS2 packages:

    ```bash
    colcon build
    ```

3. **On First Run only: Install SpeciesNet**:

    This is needed for full-precision animal detection and classification entirely on the Arm Cortex-A53 CPU, without using the DPU or any hardware accelerator.

    ```bash
    pip install speciesnet
    ```

4. **On First Run only: Install MegaDetector Utils**:

    Needed for drawing bounding boxes around detected animals on snapshots:

    ```bash
    pip install megadetector-utils
    ```

5. **On First Run only: Download 32-bit Models**:

    In the top directory of this repo install the original SpeciesNet model:

    ```bash
    cd SpeciesNet
    wget https://www.kaggle.com/api/v1/models/google/speciesnet/pyTorch/v4.0.1a/1/download -O speciesnet-pytorch-v4.0.1a-v1.tar.gz
    tar -xvf speciesnet-pytorch-v4.0.1a-v1.tar.gz
    ```

6. **On First Run only: Save a running Docker container as a new image**:

    Make sure that the run.sh script starts this new image:

    ```bash
    docker ps
    docker commit <id> <image-name>:tag
    ```

## Running the Application

1. In the running Wild-Sight-AI Docker container:

    * Launch the application:

      ```bash
      ./run_app.sh
      ```

      You should see the camera’s captured images on the monitor connected to the board. When an animal is detected, a boundary box will appear around it, tracking the animal as it moves. The camera rotator will also adjust to keep the detected animal centered on the screen.

    * Press `Ctrl-C` to exit.

    * To change the RTSP IP camera URL, run the startup script with the new URL:

      ```bash
      ./run_app.sh rtsp://192.168.1.20:554/stream1
      ```

    Watch a live demo of Wild-Sight-AI detecting animals and humans in real-time on the Kria board, raising alerts for potential human-wildlife conflicts, and tracking detected objects.

    [![Wild-Sight-AI – Real-Time Animal & Human Conflict Detection on Kria DPU](https://img.youtube.com/vi/Weteg4Qui8w/hqdefault.jpg)](https://www.youtube.com/watch?v=Weteg4Qui8w)

## 📸 Snapshot Capture and CPU Inference Results

The application includes a snapshot feature designed for the **ARM AI Developer Challenge**.  
It allows you to capture a single frame from the live video stream and run **high-accuracy, CPU-only inference** directly on the KR260’s Arm Cortex-A53 cores.

### How to Trigger a Snapshot
Press the **left physical push button** on a custom RS-485 module connected to the **PMOD interface**.  
When pressed, the system:

1. Captures the current video frame from the GStreamer pipeline  
2. Saves the raw snapshot as a `.jpg` file in the folder:

      ```bash
      snapshots/
      ```

3. Starts a CPU-only inference workflow (MegaDetector + SpeciesNet)  
4. Generates an annotated output image containing:  
- bounding boxes  
- class names  
- detection confidence  

### Where Results Are Stored
After inference completes, the processed image is saved in the directory:  

      ```bash
      results/
      ```
The original snapshot and the final annotated result share similar filenames, making it easy to compare input vs. inference output. All files remain safely stored on the SD card, even after the Docker container exits.

This feature showcases accurate, full-precision animal detection and classification running **entirely on the Arm CPU**, without DPU acceleration or cloud services.

## License

This project is licensed under the GPL-3.0. See the LICENSE file for details.

For further information or support, please refer to the project documentation on [Hackster.io](https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa).
