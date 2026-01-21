#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "plansys2_interface/srv/get_marker_pose.hpp"
#include "plansys2_interface/msg/detected_markers.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>
#include <string>
#include <chrono>

using namespace std::chrono_literals;

struct MarkerInfo {
  int marker_id;
  std::string robot_wp;
};

class WorldNode : public rclcpp::Node
{
public:
  WorldNode()
  : Node("world_node")
  {
    // Service to add a detected marker
    add_marker_srv_ = this->create_service<plansys2_interface::srv::GetMarkerPose>(
      "/world_node/add_marker",
      std::bind(&WorldNode::add_marker_callback, this,
                std::placeholders::_1, std::placeholders::_2)
    );
    
    // Service to get nth lowest marker by ID
    get_nth_marker_srv_ = this->create_service<plansys2_interface::srv::GetMarkerPose>(
      "/world_node/get_nth_marker",
      std::bind(&WorldNode::get_nth_marker_callback, this,
                std::placeholders::_1, std::placeholders::_2)
    );
    
    // Publisher for all detected markers
    detected_markers_pub_ = this->create_publisher<plansys2_interface::msg::DetectedMarkers>(
      "/world_node/detected_markers", 10);
    
    // Timer for periodic publishing
    timer_ = this->create_wall_timer(500ms, std::bind(&WorldNode::publish_markers_timer, this));
    
    RCLCPP_INFO(get_logger(), "WorldNode initialized");
  }

private:
  void add_marker_callback(
    const std::shared_ptr<plansys2_interface::srv::GetMarkerPose::Request> req,
    std::shared_ptr<plansys2_interface::srv::GetMarkerPose::Response> res)
  {
    int marker_id = req->marker_id;
    
    // Store marker info
    MarkerInfo info;
    info.marker_id = marker_id;
    info.robot_wp = req->wp;
    markers_[marker_id] = info;
    
    RCLCPP_INFO(get_logger(), "Added marker ID %d", marker_id);
    res->success = true;
    res->wp = req->wp;
  }
  
  void get_nth_marker_callback(
    const std::shared_ptr<plansys2_interface::srv::GetMarkerPose::Request> req,
    std::shared_ptr<plansys2_interface::srv::GetMarkerPose::Response> res)
  {
    int n = req->marker_id;  // Use marker_id field as n (nth lowest)
    
    if (markers_.empty()) {
      res->success = false;
      RCLCPP_WARN(get_logger(), "No markers detected");
      return;
    }
    
    // Create sorted vector of marker IDs
    std::vector<int> sorted_ids;
    for (const auto& [id, info] : markers_) {
      sorted_ids.push_back(id);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());
    
    if (n < 0 || n >= static_cast<int>(sorted_ids.size())) {
      res->success = false;
      RCLCPP_WARN(get_logger(), "Invalid index %d, have %zu markers", n, sorted_ids.size());
      return;
    }
    
    int nth_id = sorted_ids[n];
    res->success = true;
    res->wp = markers_[nth_id].robot_wp;
    RCLCPP_INFO(get_logger(), "Nth marker (n=%d): ID %d", n, nth_id);
  }
  
  void publish_markers_timer()
  {
    plansys2_interface::msg::DetectedMarkers msg;
    
    // Create sorted vector of marker IDs
    std::vector<int> sorted_ids;
    for (const auto& [id, info] : markers_) {
      sorted_ids.push_back(id);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());
    
    // Add markers in sorted order
    for (const int& id : sorted_ids) {
      plansys2_interface::msg::DetectedMarker marker;
      marker.marker_id = markers_[id].marker_id;
      marker.wp = markers_[id].robot_wp;
      msg.markers.push_back(marker);
    }
    
    detected_markers_pub_->publish(msg);
  }

  
  // Store detected markers: marker_id -> MarkerInfo
  std::unordered_map<int, MarkerInfo> markers_;
  
  rclcpp::Service<plansys2_interface::srv::GetMarkerPose>::SharedPtr add_marker_srv_;
  rclcpp::Service<plansys2_interface::srv::GetMarkerPose>::SharedPtr get_nth_marker_srv_;
  rclcpp::Publisher<plansys2_interface::msg::DetectedMarkers>::SharedPtr detected_markers_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<WorldNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}

