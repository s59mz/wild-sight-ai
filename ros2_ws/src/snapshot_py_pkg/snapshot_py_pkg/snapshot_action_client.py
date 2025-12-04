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
from rclpy.action import ActionClient
from wild_sight_interfaces.action import ProcessImage
from wild_sight_interfaces.msg import SendSnapshot


class SnapshotActionClientNode(Node):

    def __init__(self):
        super().__init__('snapshot_action_client')

        self._client = ActionClient(self, ProcessImage, 'process_image')

        # Subscribe to the topic carrying the snapshot filename
        self.subscription = self.create_subscription(SendSnapshot, '/send_snapshot', self.snapshot_callback, 10)

        # Publisher for detections
        self.publisher = self.create_publisher(SendSnapshot, '/snapshot_complete', 10)

        self.get_logger().info("Client ready - waiting for image filenames")

    def snapshot_callback(self, msg: SendSnapshot):
        image_path = msg.filename
        self.get_logger().info(f"Trigger received: {image_path}")

        goal = ProcessImage.Goal()
        goal.image_path = image_path

        # Ensure action server is running
        self._client.wait_for_server()

        # Send goal async
        send_future = self._client.send_goal_async(goal)
        send_future.add_done_callback(self.on_goal_sent)

    def on_goal_sent(self, goal_future):
        goal_handle = goal_future.result()

        if not goal_handle.accepted:
            self.get_logger().error("Goal was rejected by the server!")
            return

        self.get_logger().info("Goal accepted, waiting for result...")

        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(self.on_result)

    def on_result(self, result_future):
        result = result_future.result().result

        msg = SendSnapshot()
        msg.filename = result.result_image_path
        self.publisher.publish(msg)

        self.get_logger().info(
            f"Processing finished - result image: {result.result_image_path}"
        )


def main(args=None):
    rclpy.init(args=args)
    node = SnapshotActionClientNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.destroy_node()
        return

if __name__ == '__main__':
    main()
