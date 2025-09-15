# Dockerfiles — Wild-Sight-AI (Kria KR260)

This folder contains the two Dockerfiles used to build and run **Wild-Sight-AI** on a Kria KR260:

1) **`kria-image-docker`** — a runtime image for Kria with Vitis AI 3.5 + VVAS 3.0 and all userspace dependencies preinstalled.  
2) **`wild-sight-docker`** — the final application image (ROS 2 Humble + GStreamer + Wild-Sight-AI binaries), built **FROM** the runtime image above.

Both images are built automatically by the repo-root script `build.sh`. You normally won’t need to call `docker build` yourself unless you want to customize.

---

## What you get (versions)

The **Kria runtime image** installs:

- **Vitis-AI 3.5** (VART 3.5.0, XIR 202310.2.15.225)  
- **VVAS 3.0** (built from source)  
- **XRT headers 2.15** (userspace headers baked in for builds)  
- **OpenCV 4.6** (core/imgproc/imgcodecs/dnn; either copied from `libs/opencv406.zip` or built from source)  
- **Protobuf 3.21.3** (built from source)  
- GStreamer 1.20+ stack and plugins required by VVAS (incl. `vvas_xinfer`)  
- Helper tools: `xbutil`, `xdputil` (needs `libdfx.so.1.0`, shipped)

The **application image** adds:

- **ROS 2 Humble** (ros-base)  
- **Wild-Sight-AI** binaries and assets (installed under `/`)  
- Python deps for device control (`pyserial`, `pymodbus`)  
- A friendly shell welcome and ROS 2 sourcing in `.bashrc`

---

## Prerequisites

### On the **Kria KR260** (target)
- FPGA bitstream with **DPUCZDX8G v4.0**  
- **`zocl` kernel module v2.15** (matching XRT 2.15 userspace)  
- An Ubuntu 22.04-based rootfs (or compatible) with Docker Engine installed  
- Network access to download Vitis-AI 3.5 zip and GitHub sources

> You can verify the DPU is alive with:
>
> ```bash
> sudo modprobe zocl
> xbutil --version
> xdputil query
> ```

### On the **host** (optional cross-build)
- Docker 20.10+  
- ~20–30 GB free disk space  
- Reliable internet (OpenCV / Protobuf / VVAS source builds can be large)  
- ARM64 build environment (native KR260 is simplest; QEMU works but is slower)

---

## Layout

```
dockerfiles/
  kria-image-docker          # base Kria/Vitis-AI/VVAS runtime image
  wild-sight-docker          # final Wild-Sight-AI app image (ROS 2 Humble)
libs/
  opencv406.zip              # optional cache of OpenCV 4.6 libs (speeds build)
scripts/
  bashrc, welcome.sh         # shell UX helpers baked into images
build.sh                     # builds both images (preferred entry point)
run.sh                       # runs final app image (maps devices/volumes)
```

> If `libs/opencv406.zip` exists, the build **reuses** it; otherwise the runtime image **compiles OpenCV 4.6** from source and then packs it for future builds.

---

## Quick Start

From the **repo root**:

```bash
./build.sh
```

- If `kria-image:3.5` doesn’t exist, it builds it first.  
- Then it builds `wild-sight-ai:1.0`.

To run the application:

```bash
./run.sh
```

> `run.sh` is responsible for giving the container access to FPGA and video devices (e.g. mapping `/dev`, setting `--privileged`, exporting env vars, mounting model files, etc.). If you customize hardware paths, do it there.

---

## Manual builds (advanced)

Build base runtime image:

```bash
docker build \
  --network host \
  --build-arg BUILD_DATE="$(date -u +'%Y/%m/%d %H:%M')" \
  -f dockerfiles/kria-image-docker \
  . -t kria-image:3.5
```

Build application image:

```bash
docker build \
  --network host \
  --build-arg BUILD_DATE="$(date -u +'%Y/%m/%d %H:%M')" \
  -f dockerfiles/wild-sight-docker \
  . -t wild-sight-ai:1.0
```

---

## Notes & Customization

- **OpenCV cache**: To avoid compiling OpenCV each time, drop a prebuilt `opencv406.zip` into `libs/` before building. If it’s missing, the Dockerfile builds OpenCV and creates the zip automatically for reuse.

- **VVAS build**: The runtime image clones **VVAS `VVAS_REL_v3.0`** and builds:
  - `vvas-core`, `vvas-utils`, `vvas-gst-plugins`, and `vvas-accel-sw-libs`

- **XRT headers only**: The container includes **XRT 2.15 headers** for compilation; the **kernel driver (`zocl`) and runtime kernel modules must be on the host OS**.

- **Vitis-AI 3.5 packages**: Pulled from AMD’s public download (`vai3.5_kr260.zip`) and installed via dpkg inside the runtime image.

- **ROS 2 Humble**: Installed from `ros2-testing` Jammy repo; final image sources `/opt/ros/humble/setup.bash` automatically for convenience.

- **Environment**:
  - `GST_PLUGIN_PATH=/usr/lib/aarch64-linux-gnu/gstreamer-1.0`
  - `LD_LIBRARY_PATH=/usr/lib:/usr/local/lib`
  - `PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig`

---

## Troubleshooting

- **DPU not visible / model won’t run**
  - Ensure `zocl` v2.15 is loaded on the host: `sudo modprobe zocl`
  - Confirm `xdputil query` shows the DPU
  - Make sure `run.sh` gives the container the necessary device access (privileged or device mappings)

- **Slow build**
  - Build **on the KR260** for the simplest path (no QEMU).  
  - Keep `libs/opencv406.zip` to skip OpenCV compilation next time.

- **GStreamer plugin not found**
  - Check `GST_PLUGIN_PATH` inside the running container.
  - Clear registry: `rm -f ~/.cache/gstreamer-1.0/registry.*`

- **Version mismatch errors**
  - This stack expects **DPU v4.0** and **zocl 2.15**.
  - If you update one component (e.g. XRT or Vitis-AI), expect to rebuild VVAS.

- **`xbutil` / `xdputil` not found**
  - `xbutil` is installed as `/usr/bin/xbutil` inside the container.
  - `xdputil` comes with the VART tools; if it complains about `libdfx`, we ship `libs/libdfx.so.1.0` in the image.

---

## Performance & Build Time

- First build on KR260: **~2 hours** (VVAS + OpenCV + Protobuf from source).  
- Subsequent builds are much faster if OpenCV is cached.  
- Final application runtime (current state): **~3.6 FPS** on MegaDetector v5 (quantized), pending further optimization.

---

## License & Attribution

- This repository includes third-party components:
  - AMD Xilinx **Vitis-AI**, **VVAS**, **XRT**
  - **OpenCV** (BSD-3-Clause)
  - **Protobuf** (BSD-3-Clause)
  - **ROS 2** (various OSS licenses)
- Please review and comply with their respective licenses when redistributing images.

---

## Support

Issues / PRs are welcome — especially around:
- Improving build speed & caching
- Alternate base images or package mirrors
- Automated tests for VVAS/GStreamer integration
- Tips for KR260 deployment workflows

If you run into blockers, please open a GitHub issue with:
- KR260 OS version  
- zocl/XRT versions  
- Full `docker build` log (tail)  
- Output of `xdputil query` and `xbutil --version` on the host

---

**Happy building — and welcome to Wild-Sight-AI!**
