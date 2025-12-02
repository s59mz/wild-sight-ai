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
#     https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa
*/


#define VVAS_GLIB_UTILS 1
#include <glib.h>
#include <gst/gst.h>
#include <gst/vvas/gstinferencemeta.h>
#include <gst/video/video.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <vvas_utils/vvas_node.h>
#include <vvas_core/vvas_infer_prediction.h>
#include <rclcpp/rclcpp.hpp>

#include "wild_sight_interfaces/msg/object_detect.hpp"
#include "wild_sight_interfaces/msg/camera_orientation.hpp"
#include "wild_sight_interfaces/msg/take_snapshot.hpp"
#include "wild_sight_pkg/cameradata.h"

// tensor debug
#include <vart/runner.hpp>
#include <vart/tensor_buffer.hpp>
#include <xir/tensor/tensor.hpp>
#include <xir/graph/graph.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "handle_tensorbuf.cpp"

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

    takesnapshot_subscription_ = this->create_subscription<wild_sight_interfaces::msg::TakeSnapshot>(
		"take_snapshot", 10, 
		std::bind(&GStreamerPipeline::takesnapshot_callback, this, std::placeholders::_1)
    );

    // Initialize GStreamer
    // Initialize GStreamer
    gst_init(nullptr, nullptr);

    // Build the pipeline string
	std::string pipeline_str = "rtspsrc location=" + camera_url_ + " ! "
        "rtph265depay ! h265parse ! omxh265dec ! "
        "videoconvert ! video/x-raw,format=NV12,width=1920,height=1080 ! "
	    "videorate ! video/x-raw, framerate=30/1 ! "
	    "queue max-size-buffers=1 leaky=2 ! "

	    "tee name=t ! "
	       "vvas_xmultisrc kconfig=\"/opt/xilinx/kr260-wild-sight/share/vvas/objectdetect/preprocess.json\" ! "
	       "video/x-raw,format=RGB,width=448,height=256 ! "
           "vvas_xinfer name=infer infer-config=\"/opt/xilinx/kr260-wild-sight/share/vvas/objectdetect/aiinference.json\" ! "
           "ima.sink_master vvas_xmetaaffixer timeout=2000 sync=true name=ima ima.src_master ! fakesink "

         "t. ! "
	       "ima.sink_slave_0 ima.src_slave_0 ! "
           "vvas_xfilter name=draw kernels-config=\"/opt/xilinx/kr260-wild-sight/share/vvas/objectdetect/drawresult.json\" ! "
           "queue max-size-buffers=1 leaky=2 ! "
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

        //
        // Get the  vvas_xinfer infer element
        //
        probe_infer_element_ = gst_bin_get_by_name(GST_BIN(pipeline_), "infer");
        if (!probe_infer_element_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get infer element");
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }

        // Attach a probe to the source pad of the infer element
        GstPad *probe_infer_pad = gst_element_get_static_pad(probe_infer_element_, "src");
        if (!probe_infer_pad) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get pad from infer element");
            gst_object_unref(probe_infer_element_);
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }
	
	    // Attach  a callback to the probe
        gst_pad_add_probe(probe_infer_pad, GST_PAD_PROBE_TYPE_BUFFER, probe_infer_callback, this, nullptr);
        gst_object_unref(probe_infer_pad);

        //
        // Get the  vvas_xfilter draw element
        //
        probe_draw_element_ = gst_bin_get_by_name(GST_BIN(pipeline_), "draw");
        if (!probe_draw_element_) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get draw element");
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }

        // Attach a probe to the sink pad of the draw element
        GstPad *probe_draw_pad = gst_element_get_static_pad(probe_draw_element_, "sink");
        if (!probe_draw_pad) {
            RCLCPP_ERROR(this->get_logger(), "Failed to get pad from draw element");
            gst_object_unref(probe_draw_element_);
            gst_object_unref(pipeline_);
            rclcpp::shutdown();
            return;
        }
	
	    // Attach  a callback to the probe
        gst_pad_add_probe(probe_draw_pad, GST_PAD_PROBE_TYPE_BUFFER, probe_draw_callback, this, nullptr);
        gst_object_unref(probe_draw_pad);

	    // initialize camera orientation structs
	    camera_orientation_.azimuth = 0.0;
	    camera_orientation_.elevation = 0.0;

        // Start playing
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);

        // disable taking snapshots for now
        should_take_snapshot_ = false;
    }

    ~GStreamerPipeline() {
        // Free resources
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(probe_infer_element_);
        gst_object_unref(probe_draw_element_);
        gst_object_unref(pipeline_);
    }

    void publish_max_bounding_box(const VvasBoundingBox *res_bbox, const VvasBoundingBox *obj_bbox, int conflict_det) {
        //static unsigned int frame_count = 0;

        // Handle only each N-th frame to reduce latency 
        //if ((frame_count++) % 2)
        //    return;

        auto msg = wild_sight_interfaces::msg::ObjectDetect();

        if (obj_bbox != nullptr) {
            msg.frame_width = res_bbox->width;
            msg.frame_height = res_bbox->height;
            msg.bbox_x = obj_bbox->x;
            msg.bbox_y = obj_bbox->y;
            msg.bbox_width = obj_bbox->width;
            msg.bbox_height = obj_bbox->height;
            msg.animal_detected = true;
            msg.conflict_detected = conflict_det;
        } else {
            msg.frame_width = 0;
            msg.frame_height = 0;
            msg.bbox_x = 0;
            msg.bbox_y = 0;
            msg.bbox_width = 0;
            msg.bbox_height = 0;
            msg.animal_detected = false;
            msg.conflict_detected = false;
        }

        object_detect_publisher_->publish(msg);
    }

    // handle the received raw tensors from the infer element
    static void handle_inference_meta_infer(GstInferenceMeta *inference_meta) {
	    // Get the parent prediction struct
        if (inference_meta && inference_meta->prediction) {
            GstInferencePrediction *predictions = inference_meta->prediction;

	        // ceck if detected any objects
            if (predictions) {
                // get root element of linked list
                VvasInferPrediction *prediction = & predictions->prediction;
                VvasTreeNode *root = prediction->node;

                // get parent bbox
                VvasBoundingBox * parent_bbox = &prediction->bbox;

		        // walk through linked list of detected objects
                if (root) {
                    for (VvasTreeNode* child = root->children; child != nullptr; child = child->next) {

                        GstInferencePrediction* gpred = (GstInferencePrediction*) child->data;
                        VvasInferPrediction *child_pred = &gpred->prediction; 
                        
                        if (child_pred) {
                            // get tensor buffer from child node
                            TensorBuf *tb = child_pred->tb;

                            if (tb) {
                                // decode raw yolov5 tensors
                                GstInferencePrediction *root_pred = nullptr;
                                handle_tensorbuf(tb, &root_pred, parent_bbox->width, parent_bbox->height);

                                if (root_pred) {
                                    // attach from the raw tensor decoded list of detected objects
                                    gst_inference_prediction_unref(inference_meta->prediction);
                                    inference_meta->prediction = root_pred;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    static void handle_inference_meta_draw(GstInferenceMeta *inference_meta, GStreamerPipeline *gsnode) {
	    // camera orientation struct to be shared between ros2 node and vvas library
        static CameraOrientation *cam_orient = nullptr;

        if (!cam_orient) {
            cam_orient = new CameraOrientation();
	    }

	    // Get the parent prediction struct
        if (inference_meta && inference_meta->prediction) {
            GstInferencePrediction *predictions = inference_meta->prediction;

	        // ceck if detected any objects
            if (predictions) {
                GstInferencePrediction *largest_child_pred = nullptr;

                guint max_width = 0;
                int animal_det = 0;
                int person_det = 0;
                int conflict_det = 0;

                // get root element of linked list
                VvasInferPrediction *prediction = & predictions->prediction;
                VvasTreeNode *root = prediction->node;

                // get parent bbox
                VvasBoundingBox * parent_bbox = &prediction->bbox;

		        // walk through linked list of detected objects
                if (root) {
                    for (VvasTreeNode* child = root->children; child != nullptr; child = child->next) {

                        GstInferencePrediction* gpred = (GstInferencePrediction*) child->data;
                        VvasInferPrediction *child_pred = &gpred->prediction; 
                        
                        if (child_pred) {
                            // read class id
                            VvasList *classes = child_pred->classifications;
                            VvasInferClassification *classification = &((GstInferenceClassification *) classes->data)->classification;
                            int class_id = classification->class_id;

                            if (class_id == 0) {    // animal detected
                                animal_det++;

                                // find the largest boundary box of detected animal, track animals only
                                if (child_pred->bbox.width > max_width) {
                                    max_width = child_pred->bbox.width;
                                    largest_child_pred = gpred;
                                }
                            }

                            if (class_id == 1) person_det++;    // person detected
                        }
                    }
                }

		        // check if the largest boundary box even exists
                if (largest_child_pred) {
                    // check if human-wildlife conflict is detected
                    if (animal_det && person_det) conflict_det = true;

		            // publish the largest one
                    gsnode->publish_max_bounding_box(parent_bbox, &largest_child_pred->prediction.bbox, conflict_det);


                    // Also, attach the inclinometer shared data struct to the 
                    // Unused pointer of GstInferencePrediction struct
                    // So, the VVAS Draw Filter element can show camera orientation on the screen

                    largest_child_pred->reserved_1 = cam_orient;

                    {   // update shared structure with inclinometer data with guard lock
                        std::lock_guard<std::mutex> lock(gsnode->mutex_);

                        cam_orient->azimuth = gsnode->camera_orientation_.azimuth;
                        cam_orient->elevation = gsnode->camera_orientation_.elevation;
                        cam_orient->conflict = conflict_det;
                    }

                    return;
                }
            }
        }

	    // no objects detected, but we count empty frames for timeout too
        gsnode->publish_max_bounding_box(nullptr, nullptr, false);
    }

    // The Attached GStreamer Infer Element Probe Callback
    static GstPadProbeReturn probe_infer_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
        static int cnt =0;
        cnt ++;
        if (!pad || !user_data) return GST_PAD_PROBE_OK;
        if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;

        GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
        if (!buf) {
            return GST_PAD_PROBE_OK;
        }

        // Get Gst inference meta from buffer
        GstMeta *meta = gst_buffer_get_meta(buf, GST_INFERENCE_META_API_TYPE);
        if (meta) {
	        // found one, let's handle it
            GstInferenceMeta *infer_meta = reinterpret_cast<GstInferenceMeta *>(meta);
            handle_inference_meta_infer(infer_meta);
        }

        return GST_PAD_PROBE_OK;
    }

    // The Attached GStreamer Draw Element Probe Callback
    static GstPadProbeReturn probe_draw_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
        static int cnt =0;
        bool take_snapshot = false;

        cnt ++;
        if (!pad || !user_data) return GST_PAD_PROBE_OK;
        if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;

	    // Get the Class Node object
        GStreamerPipeline *node = reinterpret_cast<GStreamerPipeline *>(user_data);

        GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
        if (!buf) {
	        // Needed for counting empty frames for timeout
            node->publish_max_bounding_box(nullptr, nullptr, false);
            return GST_PAD_PROBE_OK;
        }

        // Get Gst inference meta from buffer
        GstMeta *meta = gst_buffer_get_meta(buf, GST_INFERENCE_META_API_TYPE);
        if (meta) {
	        // found one, let's handle it
            GstInferenceMeta *infer_meta = reinterpret_cast<GstInferenceMeta *>(meta);
            handle_inference_meta_draw(infer_meta, node);
        } else {
	        // Needed for counting empty frames for timeout
            node->publish_max_bounding_box(nullptr, nullptr, false);
        }

        // check the snapshot request flag
        { 
                std::lock_guard<std::mutex> lock(node->mutex_);

                take_snapshot = node->should_take_snapshot_;
                node->should_take_snapshot_ = false; // reset flag
        }

        // Take a video snapshot, if requested
        if (take_snapshot) {
            // Get caps from pad to decode width/height/format
            GstCaps *caps = gst_pad_get_current_caps(pad);
            if (!caps) {
                RCLCPP_ERROR(node->get_logger(), "Snapshot probe: no caps available");
                return GST_PAD_PROBE_OK;
            }

            GstVideoInfo vinfo;
            if (!gst_video_info_from_caps(&vinfo, caps)) {
                RCLCPP_ERROR(node->get_logger(), "Snapshot probe: failed to read GstVideoInfo");
                gst_caps_unref(caps);
                return GST_PAD_PROBE_OK;
            }
            gst_caps_unref(caps);

            // Map the buffer into GstVideoFrame to access planes & stride safely
            GstVideoFrame vframe;
            if (!gst_video_frame_map(&vframe, &vinfo, buf, GST_MAP_READ)) {
                RCLCPP_ERROR(node->get_logger(), "Snapshot probe: gst_video_frame_map() failed");
                return GST_PAD_PROBE_OK;
            }

            RCLCPP_INFO(node->get_logger(),
                        "Snapshot triggered: format=%s %dx%d planes=%d",
                        gst_video_format_to_string(GST_VIDEO_FRAME_FORMAT(&vframe)),
                        GST_VIDEO_FRAME_WIDTH(&vframe),
                        GST_VIDEO_FRAME_HEIGHT(&vframe),
                        GST_VIDEO_FRAME_N_PLANES(&vframe));

            // Do the actual conversion + JPEG save
            node->convert_and_save_jpeg(vframe);

            // Unmap when done
            gst_video_frame_unmap(&vframe);
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

    // take snapshot subscription callback
    void takesnapshot_callback(const wild_sight_interfaces::msg::TakeSnapshot::SharedPtr msg) {

        {   // received data from subscriber, save them with a lock guard
                std::lock_guard<std::mutex> lock(this->mutex_);

                this->should_take_snapshot_ = msg->take_snapshot;
        }
    }

    void convert_and_save_jpeg(GstVideoFrame &vframe)
    {
        int width  = GST_VIDEO_FRAME_WIDTH(&vframe);
        int height = GST_VIDEO_FRAME_HEIGHT(&vframe);

        uint8_t *y_plane  = static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 0));
        uint8_t *uv_plane = static_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&vframe, 1));

        int y_stride  = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 0);
        int uv_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&vframe, 1);

        RCLCPP_INFO(this->get_logger(),
                    "Converting NV12 snapshot: %dx%d y_stride=%d uv_stride=%d",
                    width, height, y_stride, uv_stride);

        cv::Mat y(height, width, CV_8UC1, y_plane, y_stride);
        cv::Mat uv(height / 2, width / 2, CV_8UC2, uv_plane, uv_stride);

        cv::Mat bgr;
        cv::cvtColorTwoPlane(y, uv, bgr, cv::COLOR_YUV2BGR_NV12);

        std::string filename =
            "/tmp/snapshot_" + std::to_string(this->now().nanoseconds()) + ".jpg";

        if (cv::imwrite(filename, bgr)) {
            RCLCPP_INFO(this->get_logger(), "Snapshot saved to %s", filename.c_str());
        } else {
            RCLCPP_ERROR(this->get_logger(), "Failed to save snapshot");
        }
    }

private:
    std::mutex mutex_;		// mutex for guarding between ROS2 node and VVAS library threads

    GstElement *pipeline_;	                // GStreamer Pipeline
    GstElement *probe_infer_element_;	    // GStreamer infer element to be probed
    GstElement *probe_draw_element_;	    // GStreamer draw element to be probed

    CameraOrientation camera_orientation_;	// Stores cam orientation received by ROS2 subscriber

    // publisher for detected object coordinates
    rclcpp::Publisher<wild_sight_interfaces::msg::ObjectDetect>::SharedPtr object_detect_publisher_;

    // subscriber for receiving Camera's Inclinometer data
    rclcpp::Subscription<wild_sight_interfaces::msg::CameraOrientation>::SharedPtr inclinometer_subscription_;

    // subscriber for receiving a take snapshot request
    rclcpp::Subscription<wild_sight_interfaces::msg::TakeSnapshot>::SharedPtr takesnapshot_subscription_;

    bool should_take_snapshot_;        // take a sapshot request flag 
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<GStreamerPipeline>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
