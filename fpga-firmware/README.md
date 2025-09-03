 firmware-kr260-wild-sight

`kr260-wild-sight` is an FPGA firmware for the Wild-Sight-AI application that runs on the Kria kr260 or kv260 board made by AMD. 

This file demonstrates how to package files into a .deb package for Ubuntu.

## The firmware-kr260-wild-sight Directory Structure

- `DEBIAN/`: Contains packaging metadata.
- `/lib/firmware/xilinx/kr260-wild-sight/`: Contains the fpga firmware files.

## Building the .deb Package

To build the .deb package, run the following command:

`dpkg-deb --build firmware-kr260-wild-sight`

To install the `firmware-kr260-wild-sight.deb` package, copy the file on 
the Kria board in /tmp folder. Then run the following command:

`sudo apt install /tmp/firmware-kr260-wild-sight.deb`

To list the installed files, run:

`dpkg -L firmware-kr260-wild-sight.deb`


## The Vivado `src` Directory Structure

- `block_design/`: Contains a Vivado TCL script that generates the block design for extensible platform.
- `xdc`: Contains the the Vivado constraints file for RS485 PMOD pins and other external signal.
- `dts`: Contains the the device tree data structure update data
- `patch`: Contains the the Vivado IP patch file for Uart Lite IP for using RS-485 interface

## Applying the Patch to UartLite IP in Vivado for Using the RS-485 PMOD Module 

To apply it to the original Vivado installation, go to its installation dir and go to .../Vivado/2023.1/data/ip/xilinx/axi_uartlite_v2_0. Copy the patch/axi_uartlite_v2_0_rs485.patch into this directory.  Then run patch to modify the sources. If you want you can test with --dry-run first.

```
cd /opt/Xilinx/Vivado/2023.1/data/ip/xilinx/axi_uartlite_v2_0
patch --dry-run -p1 < axi_uartlite_v2_0_rs485.patch
patch -p1 < axi_uartlite_v2_0_rs485.patch
```

This patch should be applied before building the Vitis Overlay

## How to use the Vivado files

Get the Official Kria Vitis Platform repository

```
git clone --recursive --branch xlnx_rel_v2023.1 https://github.com/Xilinx/kria-vitis-platforms.git
cd kria-vitis-platforms/kv260
```

Copy generated block design tcl script from platform building over old one here:

```
cp block_design/config_bd.tcl  kria-vitis-platforms/kv260/platforms/vivado/kv260_ispMipiRx_vcu_DP/scripts/config_bd.tcl
```

Copy constraint file pin.xdc from platform ditectory over old one here

`cp xdc/pin.xdc kria-vitis-platforms/kv260/platforms/vivado/kv260_ispMipiRx_vcu_DP/xdc/pin.xdc`

Build the overlay - it takes abour 4 hours

```
cd kria-vitis-platforms/kv260

make clean
make overlay OVERLAY=smartcam
```

The generated bitfile and xclbin will be located at:

```
 overlays/examples/smartcam/binary_container_1/link/int/system.bit
 overlays/examples/smartcam/binary_container_1/dpu.xclbin
```

Convert system.bit to a .bit.bin file:

```
cd overlays/examples/smartcam/binary_container_1/link/int/

echo 'all:{system.bit}'>bootgen.bif
bootgen -w -arch zynqmp -process_bitstream bin -image bootgen.bif

ls -l system.bit.bin
cd -

cp overlays/examples/smartcam/binary_container_1/link/int/system.bit.bin ./kr260-wild-sight.bit.bin
cp overlays/examples/smartcam/binary_container_1/dpu.xclbin ./kr260-wild-sight.xclbin
```

## Generate Device Tree

Get the original smartcam dtsi

`wget https://raw.githubusercontent.com/Xilinx/kria-apps-firmware/xlnx_rel_v2023.1/boards/kv260/smartcam/kv260-smartcam.dtsi`

Open the `kv260-smartcam.dtsi` file and add the content of the `dts/update_dtsi.txt` file in the same place where the unused already commented out audio pipeline is (lines from 234 to 285). Remove the commented out audio module.

Compile dtb with dtc

`dtc -@ -I dts -O dtb -o kr260-wild-sight.dtbo kv260-smartcam.dtsi`

