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
from rclpy.action import ActionServer
from wild_sight_interfaces.action import ProcessImage
import subprocess
import os
import threading


class SnapshotActionServerNode(Node):

    def __init__(self):
        super().__init__('snapshot_action_server')

        self._server = ActionServer(
            self,
            ProcessImage,
            'process_image',
            self.execute_callback)

        self.get_logger().info("Snapshot Action Server ready")

    def execute_callback(self, goal_handle):
        image_path = goal_handle.request.image_path
        self.get_logger().info(f"Start processing: {image_path}")

        try:
            predictions_json = "/root/ros2_ws/snapshots/out.json"
            results_dir = "/root/ros2_ws/results"

            subprocess.run([
                "rm", "-f", "snapshots/out.json"
            ], check=True)

            subprocess.run([
                "python3", "-m", "speciesnet.scripts.run_model",
                "--model", "SpeciesNet",
                "--filepaths", image_path,
                "--predictions_json", predictions_json
            ], check=True)
            self.get_logger().info("MegaDetector and SpeciesNet finished")

            subprocess.run([
                "python3", "-m", "megadetector.visualization.visualize_detector_output",
                "--images_dir", "/root/ros2_ws",
                "--preserve_path_structure",
                "--output_image_width", "-1",
                predictions_json,
                results_dir
            ], check=True)
            self.get_logger().info("Visualization finished")

            result = ProcessImage.Result()
            result.result_image_path = os.path.join(
                results_dir, os.path.basename(image_path))

            self.get_logger().info("Done.")
            goal_handle.succeed()
            return result

        except subprocess.CalledProcessError as e:
            self.get_logger().error(f"ERROR: {e}")
            goal_handle.abort()
            return ProcessImage.Result()

def main(args=None):
    rclpy.init(args=args)
    node = SnapshotActionServerNode()
        
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.destroy_node()
        return

if __name__ == '__main__':
    main()
