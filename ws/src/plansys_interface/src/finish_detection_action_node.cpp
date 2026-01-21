#include "plansys2_executor/ActionExecutorClient.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

class FinishDetectionAction : public plansys2::ActionExecutorClient
{
public:
  FinishDetectionAction()
  : plansys2::ActionExecutorClient("finishdetection", 100ms)
  {
  }

private:
  void do_work() override
  {
    // This action doesn't do anything special
    // It simply marks the detection phase as complete
    // and returns success immediately
    
    RCLCPP_INFO(get_logger(), "Detection phase complete - finishing");
    
    // Finish with success (true), 100% progress, and completion message
    finish(true, 1.0, "Detection phase completed");
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<FinishDetectionAction>();

  node->set_parameter(rclcpp::Parameter("action_name", "finishdetection"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);

  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();

  return 0;
}

