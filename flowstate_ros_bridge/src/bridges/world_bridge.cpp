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

#include "world_bridge.hpp"

#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "intrinsic/eigenmath/types.h"
#include "intrinsic/math/proto_conversion.h"
#include "intrinsic/util/eigen.h"
#include "tf2_ros/qos.hpp"

namespace flowstate_ros_bridge {

constexpr const char* kTfPrefixParamName = "world_tf_prefix";
constexpr const char* kStripFlowstateTfPrefixParamName =
    "strip_flowstate_tf_prefix";
constexpr const char* kResourceServiceName = "flowstate_get_resource";
constexpr const char* kMeshUrlPrefixParamName = "mesh_url_prefix";
constexpr const char* kEnableRobotJointStateTopicParamName =
    "enable_robot_joint_state_topic";
constexpr const char* kEnableForceTorqueTopicParamName =
    "enable_force_torque_topic";
constexpr const char* kRobotJointStateTopicParamName =
    "robot_joint_state_topic";
constexpr const char* kForceTorqueTopicParamName = "force_torque_topic";
constexpr const char* kForceTorqueSensorFrameIDParamName =
    "force_torque_sensor_frame_id";
constexpr const char* kRobotBaseFrameIDParamName = "robot_base_frame_id";
constexpr const char* kRobotControllerNameParamName =
    "robot_controller_instance";
constexpr const char* kThrottleRobotStateTopicParamName =
    "throttle_robot_state_topic";
constexpr const char* kOverrideJointNamesParamName = "override_joint_names";

///=============================================================================
void WorldBridge::declare_ros_parameters(
    ROSNodeInterfaces ros_node_interfaces) {
  const auto& param_interface =
      ros_node_interfaces
          .get<rclcpp::node_interfaces::NodeParametersInterface>();

  param_interface->declare_parameter(kTfPrefixParamName,
                                     rclcpp::ParameterValue{""});
  param_interface->declare_parameter(
      kStripFlowstateTfPrefixParamName,
      rclcpp::ParameterValue(std::vector<std::string>{}));
  param_interface->declare_parameter(
      kMeshUrlPrefixParamName,
      rclcpp::ParameterValue{"service:///flowstate_get_resource:/"});
  param_interface->declare_parameter(kEnableRobotJointStateTopicParamName,
                                     rclcpp::ParameterValue(true));
  param_interface->declare_parameter(kEnableForceTorqueTopicParamName,
                                     rclcpp::ParameterValue(true));
  param_interface->declare_parameter(kRobotJointStateTopicParamName,
                                     rclcpp::ParameterValue("/joint_states"));
  param_interface->declare_parameter(
      kForceTorqueTopicParamName,
      rclcpp::ParameterValue("/fts_broadcaster/wrench"));
  param_interface->declare_parameter(
      kForceTorqueSensorFrameIDParamName,
      rclcpp::ParameterValue(
          "force_torque_sensor/force_torque_sensor/AtiForceTorqueSensor"));
  param_interface->declare_parameter(
      kRobotBaseFrameIDParamName,
      rclcpp::ParameterValue("robot/robot/base_link"));
  param_interface->declare_parameter(
      kRobotControllerNameParamName,
      rclcpp::ParameterValue("robot_controller"));
  param_interface->declare_parameter(kThrottleRobotStateTopicParamName,
                                     rclcpp::ParameterValue(false));
  param_interface->declare_parameter(
      kOverrideJointNamesParamName,
      rclcpp::ParameterValue(std::vector<std::string>{}));
}

///=============================================================================
bool WorldBridge::initialize(ROSNodeInterfaces ros_node_interfaces,
                             std::shared_ptr<Executive> /*executive_client*/,
                             std::shared_ptr<World> world_client,
                             std::shared_ptr<intrinsic::PubSub> /*pubsub*/) {
  data_ = std::make_shared<Data>();
  data_->node_interfaces_ = std::move(ros_node_interfaces);
  data_->world_ = std::move(world_client);

  std::shared_ptr<rclcpp::node_interfaces::NodeParametersInterface>
      param_interface =
          data_->node_interfaces_
              .get<rclcpp::node_interfaces::NodeParametersInterface>();

  data_->get_resource_srv_ = rclcpp::create_service<GetResource>(
      data_->node_interfaces_.get<rclcpp::node_interfaces::NodeBaseInterface>(),
      data_->node_interfaces_
          .get<rclcpp::node_interfaces::NodeServicesInterface>(),
      kResourceServiceName,
      [data_ = this->data_](const std::shared_ptr<GetResource::Request> request,
                            std::shared_ptr<GetResource::Response> response) {
        const std::string gltf_id = request->path;
        LOG(INFO) << "request resource path: " << gltf_id;

        {
          absl::MutexLock lock(&data_->mutex_);
          auto it = data_->renderables_.find(gltf_id);
          if (it == data_->renderables_.end()) {
            response->status_code = GetResource::Response::ERROR;
            return;
          }
          response->body = it->second;
        }
        response->status_code = GetResource::Response::OK;
      },
      rclcpp::ServicesQoS(), nullptr);

  data_->tf_prefix_ = param_interface->get_parameter(kTfPrefixParamName)
                          .get_value<std::string>();
  data_->strip_flowstate_tf_prefixes_ =
      param_interface->get_parameter(kStripFlowstateTfPrefixParamName)
          .as_string_array();

  std::shared_ptr<rclcpp::node_interfaces::NodeTopicsInterface>
      topics_interface =
          data_->node_interfaces_
              .get<rclcpp::node_interfaces::NodeTopicsInterface>();

  data_->tf_pub_ = rclcpp::create_publisher<tf2_msgs::msg::TFMessage>(
      param_interface, topics_interface, "tf",
      tf2_ros::DynamicBroadcasterQoS());

  data_->sim_tf_pub_ = rclcpp::create_publisher<tf2_msgs::msg::TFMessage>(
      param_interface, topics_interface, "tf_sim",
      tf2_ros::DynamicBroadcasterQoS());

  const rclcpp::QoS markers_qos =
      rclcpp::QoS(rclcpp::KeepLast(1)).transient_local();
  data_->workcell_markers_pub_ =
      rclcpp::create_publisher<visualization_msgs::msg::MarkerArray>(
          param_interface, topics_interface, "workcell_markers", markers_qos);

  data_->mesh_url_prefix_ =
      param_interface->get_parameter(kMeshUrlPrefixParamName)
          .get_value<std::string>();

  data_->override_joint_names_ =
      param_interface->get_parameter(kOverrideJointNamesParamName)
          .as_string_array();

  auto tf_sub_ = data_->world_->CreateTfSubscription(
      [this](const intrinsic_proto::TFMessage& msg) { this->TfCallback(msg); });
  if (!tf_sub_.ok()) {
    LOG(ERROR) << "Unable to create TF Subscription: " << tf_sub_.status();
    return false;
  }
  LOG(INFO) << "Subscribed to Flowstate TF topic";
  data_->tf_sub_ = std::move(*tf_sub_);

  auto sim_tf_sub = data_->world_->CreateTfSubscription(
      [this](const intrinsic_proto::TFMessage& msg) {
        this->SimTfCallback(msg);
      },
      /*sim=*/true);
  if (!sim_tf_sub.ok()) {
    LOG(WARNING) << "Unable to create Sim TF Subscription: "
                 << sim_tf_sub.status();
  } else {
    LOG(INFO) << "Subscribed to Flowstate Sim TF topic";
    data_->sim_tf_sub_ = std::move(*sim_tf_sub);
  }

  // Robot States Bridge
  data_->robot_joint_state_topic_enabled_ =
      param_interface->get_parameter(kEnableRobotJointStateTopicParamName)
          .as_bool();
  data_->force_torque_topic_enabled_ =
      param_interface->get_parameter(kEnableForceTorqueTopicParamName)
          .as_bool();
  LOG(INFO) << "Robot Joint State Bridge Enabled: "
            << data_->robot_joint_state_topic_enabled_;
  LOG(INFO) << "Force Torque Bridge Enabled: "
            << data_->force_torque_topic_enabled_;

  // Create ROS publishers
  data_->robot_joint_state_pub_ =
      rclcpp::create_publisher<sensor_msgs::msg::JointState>(
          param_interface, topics_interface,
          param_interface->get_parameter(kRobotJointStateTopicParamName)
              .as_string(),
          rclcpp::SystemDefaultsQoS());
  data_->force_torque_pub_ =
      rclcpp::create_publisher<geometry_msgs::msg::WrenchStamped>(
          param_interface, topics_interface,
          param_interface->get_parameter(kForceTorqueTopicParamName)
              .as_string(),
          rclcpp::SensorDataQoS());

  // Create Flowstate subscriptions
  std::string robot_controller_instance =
      param_interface->get_parameter(kRobotControllerNameParamName).as_string();
  bool throttle_robot_state_topic =
      param_interface->get_parameter(kThrottleRobotStateTopicParamName)
          .as_bool();
  auto robot_state_sub = data_->world_->CreateRobotStateSubscription(
      [this](const intrinsic_proto::data_logger::LogItem& msg) {
        this->RobotStateCallback(msg);
      },
      robot_controller_instance, throttle_robot_state_topic);
  if (!robot_state_sub.ok()) {
    LOG(ERROR) << "Unable to create Robot State Subscription: "
               << robot_state_sub.status();
    return false;
  }
  LOG(INFO) << "Subscribed to Flowstate Robot State topic";
  data_->robot_state_sub_ = std::move(*robot_state_sub);

  data_->ft_sensor_frame_id_ =
      param_interface->get_parameter(kForceTorqueSensorFrameIDParamName)
          .as_string();
  data_->robot_base_frame_id_ =
      param_interface->get_parameter(kRobotBaseFrameIDParamName).as_string();

  // Start a thread to publish sceneObject visualization messages whenever a new
  // object arrives
  std::weak_ptr<Data> data_wp = data_;
  data_->viz_thread_ = std::make_shared<std::thread>([data_wp]() {
    while (rclcpp::ok()) {
      if (auto data = data_wp.lock()) {
        data->mutex_.LockWhen(absl::Condition(
            +[](bool* condn) { return *condn; }, &data->send_new_objects_));

        // Copy data and unlock immediately so TfCallback is not blocked during
        // heavy mesh downloading.
        std::optional<std::vector<std::string>> local_object_names =
            data->send_object_names_;
        data->send_object_names_ = std::nullopt;
        data->send_new_objects_ = false;
        data->mutex_.Unlock();

        const absl::Status status =
            data->SendObjectVisualizationMessages(local_object_names);
        if (!status.ok()) {
          LOG(ERROR) << "Unable to send object visualization messages: "
                     << status.message();
        }
      } else {
        LOG(ERROR) << "data has expired! Terminating thread sending "
                      "visualization objects";
        return;
      }
    }
  });

  return true;
}

absl::Status WorldBridge::Data::SendObjectVisualizationMessages(
    std::optional<std::vector<std::string>> object_names) {
  bool rotate_mesh = true;
  const char* ros_distro = std::getenv("ROS_DISTRO");
  if (ros_distro != nullptr && std::string_view(ros_distro) == "jazzy") {
    rotate_mesh = false;
    LOG(INFO) << "Keeping mesh orientation as-is.";
  } else {
    LOG(INFO) << "Adding extra rotation to correct glTF orientation.";
  }

  absl::StatusOr<std::vector<intrinsic::world::WorldObject>> objects =
      world_->GetObjects(std::move(object_names));
  if (!objects.ok()) {
    return objects.status();
  }

  LOG(INFO) << "Retrieved " << objects->size() << " world objects:";

  size_t total_gltf_size = 0;
  visualization_msgs::msg::MarkerArray array_msg;
  rclcpp::Clock clock;
  const rclcpp::Time t = clock.now();

  for (const intrinsic::world::WorldObject& object : *objects) {
    int object_id = 0;
    const intrinsic_proto::world::Object& proto = object.Proto();
    for (const auto& entity : proto.entities()) {
      if (!entity.second.has_geometry_component()) {
        continue;
      }
      for (const auto& named_geometry :
           entity.second.geometry_component().named_geometries()) {
        if (named_geometry.first != "Intrinsic_Visual") continue;

        const auto& named_geoms = named_geometry.second.named_geometries();

        for (const auto& [inner_name, transformed_geometry] : named_geoms) {
          const auto& geometry = transformed_geometry.geometry();
          if (!geometry.has_geo_ref()) {
            continue;
          }
          const auto& geo_ref = geometry.geo_ref();
          std::string object_name = object.Name().value();
          if (object_name.empty()) {
            object_name =
                proto.name().empty() ? entity.second.name() : proto.name();
          }
          object_name =
              StripTfPrefixes(object_name, strip_flowstate_tf_prefixes_);
          std::string tf_frame_name = absl::StrFormat(
              "%s%s/%s", tf_prefix_.c_str(), object_name, entity.second.name());

          // Safely strip intcas:// prefix if present
          std::string exact_geom_ref = geo_ref.exact_geometry_ref();
          if (absl::StartsWith(exact_geom_ref, "intcas://")) {
            exact_geom_ref = exact_geom_ref.substr(9);
          }
          std::string renderable_ref = geo_ref.renderable_ref();
          if (absl::StartsWith(renderable_ref, "intcas://")) {
            renderable_ref = renderable_ref.substr(9);
          }
          const std::string gltf_path =
              absl::StrFormat("gltf/%s_%s.glb", exact_geom_ref, renderable_ref);

          const auto renderable_name = std::string("/") + gltf_path;

          bool has_renderable = false;
          {
            absl::MutexLock lock(&mutex_);
            has_renderable = renderables_.contains(renderable_name);
          }

          if (!has_renderable) {
            const absl::StatusOr<std::string> gltf = world_->GetGltf(
                geo_ref.exact_geometry_ref(), geo_ref.renderable_ref());
            if (!gltf.ok()) {
              LOG(ERROR) << "Unable to fetch renderable for " << tf_frame_name
                         << ": " << gltf.status();
              continue;
            }
            total_gltf_size += gltf->size();
            std::vector<uint8_t> gltf_data;
            gltf_data.resize(gltf->size());
            memcpy(&gltf_data[0], gltf->data(), gltf->size());

            LOG(INFO) << "Fetched " << gltf->size() << " bytes for "
                      << tf_frame_name;

            // Safely insert the newly downloaded mesh
            absl::MutexLock lock(&mutex_);
            renderables_.emplace(renderable_name, std::move(gltf_data));
          }

          const auto& ref_t_shape = transformed_geometry.ref_t_shape();
          if (!ref_t_shape.has_matrix4d()) {
            continue;
          }
          const absl::StatusOr<intrinsic::eigenmath::MatrixXd> transform_xd =
              intrinsic_proto::FromProto(ref_t_shape.matrix4d());
          if (!transform_xd.ok()) {
            continue;
          }
          const intrinsic::eigenmath::Matrix4d transform_4d = *transform_xd;
          const intrinsic::eigenmath::AffineTransform3d affine(transform_4d);

          visualization_msgs::msg::Marker marker_msg;
          marker_msg.header.frame_id = tf_frame_name;
          marker_msg.header.stamp = t;
          marker_msg.ns = tf_frame_name;
          marker_msg.id = object_id++;
          marker_msg.type = visualization_msgs::msg::Marker::MESH_RESOURCE;
          marker_msg.action = visualization_msgs::msg::Marker::ADD;
          marker_msg.pose.position.x = affine.translation().x();
          marker_msg.pose.position.y = affine.translation().y();
          marker_msg.pose.position.z = affine.translation().z();
          intrinsic::eigenmath::Quaterniond quat(affine.rotation());
          if (rotate_mesh) {
            // Manually rotate the mesh by 90 degrees to align with its correct
            // orientation.
            const double inv_sqrt_2 =
                1.4142135623730951 / 2.0;  // std::sqrt(2.0)/2.0
            const intrinsic::eigenmath::Quaterniond rot_x_neg_90(
                inv_sqrt_2, -inv_sqrt_2, 0.0, 0.0);
            quat = quat * rot_x_neg_90;
          }
          marker_msg.pose.orientation.x = quat.x();
          marker_msg.pose.orientation.y = quat.y();
          marker_msg.pose.orientation.z = quat.z();
          marker_msg.pose.orientation.w = quat.w();
          // Currently all of our meshes have unit scaling. If this changes,
          // we could use Transform::computeRotationScaling() but that costs
          // a SVD, and it doesn't seem worth it when _all_ of our meshes are
          // already at unit scale.
          marker_msg.scale.x = 1.0;
          marker_msg.scale.y = 1.0;
          marker_msg.scale.z = 1.0;
          // Leave color as (1, 1, 1, 1) so that the mesh color is as expected
          // from the embedded glTF textures.
          marker_msg.color.r = 1.0;
          marker_msg.color.g = 1.0;
          marker_msg.color.b = 1.0;
          marker_msg.color.a = 1.0;
          // Set lifetime to (0, 0) to indicate these meshes never expire.
          marker_msg.lifetime.sec = 0;
          marker_msg.lifetime.nanosec = 0;
          // Lock the mesh to its TF frame so that motion is handled correctly
          marker_msg.frame_locked = true;
          // Set the mesh resource path to the HTTP/rmw_zenoh proxy
          marker_msg.mesh_resource = mesh_url_prefix_ + gltf_path;
          marker_msg.mesh_use_embedded_materials = true;

          array_msg.markers.push_back(std::move(marker_msg));
        }
      }
    }
  }
  LOG(INFO) << "Total gltf size: " << total_gltf_size << " bytes";

  if (!array_msg.markers.empty()) {
    workcell_markers_pub_->publish(array_msg);
  }

  return absl::OkStatus();
}

///=============================================================================
WorldBridge::Data::~Data() {}

///=============================================================================
std::string WorldBridge::StripTfPrefixes(
    absl::string_view frame, const std::vector<std::string>& prefixes) {
  absl::string_view stripped = frame;
  for (const auto& prefix : prefixes) {
    // Add check to only strip the prefix once per frame
    if (!prefix.empty() && absl::StartsWith(stripped, prefix)) {
      stripped = absl::StripPrefix(stripped, prefix);
      break;
    }
  }
  return std::string(stripped);
}

///=============================================================================
tf2_msgs::msg::TFMessage WorldBridge::ConvertTfProtoToRos(
    const intrinsic_proto::TFMessage& tf_proto) const {
  tf2_msgs::msg::TFMessage tf_ros;
  tf_ros.transforms = std::vector<geometry_msgs::msg::TransformStamped>(
      tf_proto.transforms_size());
  int tf_idx = 0;
  for (const auto& ts_proto : tf_proto.transforms()) {
    geometry_msgs::msg::TransformStamped* ts_ros = &tf_ros.transforms[tf_idx++];
    ts_ros->header.stamp.sec = ts_proto.header().stamp().seconds();
    ts_ros->header.stamp.nanosec = ts_proto.header().stamp().nanos();

    // Strip away Flowstate TF prefixes
    const std::string frame_id = StripTfPrefixes(
        ts_proto.header().frame_id(), data_->strip_flowstate_tf_prefixes_);

    const std::string child_frame_id = StripTfPrefixes(
        ts_proto.child_frame_id(), data_->strip_flowstate_tf_prefixes_);

    ts_ros->header.frame_id = data_->tf_prefix_ + frame_id;
    ts_ros->child_frame_id = data_->tf_prefix_ + child_frame_id;

    // The auto-generated CDR types do not currently have assignment operators
    // or helper conversion functions from the corresponding protos, so we need
    // to explicitly copy all the fields.
    const auto& t = ts_proto.transform();  // just to save some typing
    ts_ros->transform.translation.x = t.translation().x();
    ts_ros->transform.translation.y = t.translation().y();
    ts_ros->transform.translation.z = t.translation().z();
    ts_ros->transform.rotation.x = t.rotation().x();
    ts_ros->transform.rotation.y = t.rotation().y();
    ts_ros->transform.rotation.z = t.rotation().z();
    ts_ros->transform.rotation.w = t.rotation().w();
  }
  return tf_ros;
}

///=============================================================================
void WorldBridge::TfCallback(const intrinsic_proto::TFMessage& tf_proto) {
  rclcpp::Clock clock;
  const rclcpp::Time t_start = clock.now();

  tf2_msgs::msg::TFMessage tf_ros = ConvertTfProtoToRos(tf_proto);
  data_->tf_pub_->publish(tf_ros);

  // print a timing snapshot every 500 messages
  const rclcpp::Duration elapsed = clock.now() - t_start;
  LOG_EVERY_N(INFO, 500) << absl::StrFormat("tf translation time: %.3f ms",
                                            1000.0 * elapsed.seconds());

  absl::flat_hash_set<std::string> new_tf_frame_names;
  absl::flat_hash_set<std::string> new_object_names;
  for (const auto& ts_ros : tf_ros.transforms) {
    new_tf_frame_names.insert(ts_ros.child_frame_id);
    if (!data_->tf_frame_names_.contains(ts_ros.child_frame_id)) {
      LOG(INFO) << "new child_frame_id: " << ts_ros.child_frame_id;
      absl::string_view child_frame = ts_ros.child_frame_id;
      // Strip ROS tf_prefix if present to retrieve the original Flowstate
      // object name.
      if (!data_->tf_prefix_.empty() &&
          absl::StartsWith(child_frame, data_->tf_prefix_)) {
        child_frame = absl::StripPrefix(child_frame, data_->tf_prefix_);
      }
      // We parse the "OBJECT_NAME/ENTITY_NAME" string to get the OBJECT_NAME
      const std::size_t str_end = child_frame.find('/');
      new_object_names.insert(std::string(child_frame.substr(0, str_end)));
    }
  }

  std::vector<std::string> deleted_tf_frames;
  for (const auto& tf_frame : data_->tf_frame_names_) {
    if (!new_tf_frame_names.contains(tf_frame)) {
      deleted_tf_frames.push_back(tf_frame);
    }
  }
  if (!deleted_tf_frames.empty()) {
    visualization_msgs::msg::MarkerArray array_msg;
    for (const auto& tf_frame : deleted_tf_frames) {
      LOG(INFO) << "Removed sceneObject with frame_id " << tf_frame
                << " in the world, updating visualization markers.";

      visualization_msgs::msg::Marker marker_msg;
      marker_msg.ns = tf_frame;
      marker_msg.action = visualization_msgs::msg::Marker::DELETEALL;

      array_msg.markers.push_back(std::move(marker_msg));
    }
    data_->workcell_markers_pub_->publish(array_msg);
  }

  if (!new_object_names.empty()) {
    absl::MutexLock lock(&data_->mutex_);
    if (data_->send_object_names_.has_value()) {
      data_->send_object_names_.value().insert(
          data_->send_object_names_.value().end(), new_object_names.begin(),
          new_object_names.end());
    } else {
      data_->send_object_names_ = std::vector<std::string>(
          new_object_names.begin(), new_object_names.end());
    }
    // Signal background thread to send object visualization messages
    data_->send_new_objects_ = true;
  }

  data_->tf_frame_names_ = std::move(new_tf_frame_names);
}

///=============================================================================
void WorldBridge::SimTfCallback(const intrinsic_proto::TFMessage& tf_proto) {
  if (!data_->sim_tf_pub_) {
    return;
  }
  rclcpp::Clock clock;
  const rclcpp::Time t_start = clock.now();

  data_->sim_tf_pub_->publish(ConvertTfProtoToRos(tf_proto));

  // print a timing snapshot every 500 messages
  const rclcpp::Duration elapsed = clock.now() - t_start;
  LOG_EVERY_N(INFO, 500) << absl::StrFormat("sim tf translation time: %.3f ms",
                                            1000.0 * elapsed.seconds());
}

///=============================================================================
void WorldBridge::RobotStateCallback(
    const intrinsic_proto::data_logger::LogItem& log_item) {
  rclcpp::Clock clock;
  const rclcpp::Time t_start = clock.now();
  const auto& payload = log_item.payload();

  switch (payload.data_case()) {
    // At the time of writing this was the only received item in the LogItem msg
    case intrinsic_proto::data_logger::LogItem::Payload::kIconRobotStatus: {
      if (data_->robot_joint_state_topic_enabled_ ||
          data_->force_torque_topic_enabled_) {
        HandleRobotStatus(payload.icon_robot_status(), t_start);
      }
      break;
    }

    default: {
      std::string msg;
      const auto* descriptor = payload.GetDescriptor();
      const auto* field = descriptor->FindFieldByNumber(payload.data_case());
      if (field) {
        msg = absl::StrFormat("Received unhandled data type: %s (ID: %d)",
                              field->name(), payload.data_case());
      } else {
        msg = absl::StrFormat("Received unknown or unset data type (ID: %d)",
                              payload.data_case());
      }
      LOG_EVERY_N(INFO, 100) << msg;
      break;
    }
  }

  // print the translation time every 5000 messages
  const rclcpp::Duration elapsed = clock.now() - t_start;
  LOG_EVERY_N(INFO, 5000) << absl::StrFormat(
      "Robot state translation time: %.3f ms", 1000.0 * elapsed.seconds());
}

void WorldBridge::HandleRobotStatus(
    const intrinsic_proto::icon::RobotStatus& robot_status,
    const rclcpp::Time& time) {
  // On first execution, cache the part names to avoid repeated map iteration
  if (!data_->robot_arm_part_name_.has_value()) {
    for (const auto& entry : robot_status.status_map()) {
      const std::string& part_name = entry.first;
      const auto& part_status = entry.second;
      if (!part_status.joint_states().empty()) {
        data_->robot_arm_part_name_ = part_name;
        LOG(INFO) << "Cached robot arm part name: " << part_name;
      }
    }
  }
  if (!data_->force_torque_part_name_.has_value()) {
    for (const auto& entry : robot_status.status_map()) {
      const std::string& part_name = entry.first;
      const auto& part_status = entry.second;
      if (!data_->force_torque_part_name_.has_value() &&
          part_status.has_wrench_at_ft()) {
        data_->force_torque_part_name_ = part_name;
        LOG(INFO) << "Cached force torque sensor part name: " << part_name;
      }
    }
  }

  // Use cached part names for publishing
  if (data_->robot_joint_state_topic_enabled_ &&
      data_->robot_arm_part_name_.has_value()) {
    const std::string& part_name = data_->robot_arm_part_name_.value();
    auto it = robot_status.status_map().find(part_name);
    if (it != robot_status.status_map().end()) {
      PublishJointState(part_name, data_->robot_base_frame_id_, it->second,
                        time);
    } else {
      LOG_EVERY_N(ERROR, 100)
          << "Error: Robot arm part [" << part_name << "] not found!";
    }
  }

  if (data_->force_torque_topic_enabled_ &&
      data_->force_torque_part_name_.has_value()) {
    const std::string& part_name = data_->force_torque_part_name_.value();
    auto it = robot_status.status_map().find(part_name);
    if (it != robot_status.status_map().end()) {
      PublishForceTorqueSensor(data_->ft_sensor_frame_id_, it->second, time);
    } else {
      LOG_EVERY_N(ERROR, 100)
          << "Error: Force torque part [" << part_name << "] not found!";
    }
  }
}

void WorldBridge::PublishJointState(
    const std::string& part_name, const std::string& frame_id,
    const intrinsic_proto::icon::PartStatus& part_status,
    const rclcpp::Time& time) {
  sensor_msgs::msg::JointState robot_joint_state_ros;
  robot_joint_state_ros.header.stamp = time;
  robot_joint_state_ros.header.frame_id = frame_id;

  for (int i = 0; i < part_status.joint_states_size(); ++i) {
    const auto& joint_state = part_status.joint_states(i);
    std::string joint_name = absl::StrFormat("%s_joint_%d", part_name, i);
    if (!data_->override_joint_names_.empty()) {
      if (std::size_t(part_status.joint_states_size()) ==
          data_->override_joint_names_.size()) {
        joint_name = data_->override_joint_names_[i];
      } else {
        LOG_EVERY_N(ERROR, 100)
            << "Size of override_joint_names is not equal to "
               "size of joints from part ["
            << part_name << "]. Using default joint names!";
      }
    }
    robot_joint_state_ros.name.push_back(joint_name);

    double pos = joint_state.has_position_sensed()
                     ? joint_state.position_sensed()
                     : std::numeric_limits<double>::quiet_NaN();
    double vel = joint_state.has_velocity_sensed()
                     ? joint_state.velocity_sensed()
                     : std::numeric_limits<double>::quiet_NaN();
    double eff = joint_state.has_torque_sensed()
                     ? joint_state.torque_sensed()
                     : std::numeric_limits<double>::quiet_NaN();

    robot_joint_state_ros.position.push_back(pos);
    robot_joint_state_ros.velocity.push_back(vel);
    robot_joint_state_ros.effort.push_back(eff);
  }
  data_->robot_joint_state_pub_->publish(robot_joint_state_ros);
}

void WorldBridge::PublishForceTorqueSensor(
    const std::string& frame_id,
    const intrinsic_proto::icon::PartStatus& part_status,
    const rclcpp::Time& time) {
  geometry_msgs::msg::WrenchStamped wrench_msg;
  wrench_msg.header.stamp = time;
  wrench_msg.header.frame_id = frame_id;

  const auto& w = part_status.wrench_at_ft();
  wrench_msg.wrench.force.x = w.x();
  wrench_msg.wrench.force.y = w.y();
  wrench_msg.wrench.force.z = w.z();
  wrench_msg.wrench.torque.x = w.rx();
  wrench_msg.wrench.torque.y = w.ry();
  wrench_msg.wrench.torque.z = w.rz();

  data_->force_torque_pub_->publish(wrench_msg);
}

///=============================================================================

WorldBridge::~WorldBridge() {
  if (data_->viz_thread_ && data_->viz_thread_->joinable()) {
    data_->viz_thread_->join();
  }
  data_->viz_thread_.reset();
}

}  // namespace flowstate_ros_bridge

#include <pluginlib/class_list_macros.hpp>

PLUGINLIB_EXPORT_CLASS(flowstate_ros_bridge::WorldBridge,
                       flowstate_ros_bridge::BridgeInterface)
