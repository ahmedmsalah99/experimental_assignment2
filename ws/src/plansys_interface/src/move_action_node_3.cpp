#include "plansys2_executor/ActionExecutorClient.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "lifecycle_msgs/msg/transition.hpp"
#include "plansys2_interface/msg/detected_markers.hpp"
#include <memory>
#include <chrono>
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace std::chrono_literals;

class MoveAction : public plansys2::ActionExecutorClient
{
public:
  MoveAction()
  : plansys2::ActionExecutorClient("move", 500ms), 
    goal_sent_(false), 
    progress_(0.0),
    visited_waypoints_(0),
    nth_request_index_(0),
    info_requested_(false)
  {
    odom_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "/odom", 10,
      std::bind(&MoveAction::odom_callback, this, std::placeholders::_1)
    );

    nav2_node_ = rclcpp::Node::make_shared("move_action_nav2_client");
    nav2_client_ = rclcpp_action::create_client<nav2_msgs::action::NavigateToPose>(
      nav2_node_, "navigate_to_pose"
    );
    
    // Subscriber to get all detected markers from world node
    detected_markers_sub_ = this->create_subscription<plansys2_interface::msg::DetectedMarkers>(
      "/world_node/detected_markers", 10,
      std::bind(&MoveAction::detected_markers_callback, this, std::placeholders::_1)
    );
  }

private:
  void do_work() override
  {
    // Skip if goal was already sent and we're waiting for result
    if (goal_sent_) {
      rclcpp::spin_some(nav2_node_);
      return;
    }
    
    auto args = get_arguments();
    if (args.size() < 3) {
      RCLCPP_ERROR(get_logger(), "Not enough arguments for move action");
      finish(false, 0.0, "Insufficient arguments");
      return;
    }

    std::string wp_to_navigate = args[2];

    double goal_x, goal_y;
    // Waypoint coordinates as per assignment 
    if(visited_waypoints_ >= 4){
      // Use cached markers from subscription (already sorted by world_node)
      if (!cached_markers_.empty() && nth_request_index_ < static_cast<int>(cached_markers_.size())) {
        marker_wp_ = cached_markers_[nth_request_index_].wp;
        nth_request_index_++;
        RCLCPP_INFO(get_logger(), "Got nth(%d) marker wp: %s",
                    nth_request_index_ - 1, marker_wp_.c_str());
      } else if (cached_markers_.empty()) {
        RCLCPP_WARN(get_logger(), "No markers received yet");
        return;
      } else {
        RCLCPP_INFO(get_logger(), "All markers already requested");
        return;
      }
      
      if(marker_wp_ == "")
        return;
      wp_to_navigate = marker_wp_;
      marker_wp_ = "";
    }
      if (wp_to_navigate == "wp1") {
        goal_x = -6.0;
        goal_y = -6.0;
      } else if (wp_to_navigate == "wp2") {
        goal_x = -6.0;
        goal_y = 6.0;
      } else if (wp_to_navigate == "wp3") {
        goal_x = 6.0;
        goal_y = 6.0;
      } else if (wp_to_navigate == "wp4") {
        goal_x = 6.0;
        goal_y = -6.0;
      } else {
        RCLCPP_ERROR(get_logger(), "Unknown waypoint: %s", wp_to_navigate.c_str());
        finish(false, 0.0, "Unknown waypoint");
        return;
      }
    

    if (!goal_sent_) {
      if (!nav2_client_->wait_for_action_server(1s)) {
        RCLCPP_WARN(get_logger(), "NavigateToPose server not ready");
        return;
      }

      geometry_msgs::msg::PoseStamped goal_pose;
      goal_pose.header.frame_id = "map";
      // goal_pose.header.stamp = this->now();
      goal_pose.pose.position.x = goal_x;
      goal_pose.pose.position.y = goal_y;
      // goal_pose.pose.position.z = 0.0;
      goal_pose.pose.orientation.w = 1.0;

      auto goal_msg = nav2_msgs::action::NavigateToPose::Goal();
      goal_msg.pose = goal_pose;

      rclcpp_action::Client<nav2_msgs::action::NavigateToPose>::SendGoalOptions send_goal_options;
      send_goal_options.result_callback =
        [this, wp_to_navigate](const rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>::WrappedResult & result)
        {
          if (result.code != rclcpp_action::ResultCode::SUCCEEDED) {
            RCLCPP_ERROR(get_logger(), "Navigation failed: %s", wp_to_navigate.c_str());
            goal_sent_ = false;
            finish(true, 1.0, "Move failed");
            return;
          }
          
          progress_ = 1.0;
          send_feedback(progress_, "Moving to " + wp_to_navigate);
      
          // Increment visited waypoints counter
          if (visited_waypoints_ <  4) {
            visited_waypoints_++;
          }
          
          RCLCPP_INFO(get_logger(), "Reached waypoint: %s (visited: %d)", wp_to_navigate.c_str(), visited_waypoints_);
          goal_sent_ = false;
          finish(true, 1.0, "Move completed");
        };

      nav2_client_->async_send_goal(goal_msg, send_goal_options);
      goal_sent_ = true;

      start_x_ = current_x_;
      start_y_ = current_y_;
      
      RCLCPP_INFO(get_logger(), "Navigating to %s at (%.2f, %.2f)", wp_to_navigate.c_str(), goal_x, goal_y);
    }

    double total_dist = std::hypot(goal_x - start_x_, goal_y - start_y_);
    double rem_dist   = std::hypot(goal_x - current_x_, goal_y - current_y_);
    progress_ = total_dist > 0.0 ? 1.0 - std::min(rem_dist / total_dist, 1.0) : 1.0;

    send_feedback(progress_, "Moving to " + wp_to_navigate);

    // if (rem_dist < 0.6) {
      
    // }

    // rclcpp::spin_some(nav2_node_);
  }

  void detected_markers_callback(const plansys2_interface::msg::DetectedMarkers::SharedPtr msg)
  {
    // Cache the received markers
    cached_markers_ = msg->markers;
    RCLCPP_INFO(get_logger(), "Received %zu detected markers", cached_markers_.size());
  }

  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    current_x_ = msg->pose.pose.position.x;
    current_y_ = msg->pose.pose.position.y;
  }


  float progress_;
  bool goal_sent_;
  double start_x_ = 0.0, start_y_ = 0.0;
  double current_x_ = 0.0, current_y_ = 0.0;
  std::string marker_wp_;
  int visited_waypoints_;
  int nth_request_index_;
  bool info_requested_;
  geometry_msgs::msg::Pose marker_pose_;

  // Cached markers from subscription
  std::vector<plansys2_interface::msg::DetectedMarker> cached_markers_;

  rclcpp::Node::SharedPtr nav2_node_;
  rclcpp_action::Client<nav2_msgs::action::NavigateToPose>::SharedPtr nav2_client_;
  rclcpp::Subscription<plansys2_interface::msg::DetectedMarkers>::SharedPtr detected_markers_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<MoveAction>();

  node->set_parameter(rclcpp::Parameter("action_name", "move"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);

  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();

  return 0;
}

