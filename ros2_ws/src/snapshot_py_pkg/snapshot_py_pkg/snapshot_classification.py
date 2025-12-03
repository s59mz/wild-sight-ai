#!/usr/bin/env python3
#
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - December 2025
#
# Snapshot Classification
#   - Received a snapshot .jpg filename on /send_snapshot topic
#   - Make Animal Detection by using MegaDetectorV5a detector
#   - Make Animal classification by using SpeciesNet classificator
#   - Create a nev .jpg snapshot with bounding boxes and animal species
#   - Publish a new created .jpg filename on /detected_animals topic
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Uses:
#   - MegaDetectorV5a detector
#   - SpeciesNet classificator
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
#



import rclpy
from rclpy.node import Node
from wild_sight_interfaces.msg import SendSnapshot


class SnapshotClassificationNode(Node):

    def __init__(self):
        super().__init__('snapshot_classification_node')

        # Subscribe to the topic carrying the snapshot filename
        self.subscription = self.create_subscription(SendSnapshot, '/send_snapshot', self.snapshot_callback, 10)

        # Publisher for inference
        self.inference_publisher_ = self.create_publisher(SendSnapshot, '/detected_animals', 10)

        self.get_logger().info("Snapshot Receiver Node started...")

    def snapshot_callback(self, msg: SendSnapshot):
        filename = msg.filename
        self.get_logger().info(f"Received snapshot filename: {filename}")

        self.process_snapshot(filename)

    def process_snapshot(self, filename: str):
        # TODO: implement your logic here
        self.get_logger().info(f"Processing snapshot: {filename}")
         
        self.publish_detected_animals('out.json')

    def publish_detected_animals(self, filename):
        msg = SendSnapshot()
        msg.filename = filename
        self.inference_publisher_.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = SnapshotClassificationNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.destroy_node()
        return

if __name__ == '__main__':
    main()
