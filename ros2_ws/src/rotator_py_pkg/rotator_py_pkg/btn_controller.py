#!/usr/bin/env python3
#
# Eagle-Eye-AI
# Smart Following Camera with Face Recognition
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - July 2024
#
# Push-Button Controller
#   - Sends the manual camera movement commands to Rotator Controller
#   - Reads the GPIO register status of two buttons and sends
#     commands through ROS2 topic messages
#   - Left button moves camera left, right button moves right
#   - Both buttons pressed move the camera's tilt motor. It tilts
#     the camera up or down, depends on which button is pressed first
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

class BtnController(Node):

    def __init__(self):
        super().__init__('btn_controller')

        # Open /dev/mem for direct register access
        self.mem_fd = os.open('/dev/mem', os.O_RDWR | os.O_SYNC)
        self.mem = mmap.mmap(self.mem_fd, mmap.PAGESIZE, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=0x80010000)

        # Publisher for /motor_control
        self.publisher_ = self.create_publisher(MotorCmd, '/motor_control', 10)

        # Initialize button states
        self.button1_state = False
        self.button2_state = False

        # Create a timer to periodically check the button states
        self.timer = self.create_timer(0.2, self.check_buttons)  # Check every 200ms

        self.get_logger().info('Node created.')

    def check_buttons(self):
        # Read button register value
        button_register = self.read_register(0x08)

        # Process button 1
        button1_pressed =  ~button_register & 0x01
        if button1_pressed and not self.button1_state:
            self.button1_state = True
            self.handle_button1_press()
        elif not button1_pressed and self.button1_state:
            self.button1_state = False
            self.handle_button1_release()

        # Process button 2
        button2_pressed = ~button_register & 0x02
        if button2_pressed and not self.button2_state:
            self.button2_state = True
            self.handle_button2_press()
        elif not button2_pressed and self.button2_state:
            self.button2_state = False
            self.handle_button2_release()

    def handle_button1_press(self):
        if not self.button2_state:
            self.publish_motor_cmd(32, 0)  # Pan left
        else:
            self.publish_motor_cmd(0, 32)  # both buttons detected, tilt up

    def handle_button1_release(self):
        self.publish_motor_cmd(0, 0)

    def handle_button2_press(self):
        if not self.button1_state:
            self.publish_motor_cmd(-32, 0)  # Pan right
        else:
            self.publish_motor_cmd(0, -32)  # Both buttons detected, tilt down

    def handle_button2_release(self):
        self.publish_motor_cmd(0, 0)

    def publish_motor_cmd(self, pan_speed, tilt_speed):
        msg = MotorCmd()
        msg.pan_speed = pan_speed
        msg.tilt_speed = tilt_speed
        self.publisher_.publish(msg)

    def read_register(self, offset):
        self.mem.seek(offset)
        return int.from_bytes(self.mem.read(1), byteorder='little')

    def destroy_node(self):
        # Clean up resources here
        self.mem.close()
        os.close(self.mem_fd)
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    node = BtnController()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.destroy_node()
        return

if __name__ == '__main__':
    main()
