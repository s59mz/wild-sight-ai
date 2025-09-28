#!/usr/bin/env python3
#
# Eagle-Eye-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - July 2024
#
# Camera Controller
#   - Moves tha camera to follow a detected object/animal
#   - Listens for the detected object coordinates on ROS2 topic
#   - Calculates the required camera movement and sends motor
#     commands to the Rotator Controller through ROS2 topic messages
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#

import rclpy
import serial

from rclpy.node import Node

from rotator_interfaces.msg import MotorCmd, SwitchCmd
from wild_sight_interfaces.msg import ObjectDetect

class CameraControllerNode(Node): 
    def __init__(self):
        super().__init__("camera_controller")

        # get configurable parameters
        self.declare_parameter("updating_period", 0.2)
        self.updating_period_ = self.get_parameter("updating_period").value

        self.declare_parameter("frame_timeout", 5)
        self.frame_timeout_ = self.get_parameter("frame_timeout").value

        self.subscriber_ = self.create_subscription(
            ObjectDetect, "object_detect", self.callback_object_detect, 10
        )

        # create publishers
        self.motor_publisher_ = self.create_publisher(MotorCmd, "motor_control", 10)
        self.switch_publisher_ = self.create_publisher(SwitchCmd, "switch_control", 10)

        self.lamp_on_ = False
        self.object_detected_ = False
        self.updating_timer_ = None

        # object tracking state
        self.object_tracking_ = False
        self.frame_count_ = 0
        self.lamp_count_ = 0

        # motor speed definition
        self.pan_speed_: int = 0
        self.tilt_speed_: int = 0

        self.motor_cmd_ = MotorCmd()
        self.switch_cmd_ = SwitchCmd()

        self.update_motor_speed()
        self.switch_lamp(False, True)

        self.get_logger().info("Node Created")

    def callback_object_detect(self, msg: ObjectDetect):
        # turn the lamp on if conflict is detected
        self.switch_lamp(msg.conflict_detected, False)

        if (msg.animal_detected):
            self.frame_count_ = 0

            # calculate object position relative to frame center
            center_x: int = msg.frame_width / 2;
            center_y: int = msg.frame_height / 2;

            object_center_x: int = msg.bbox_x + msg.bbox_width / 2;
            object_center_y: int = msg.bbox_y + msg.bbox_height / 2;

            self.offset_y = object_center_y - center_y;
            self.offset_x = object_center_x - center_x;

            # calculate required motor speed for tracking
            if (abs(self.offset_x) > msg.frame_width / 8):
                self.pan_speed_ = (int) (- self.offset_x * 240 / msg.frame_width)
            else:
                # object is centered enough in X
                self.pan_speed_ = 0

            if (abs(self.offset_y) > msg.frame_height / 4):
                self.tilt_speed_ = (int) (- self.offset_y)
            else:
                # object is centered enough in Y
                self.tilt_speed_ = 0

            if not self.object_tracking_:
                self.start_object_tracking()

        else:
            if self.object_tracking_:
                self.frame_count_ += 1

                if (self.frame_count_ > self.frame_timeout_):
                    self.stop_object_tracking()

    def start_object_tracking(self):
        self.object_tracking_ = True

        # start the motor imediately
        self.update_motor_speed()

        # start updating timer
        if (self.updating_timer_ == None):
            self.updating_timer_ = self.create_timer(self.updating_period_, 
                                                 self.update_motor_speed)


    def stop_object_tracking(self):
        self.object_tracking_ = False

        # stop the motor imediately
        self.pan_speed_ = 0
        self.tilt_speed_ = 0
        self.update_motor_speed()

        # stop updating timer
        if self.updating_timer_:
            self.updating_timer_.cancel()
            self.updating_timer_ = None

    def update_motor_speed(self):
        self.motor_cmd_.pan_speed = self.pan_speed_
        self.motor_cmd_.tilt_speed = self.tilt_speed_
        self.motor_publisher_.publish(self.motor_cmd_)

    def switch_lamp(self, lamp_state, force):
        if force or lamp_state:
            self.switch_cmd_.switch_on = lamp_state
            self.switch_publisher_.publish(self.switch_cmd_)
            self.lamp_count_ = 0
        else:
            if (self.switch_cmd_.switch_on != lamp_state):
                self.lamp_count_ += 1
                if self.lamp_count_ > 10:
                    self.switch_cmd_.switch_on = lamp_state
                    self.switch_publisher_.publish(self.switch_cmd_)
                    self.lamp_count_ = 0
            else:
                self.lamp_count_ = 0


def main(args=None):
    rclpy.init(args=args)

    node = CameraControllerNode() 

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        return

if __name__ == "__main__":
    main()



