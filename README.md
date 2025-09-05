# Wild-Sight-AI

Wild-Sight-AI is a project designed for the Kria KR260 board that enables AI-driven camera tracking and animal detection. The project integrates custom hardware and software, including a RS-485 PMOD module for camera rotator control and ROS2 nodes for real-time processing and communication. Follow our comprehensive guide on [Hackster.io](https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa) to build, test, and deploy the system, transforming your camera into an intelligent, autonomous tracking device.

## Requirements

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

    For a visual guide on building and installing the Wild-Sight-AI application, watch this [YouTube video](https://www.youtube.com/watch?v=w_0K5YZrkO0).

    [![Building and Installing the Wild Sight AI Application](https://img.youtube.com/vi/w_0K5YZrkO0/hqdefault.jpg)](https://www.youtube.com/watch?v=w_0K5YZrkO0)

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

    For a visual guide on starting the Wild-Sight-AI application, watch this [YouTube video](https://www.youtube.com/watch?v=IakoRX5yPNo).

    [![Starting the Wild Sight AI Application](https://img.youtube.com/vi/IakoRX5yPNo/hqdefault.jpg)](https://www.youtube.com/watch?v=IakoRX5yPNo)

## License

This project is licensed under the GPL-3.0. See the LICENSE file for details.

For further information or support, please refer to the project documentation on [Hackster.io](https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa).
