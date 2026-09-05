// Copyright 2025 Intrinsic Innovation LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BRIDGES__WORLD_BRIDGE_HPP_
#define BRIDGES__WORLD_BRIDGE_HPP_

#include <memory>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "flowstate_ros_bridge/bridge_interface.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "resource_retriever_interfaces/srv/get_resource.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "tf2_msgs/msg/tf_message.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

namespace flowstate_ros_bridge {

///=============================================================================
// The Node will retrieve information required for configuration from
// ROS Parameters.
class WorldBridge : public BridgeInterface {
 public:
  ~WorldBridge();

  using GetResource = resource_retriever_interfaces::srv::GetResource;

  /// Documentation inherited.
  void declare_ros_parameters(ROSNodeInterfaces ros_node_interfaces) final;

  /// Documentation inherited.
  bool initialize(ROSNodeInterfaces ros_node_interfaces,
                  std::shared_ptr<Executive> executive_client,
                  std::shared_ptr<World> world_client,
                  std::shared_ptr<intrinsic::PubSub> pubsub) final;

 private:
  void TfCallback(const intrinsic_proto::TFMessage&);
  void SimTfCallback(const intrinsic_proto::TFMessage&);

  tf2_msgs::msg::TFMessage ConvertTfProtoToRos(
      const intrinsic_proto::TFMessage& tf_proto) const;

  static std::string StripTfPrefixes(absl::string_view frame,
                                     const std::vector<std::string>& prefixes);

  void RobotStateCallback(const intrinsic_proto::data_logger::LogItem&);

  // Helper method for unpacking data in the RobotStatus proto and publishing
  // them
  void HandleRobotStatus(const intrinsic_proto::icon::RobotStatus& robot_status,
                         const rclcpp::Time& time);

  void PublishJointState(const std::string& part_name,
                         const std::string& frame_id,
                         const intrinsic_proto::icon::PartStatus& part_status,
                         const rclcpp::Time& time);

  void PublishForceTorqueSensor(
      const std::string& frame_id,
      const intrinsic_proto::icon::PartStatus& part_status,
      const rclcpp::Time& time);

  struct Data : public std::enable_shared_from_this<Data> {
    /**
     * @brief Send visualization messages for Flowstate sceneObjects
     *
     * @param object_names An optional vector of the names of SceneObjects to be
     * retrieved and published as Marker messages. If unspecified, all
     * SceneObjects in the Belief World will be retrieved and published.
     * @return absl::Status
     */
    absl::Status SendObjectVisualizationMessages(
        std::optional<std::vector<std::string>> object_names = std::nullopt);

    ROSNodeInterfaces node_interfaces_;
    std::shared_ptr<World> world_;

    // TF functionality
    std::shared_ptr<intrinsic::Subscription> tf_sub_;
    std::shared_ptr<rclcpp::Publisher<tf2_msgs::msg::TFMessage>> tf_pub_;

    // Sim TF functionality
    std::shared_ptr<intrinsic::Subscription> sim_tf_sub_;
    std::shared_ptr<rclcpp::Publisher<tf2_msgs::msg::TFMessage>> sim_tf_pub_;

    // Robot state and force torque functionality
    std::shared_ptr<intrinsic::Subscription> robot_state_sub_;
    std::shared_ptr<rclcpp::Publisher<sensor_msgs::msg::JointState>>
        robot_joint_state_pub_;
    std::shared_ptr<rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>>
        force_torque_pub_;
    bool robot_joint_state_topic_enabled_;
    bool force_torque_topic_enabled_;
    std::string ft_sensor_frame_id_;
    std::string robot_base_frame_id_;
    // Cached part names to avoid repeated map iteration
    std::optional<std::string> robot_arm_part_name_;
    std::optional<std::string> force_torque_part_name_;

    std::vector<std::string> override_joint_names_;

    std::shared_ptr<rclcpp::Publisher<visualization_msgs::msg::MarkerArray>>
        workcell_markers_pub_;
    std::string tf_prefix_;
    std::vector<std::string> strip_flowstate_tf_prefixes_;
    std::shared_ptr<rclcpp::Service<GetResource>> get_resource_srv_;
    absl::flat_hash_map<std::string, std::vector<uint8_t>> renderables_
        ABSL_GUARDED_BY(mutex_);
    absl::flat_hash_set<std::string> tf_frame_names_;
    std::optional<std::vector<std::string>> send_object_names_
        ABSL_GUARDED_BY(mutex_) = std::nullopt;
    bool send_new_objects_ ABSL_GUARDED_BY(mutex_) = true;
    std::shared_ptr<std::thread> viz_thread_;
    absl::Mutex
        mutex_;  // protects send_object_names_, send_new_objects_, renderables_
    std::string mesh_url_prefix_;
    ~Data();
  };
  std::shared_ptr<Data> data_;
};

}  // namespace flowstate_ros_bridge.

#endif  // BRIDGES__WORLD_BRIDGE_HPP_
