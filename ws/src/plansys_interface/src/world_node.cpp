#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "plansys2_interface/srv/get_marker_pose.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>
#include <string>

struct MarkerInfo {
  int marker_id;
  geometry_msgs::msg::Pose robot_pose;
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
    info.robot_pose = req->pose;
    markers_[marker_id] = info;
    
    RCLCPP_INFO(get_logger(), "Added marker ID %d", marker_id);
    res->success = true;
    res->pose = req->pose;
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
    res->pose = markers_[nth_id].robot_pose;
    RCLCPP_INFO(get_logger(), "Nth marker (n=%d): ID %d", n, nth_id);
  }
  
  // Store detected markers: marker_id -> MarkerInfo
  std::unordered_map<int, MarkerInfo> markers_;
  
  rclcpp::Service<plansys2_interface::srv::GetMarkerPose>::SharedPtr add_marker_srv_;
  rclcpp::Service<plansys2_interface::srv::GetMarkerPose>::SharedPtr get_nth_marker_srv_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<WorldNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}

