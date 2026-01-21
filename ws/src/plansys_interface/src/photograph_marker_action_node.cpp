#include "plansys2_executor/ActionExecutorClient.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "aruco_opencv_msgs/msg/aruco_detection.hpp"
#include "cv_bridge/cv_bridge.hpp"
#include "opencv2/opencv.hpp"
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>

using namespace std::chrono_literals;

class PhotographMarkerAction : public plansys2::ActionExecutorClient
{
public:
  PhotographMarkerAction()
  : plansys2::ActionExecutorClient("photographmarker", 100ms),
    waiting_for_photo_(true),
    photo_taken_(false)
  {

    
    // Subscribe to detection topic to get marker info
    detection_sub_ = this->create_subscription<aruco_opencv_msgs::msg::ArucoDetection>(
      "/aruco_detections", 10,
      [this](const aruco_opencv_msgs::msg::ArucoDetection::SharedPtr msg) {
        latest_detection_ = msg;
      }
    );
    
    image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "/camera/image", 10,
      [this](const sensor_msgs::msg::Image::SharedPtr msg) {
        if (!waiting_for_photo_) return;
        take_photo(msg);
      }
    );
    
    fx_ = 381.3611602783203;
    fy_ = 381.3611602783203;
    cx_ = 320.0;
    cy_ = 240.0;
    marker_size_ = 0.0742;
    photo_start_ = this->now();
    RCLCPP_INFO(get_logger(), "PhotographMarkerAction ready");
  }

private:
  std::tuple<int, int, int> project_marker(const geometry_msgs::msg::Point& pos, double size)
  {
    double x = pos.x, y = pos.y, z = pos.z;
    if (z <= 0) return {-1, -1, 0};
    
    double h = size / 2.0;
    std::vector<std::tuple<double, double, double>> corners = {
      {-h, -h, 0}, {h, -h, 0}, {h, h, 0}, {-h, h, 0}
    };
    
    int cu = static_cast<int>(fx_ * x / z + cx_);
    int cv = static_cast<int>(fy_ * y / z + cy_);
    
    int max_r = 0;
    for (auto [cx, cy, cz] : corners) {
      double X = x + cx, Y = y + cy, Z = (z + cz > 0) ? z + cz : z;
      int u = static_cast<int>(fx_ * X / Z + cx_);
      int v = static_cast<int>(fy_ * Y / Z + cy_);
      max_r = std::max(max_r, static_cast<int>(std::hypot(u - cu, v - cv)));
    }
    return {cu, cv, max_r};
  }

  void take_photo(const sensor_msgs::msg::Image::SharedPtr& msg)
  {

    try {
      cv::Mat frame = cv_bridge::toCvShare(msg, "bgr8")->image.clone();
      
      // Get marker info from latest detection
      if (latest_detection_ && !latest_detection_->markers.empty()) {
        const auto& marker = latest_detection_->markers[0];
        int id = marker.marker_id;
        auto [cx, cy, r] = project_marker(marker.pose.position, marker_size_);
        
        if (cx >= 0 && cx < frame.cols && cy >= 0 && cy < frame.rows) {
          cv::circle(frame, cv::Point(cx, cy), r, cv::Scalar(0, 255, 0), 3);
          cv::putText(frame, "ID " + std::to_string(id),
                      cv::Point(cx + r + 5, cy),
                      cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
          
          std::filesystem::create_directories("/home/assignments/assignment3/ws/src/plansys_interface/images");
          std::string filename = "/home/assignments/assignment3/ws/src/plansys_interface/images/" + 
                                 std::to_string(id) + ".png";
          cv::imwrite(filename, frame);
          RCLCPP_INFO(get_logger(), "Saved: %s", filename.c_str());
          photo_taken_ = true;
        }
      } else {
        return;
      }
      waiting_for_photo_ = false;
    } catch (cv_bridge::Exception& e) {
      RCLCPP_ERROR(get_logger(), "CV Bridge: %s", e.what());
    }
  }

  void do_work() override
  {
    auto args = get_arguments();
    if (args.size() < 4) {
      finish(false, 0.0, "Insufficient arguments");
      return;
    }
    if(!waiting_for_photo_){
      photo_start_ = this->now();
    }
    std::string marker_name = args[2];
    RCLCPP_INFO(get_logger(), "Photograph [%s]", marker_name.c_str());
    
    waiting_for_photo_ = true;
    if (waiting_for_photo_) {
      auto elapsed = (this->now() -  photo_start_).seconds();
      send_feedback(elapsed / 60.0, "Taking photo...");
      
      if (photo_taken_) {
        photo_taken_ = false;
        waiting_for_photo_ = false;
        finish(true, 1.0, "Photo saved");
      }
      
      if (elapsed > 60.0) {
        waiting_for_photo_ = false;
        finish(true, 1.0, "Photo timeout");
      }
    }
    
  }
  
  
  bool waiting_for_photo_, photo_taken_;
  rclcpp::Time photo_start_;
  geometry_msgs::msg::Pose start_pose_, current_pose_;
  aruco_opencv_msgs::msg::ArucoDetection::SharedPtr latest_detection_;
  double fx_, fy_, cx_, cy_, marker_size_;
  
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<aruco_opencv_msgs::msg::ArucoDetection>::SharedPtr detection_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<PhotographMarkerAction>();
  node->set_parameter(rclcpp::Parameter("action_name", "photographmarker"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}

