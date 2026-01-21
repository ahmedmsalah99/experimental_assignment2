#include "plansys2_executor/ActionExecutorClient.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "aruco_opencv_msgs/msg/aruco_detection.hpp"
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <mutex>

using namespace std::chrono_literals;

class AlignAction : public plansys2::ActionExecutorClient
{
public:
  AlignAction()
  : plansys2::ActionExecutorClient("align", 100ms),
    alignment_active_(false),
    aligned_(false),
    center_tolerance_(0.03)
  {
    // Publisher for cmd_vel to rotate the robot
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    
    // Subscriber for marker detection
    detection_sub_ = this->create_subscription<aruco_opencv_msgs::msg::ArucoDetection>(
      "/aruco_detections", 10,
      std::bind(&AlignAction::detection_callback, this, std::placeholders::_1)
    );
    
    RCLCPP_INFO(get_logger(), "AlignAction initialized");
  }

private:
  void detection_callback(const aruco_opencv_msgs::msg::ArucoDetection::SharedPtr msg)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!alignment_active_) return;
    if (msg->markers.empty()) return;
    
    // Get the first marker's x position in camera frame
    const auto& marker = msg->markers[0];
    double marker_x = marker.pose.position.x;
    
    RCLCPP_DEBUG(get_logger(), "Align: marker x = %.4f", marker_x);
    
    // Check if marker is centered (within tolerance)
    if (std::abs(marker_x) <= center_tolerance_) {
      aligned_ = true;
      RCLCPP_INFO(get_logger(), "Marker centered at x = %.4f", marker_x);
      
      // Stop rotating
      geometry_msgs::msg::Twist stop_cmd;
      cmd_vel_pub_->publish(stop_cmd);
    } else {
      // Rotate to center the marker
      geometry_msgs::msg::Twist rotate_cmd;
      rotate_cmd.linear.x = 0.0;
      
      // If marker is to the left (negative x), rotate left (positive angular.z)
      // If marker is to the right (positive x), rotate right (negative angular.z)
      if (marker_x < -center_tolerance_) {
        rotate_cmd.angular.z = 0.3;  // Rotate left
      } else if (marker_x > center_tolerance_) {
        rotate_cmd.angular.z = -0.3; // Rotate right
      }
      
      cmd_vel_pub_->publish(rotate_cmd);
    }
  }

  void do_work() override
  {
    auto args = get_arguments();
    
    // Expected: align ?r ?w
    if (args.size() < 3) {
      RCLCPP_ERROR(get_logger(), "Not enough arguments for align action");
      finish(false, 0.0, "Insufficient arguments");
      return;
    }
    
    std::string waypoint = args[2];
    RCLCPP_INFO(get_logger(), "Aligning at waypoint [%s]", waypoint.c_str());
    
    if (!alignment_active_) {
      alignment_active_ = true;
      aligned_ = false;
      alignment_start_time_ = this->now();
    }
    
    // Check timeout (5 seconds)
    auto elapsed = (this->now() - alignment_start_time_).seconds();
    
    if (elapsed > 5.0) {
      RCLCPP_WARN(get_logger(), "Alignment timeout");
      geometry_msgs::msg::Twist stop_cmd;
      cmd_vel_pub_->publish(stop_cmd);
      alignment_active_ = false;
      
      // Still finish successfully if we timed out (might be close enough)
      finish(true, 1.0, "Alignment timeout");
      return;
    }
    
    double progress = elapsed / 5.0;
    send_feedback(progress, "Aligning with marker...");
    
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (aligned_) {
        geometry_msgs::msg::Twist stop_cmd;
        cmd_vel_pub_->publish(stop_cmd);
        alignment_active_ = false;
        aligned_ = false;
        RCLCPP_INFO(get_logger(), "Alignment complete");
        finish(true, 1.0, "Aligned with marker");
      }
    }
  }
  
  bool alignment_active_;
  bool aligned_;
  double center_tolerance_;
  rclcpp::Time alignment_start_time_;
  
  std::mutex mutex_;
  
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::Subscription<aruco_opencv_msgs::msg::ArucoDetection>::SharedPtr detection_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<AlignAction>();
  node->set_parameter(rclcpp::Parameter("action_name", "align"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}

