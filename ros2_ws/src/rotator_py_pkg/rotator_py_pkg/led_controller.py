#!/usr/bin/env python3
#
# Eagle-Eye-AI
# Smart Following Camera with Face Recognition
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - July 2024
#
# Led Controller
#   - Controls two status LEDs on a custom made RS-485 PMOD module
#     through the register of the AXI GPIO IP block
#   - Listens for rotator motor commands on ROS2 topic and 
#     turns On the Red or Blue LED when Pan or Tilt motor 
#     is active, respectively
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#


import rclpy
from rclpy.node import Node
from rotator_interfaces.msg import MotorCmd

import os
import mmap

class LedController(Node):

    def __init__(self):
        super().__init__('led_controller')

        # Initialize motor speeds
        self.pan_speed_ = 0
        self.tilt_speed_ = 0

        # Open /dev/mem for direct register access
        self.mem_fd_ = os.open('/dev/mem', os.O_RDWR | os.O_SYNC)
        self.mem_ = mmap.mmap(self.mem_fd_, mmap.PAGESIZE, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=0x80010000)

        # Create a subscriber to listen to /motor_control topic
        self.subscription_ = self.create_subscription(MotorCmd, '/motor_control', self.motor_control_callback, 10)

        # Create a timer to periodically update registers
        self.timer_ = self.create_timer(0.5, self.update_registers)

        self.get_logger().info('Node created.')

    def motor_control_callback(self, msg: MotorCmd):
        self.pan_speed_ = msg.pan_speed
        self.tilt_speed_ = msg.tilt_speed

    def update_registers(self):
        register_value = 0
        if self.pan_speed_ != 0:
            register_value |= 0x01  # Set bit0 if pan_speed is non-zero to turn on LED-1
        if self.tilt_speed_ != 0:
            register_value |= 0x02  # Set bit1 if tilt_speed is non-zero to turn on LED-2

        # Write to the register
        self.write_register(0x00, register_value)

    def write_register(self, offset, value):
        self.mem_.seek(offset)
        self.mem_.write_byte(value)

    def destroy_node(self):
        # Clean up resources here
        self.mem_.close()
        os.close(self.mem_fd_)
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    node = LedController()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.destroy_node()
        return

if __name__ == '__main__':
    main()
