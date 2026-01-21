#include "plansys2_executor/ActionExecutorClient.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "aruco_opencv_msgs/msg/aruco_detection.hpp"
#include "plansys2_interface/srv/get_marker_pose.hpp"
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <mutex>

using namespace std::chrono_literals;

class RotateAndDetectAction : public plansys2::ActionExecutorClient
{
public:
  RotateAndDetectAction()
  : plansys2::ActionExecutorClient("rotateanddetect", 100ms),
    rotation_active_(false),
    marker_detected_(false),
    current_marker_id_(-1),
    detection_start_time_()
  {
    // Publisher for cmd_vel to rotate the robot
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    
    // Subscriber for odometry to get robot pose
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "/odom", 10,
      std::bind(&RotateAndDetectAction::odom_callback, this, std::placeholders::_1)
    );
    
    // Subscriber for marker detection
    detection_sub_ = this->create_subscription<aruco_opencv_msgs::msg::ArucoDetection>(
      "/aruco_detections", 10,
      std::bind(&RotateAndDetectAction::detection_callback, this, std::placeholders::_1)
    );
    
    // Service client to register markers in world node
    world_client_ = this->create_client<plansys2_interface::srv::GetMarkerPose>(
      "/world_node/add_marker"
    );
    
    RCLCPP_INFO(get_logger(), "RotateAndDetectAction initialized");
  }

private:
  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    // Store the robot's current pose when marker is detected
    robot_pose_ = msg->pose.pose;
  }
  
  void detection_callback(const aruco_opencv_msgs::msg::ArucoDetection::SharedPtr msg)
  {
    
    if (!rotation_active_) return;
    if (msg->markers.empty()) return;
    
    // Take the first detected marker
    const auto& marker = msg->markers[0];
    RCLCPP_INFO(get_logger(), "Detected marker ID: %d", marker.marker_id);
    
    marker_detected_ = true;
    current_marker_id_ = marker.marker_id;
    // Store the robot pose (orientation) when marker is detected
    // The robot_pose_ is updated via odom_callback
    detected_pose_ = robot_pose_;
    
    // Stop rotating
    geometry_msgs::msg::Twist stop_cmd;
    cmd_vel_pub_->publish(stop_cmd);
  }

  void do_work() override
  {
    auto args = get_arguments();
    
    // Expected: rotate-and-detect ?r ?w ?m
    if (args.size() < 3) {
      RCLCPP_ERROR(get_logger(), "Not enough arguments");
      finish(false, 0.0, "Insufficient arguments");
      return;
    }
    
    current_wp_ = args[1];
    std::string marker_name = args[2];
    
    RCLCPP_INFO(get_logger(), "Rotate-and-detect at [%s] for [%s]", current_wp_.c_str(), marker_name.c_str());
    
    if (!rotation_active_ && !marker_detected_) {
      rotation_active_ = true;
      marker_detected_ = false;
      detection_start_time_ = this->now();
      
      // Start rotating
      geometry_msgs::msg::Twist rotate_cmd;
      rotate_cmd.linear.x = 0.0;
      rotate_cmd.angular.z = 0.5;
      cmd_vel_pub_->publish(rotate_cmd);
      RCLCPP_INFO(get_logger(), "Started rotating to detect marker...");
    }
    
    // Check timeout (10 seconds)
    auto elapsed = (this->now() - detection_start_time_).seconds();
    
    if (elapsed > 60) {
      RCLCPP_WARN(get_logger(), "Detection timeout");
      geometry_msgs::msg::Twist stop_cmd;
      cmd_vel_pub_->publish(stop_cmd);
      rotation_active_ = false;
      marker_detected_ = false;
      finish(false, 0.0, "Marker not detected");
      return;
    }
    
    double progress = elapsed / 60.0;
    send_feedback(progress, "Rotating to detect...");
    
    {
      if (marker_detected_) {
        geometry_msgs::msg::Twist stop_cmd;
        cmd_vel_pub_->publish(stop_cmd);
        rotation_active_ = false;
        marker_detected_ = false;
        
        // Register marker in world node
        register_marker(current_marker_id_);
        
        RCLCPP_INFO(get_logger(), "Marker %d detected and registered", current_marker_id_);
        finish(true, 1.0, "Marker detected");
      }
    }
  }
  
  bool register_marker(int marker_id)
  {
    auto request = std::make_shared<plansys2_interface::srv::GetMarkerPose::Request>();
    request->marker_id = marker_id;
    request->wp = current_wp_;
    
    if (!world_client_->wait_for_service(1s)) {
      RCLCPP_WARN(get_logger(), "World node service not available");
      return false;
    }
    
    auto future = world_client_->async_send_request(request);
    return true;
    // auto result = future.get();
    
    // if (result->success) {
    //   RCLCPP_INFO(get_logger(), "Registered marker %d in world node", marker_id);
    //   return true;
    // } else {
    //   RCLCPP_ERROR(get_logger(), "Failed to register marker %d", marker_id);
    //   return false;
    // }
  }
  
  bool rotation_active_;
  bool marker_detected_;
  int current_marker_id_;
  rclcpp::Time detection_start_time_;
  geometry_msgs::msg::Pose detected_pose_;
  geometry_msgs::msg::Pose robot_pose_;
  std::mutex mutex_;
  std::string current_wp_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<aruco_opencv_msgs::msg::ArucoDetection>::SharedPtr detection_sub_;
  rclcpp::Client<plansys2_interface::srv::GetMarkerPose>::SharedPtr world_client_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RotateAndDetectAction>();
  node->set_parameter(rclcpp::Parameter("action_name", "rotateanddetect"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}

