/*
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - August 2025
#
# GStreamer Pipeline
#   - Creates a GStreamer pipeline based on Smartcam demo app
#     in a ROS2 Node with RTSP video camera stream as an input.
#   - Added a probe to GStreamer element that extract the 
#     coordinates of the detected object and published that data
#     on ROS2 topic.
#   - The probe also populates Inference Metadata with the 
#     data read from the camera Inclinometer, so the VVAS Draw
#     Filter can show the Camera's Azimuth and Elevation Status
#     in the video stream.
#
# Design based on Kria KV260 Smartcam Demo App by AMD
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4
*/


#define VVAS_GLIB_UTILS 1
#include <glib.h>

#include <gst/gst.h>
#include <rclcpp/rclcpp.hpp>
#include "wild_sight_interfaces/msg/object_detect.hpp"
#include "wild_sight_interfaces/msg/camera_orientation.hpp"

#include "wild_sight_pkg/cameradata.h"
#include "wild_sight_pkg/gstinferencemeta.h"


// ROS2 Node Class
class GStreamerPipeline : public rclcpp::Node {
public:
    GStreamerPipeline()
        : Node("gstreamer_pipeline") {

	// define node input parameters
	std::string camera_url_ = "";
	this->declare_parameter<std::string>("camera_url", "rtsp://192.168.1.11:554/stream1");
	this->get_parameter("camera_url", camera_url_);

	// create a publisher
        object_detect_publisher_ = this->create_publisher<wild_sight_interfaces::msg::ObjectDetect>("object_detect", 10);

	// create a subscriber
        inclinometer_subscription_ = this->create_subscription<wild_sight_interfaces::msg::CameraOrientation>(
		"camera_orientation", 10, 
		std::bind(&GStreamerPipeline::inclinometer_callback, this, std::placeholders::_1)
        );

        // Initialize GStreamer
        gst_init(nullptr, nullptr);

        // Build the pipeline string
	std::string pipeline_str = "rtspsrc location=" + camera_url_ + " ! "
            "rtph265depay ! h265parse ! omxh265dec ! "
            "videoconvert ! video/x-raw, format=NV12 ! "
	    "videorate ! video/x-raw, framerate=30/1 ! "

	    "tee name=t ! "
	       "queue max-size-buffers=2 leaky=0 ! "
	       "vvas_xmultisrc kconfig=\"/opt/xilinx/kr260-wild-sight/share/vvas/detect/preprocess.json\" ! "
	       "video/x-raw,format=BGR,width=640,height=360 ! "
	       "queue max-size-buffers=1 leaky=2 ! "
               "vvas_xinfer infer-config=\"/opt/xilinx/kr260-wild-sight/share/vvas/detect/aiinference.json\" ! "
               "ima.sink_master vvas_xmetaaffixer name=ima ima.src_master ! fakesink "

            "t. ! "
	       "queue max-size-buffers=1 leaky=2 ! ima.sink_slave_0 ima.src_slave_0 ! "
	       "vvas_xmetaconvert config-location=\"/opt/xilinx/kr260-wild-sight/share/vvas/detect/metaconvert.json\" ! "
	       "vvas_xoverlay name=draw ! queue max-size-buffers=2 leaky=2 ! "
               "kmssink driver-name=xlnx plane-id=39 sync=false fullscreen-overlay=true";

	// Convert the pipeline string to const gchar*
    	const gchar *pipeline_cstr = pipeline_str.c_str();

    	// Create the GStreamer pipeline
    	pipeline_ = gst_parse_launch(pipeline_cstr, nullptr);
        if (!pipeline_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to create pipeline");
            rclcpp::shutdown();
            return;
        }

        // Get the draw vvas_filter element
        draw_probe_ = gst_bin_get_by_name(GST_BIN(pipeline_), "draw");
        if (!draw_probe_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get draw element");
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }

        // Attach a draw probe to the sink pad of the draw element
        GstPad *draw_pad = gst_element_get_static_pad(draw_probe_, "sink");
        if (!draw_pad) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get sink pad from draw probe");
            gst_object_unref(draw_probe_);
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }
	
	// Attach  a callback to the probe
        gst_pad_add_probe(draw_pad, GST_PAD_PROBE_TYPE_BUFFER, draw_probe_callback, this, nullptr);
        gst_object_unref(draw_pad);

	// initialize camera orientation structs
	camera_orientation_.azimuth = 0.0;
	camera_orientation_.elevation = 0.0;

        // Start playing
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    }

    ~GStreamerPipeline() {
        // Free resources
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(draw_probe_);
        gst_object_unref(pipeline_);
    }

    void publish_max_bounding_box(const BoundingBox *res_bbox, const BoundingBox *obj_bbox) {
        static unsigned int frame_count = 0;

        // Handle only each N-th frame to reduce latency 
        if ((frame_count++) % 2)
            return;

        auto msg = wild_sight_interfaces::msg::ObjectDetect();

        if (obj_bbox != nullptr) {
            msg.frame_width = res_bbox->width;
            msg.frame_height = res_bbox->height;
            msg.bbox_x = obj_bbox->x;
            msg.bbox_y = obj->y;
            msg.bbox_width = obj_bbox->width;
            msg.bbox_height = obj_bbox->height;
            msg.object_detected = true;
        } else {
            msg.frame_width = 0;
            msg.frame_height = 0;
            msg.bbox_x = 0;
            msg.bbox_y = 0;
            msg.bbox_width = 0;
            msg.bbox_height = 0;
            msg.object_detected = false;
        }

        object_detect_publisher_->publish(msg);
    }

    static void handle_inference_meta(GstMeta *meta, GStreamerPipeline *gsnode) {
	    
	// camera orientation struct to be shared between ros2 node and vvas library
        static CameraOrientation *cam_orient = nullptr;

        if (!cam_orient) {
            cam_orient = new CameraOrientation();
	}
#if 0 
	// Get Inference Metadata
        GstInferenceMeta *inference_meta = reinterpret_cast<GstInferenceMeta *>(meta);

	// Get the parent prediction struct
        if (inference_meta && inference_meta->prediction) {
            GstInferencePrediction *prediction = inference_meta->prediction;

            // Parent bbox holds the resolution of the inference frame
            BoundingBox *parent_bbox = &prediction->bbox;

            // Get the linked list root Node that holds all detected objects (child nodes)
            GNode *predictions = prediction->predictions;

	    // ceck if detected any objects
            if (predictions) {
                GstInferencePrediction *largest_child_pred = nullptr;
                guint max_width = 0;

		// walk through linked list of detected objects
                for (GNode *node = predictions->children; node; node = node->next) {
                    GstInferencePrediction *child_pred = (GstInferencePrediction *) node->data;

		    // find the largest boundary box
                    if (child_pred->bbox.width > max_width) {
                        max_width = child_pred->bbox.width;
                        largest_child_pred = child_pred;

			child_pred->obj_track_label = nullptr;
                    }
                }

		// check if the largest boundary box even exists
                if (largest_child_pred) {
		    // publish the largest one
                    gsnode->publish_max_bounding_box(parent_bbox, &largest_child_pred->bbox);

		    // Also, attach the inclinometer shared data struct to the 
		    // Unused pointer of GstInferencePrediction struct
		    // So, the VVAS Draw Filter element can show camera orientation on the screen
		    largest_child_pred->reserved_1 = cam_orient;

		    {   // update shared structure with inclinometer data with guard lock
		        std::lock_guard<std::mutex> lock(gsnode->mutex_);

			cam_orient->azimuth = gsnode->camera_orientation_.azimuth;
			cam_orient->elevation = gsnode->camera_orientation_.elevation;
		    }

                    return;
                }
            }
        }
#endif

	// no objects detected, but we count empty frames for timeout too
        gsnode->publish_max_bounding_box(nullptr, nullptr);
    }

    // The Attached GStreamer Element Probe Callback
    static GstPadProbeReturn draw_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
        if (!pad) {
            return GST_PAD_PROBE_OK;
        }

	// Get the Class Node object
        GStreamerPipeline *node = reinterpret_cast<GStreamerPipeline *>(user_data);

	// Get the buffer
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        if (!buffer) {
            node->publish_max_bounding_box(nullptr, nullptr);
            return GST_PAD_PROBE_OK;
        }

	// Look for VVAS Inference Meta frames only
        static GType api_type = g_type_from_name("GstVvasInferenceMetaAPI");
        GstMeta *meta = gst_buffer_get_meta(buffer, api_type);

        if (meta) {
	    // found one, let's handle it
            handle_inference_meta(meta, node);
        } else {
	    // Needed for counting empty frames for timeout
            node->publish_max_bounding_box(nullptr, nullptr);
        }

        return GST_PAD_PROBE_OK;
    }

    // Inclinometer subscription callback
    void inclinometer_callback(const wild_sight_interfaces::msg::CameraOrientation::SharedPtr msg) {

	{   // received data from subscriber, save them with a lock guard
            std::lock_guard<std::mutex> lock(this->mutex_);

            this->camera_orientation_.azimuth = msg->azimuth;
            this->camera_orientation_.elevation = msg->elevation;
	}
    }

private:
    std::mutex mutex_;		// mutex for guarding between ROS2 node and VVAS library threads

    GstElement *pipeline_;	// GStreamer Pipeline
    GstElement *draw_probe_;	// GStreamer Probe on the Draw VVAS Filter element

    CameraOrientation camera_orientation_;	// Stores cam orientation received by ROS2 subscriber

    // publisher for detected object coordinates
    rclcpp::Publisher<wild_sight_interfaces::msg::ObjectDetect>::SharedPtr object_detect_publisher_;

    // subscriber for receiving Camera's Inclinometer data
    rclcpp::Subscription<wild_sight_interfaces::msg::CameraOrientation>::SharedPtr inclinometer_subscription_;
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<GStreamerPipeline>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
