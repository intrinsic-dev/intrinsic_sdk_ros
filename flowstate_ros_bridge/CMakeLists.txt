cmake_minimum_required(VERSION 3.8)
project(flowstate_ros_bridge)

include(GNUInstallDirs)

# Find lttng-ust using PkgConfig to avoid hardcoding paths
find_package(PkgConfig REQUIRED)
pkg_check_modules(LTTNG_UST REQUIRED lttng-ust IMPORTED_TARGET)

# Add library path found by PkgConfig to linker search path for targets that might not inherit it
link_directories(${LTTNG_UST_LIBRARY_DIRS})

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

# other dependencies
set(dep_pkgs
  abseil_cpp_vendor
  flowstate_interfaces
  sensor_msgs
  std_msgs
  tf2_msgs
  visualization_msgs
  grpc_vendor
  intrinsic_sdk_cmake
  nlohmann_json
  pluginlib
  rclcpp
  rclcpp_action
  rclcpp_components
  rclcpp_lifecycle
  rclpy
  resource_retriever_interfaces
  tf2_ros
)
foreach(pkg ${dep_pkgs})
  find_package(${pkg} REQUIRED)
endforeach()

# create shared library for executive.
add_library(executive SHARED
  src/executive.cpp
)
target_include_directories(executive
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(executive
  absl::failure_signal_handler
  absl::flags
  absl::log
  absl::time
  absl::strings
  absl::cordz_functions
  intrinsic_sdk_cmake::intrinsic_sdk_cmake
  nlohmann_json::nlohmann_json
  ${flowstate_interfaces_TARGETS}
  ${resource_retriever_interfaces_TARGETS}
)

# create shared library for world.
add_library(world SHARED
  src/world.cpp
)
target_include_directories(world
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(world
  absl::flags
  absl::log
  absl::time
  absl::strings
  intrinsic_sdk_cmake::intrinsic_sdk_cmake
  nlohmann_json::nlohmann_json
  ${flowstate_interfaces_TARGETS}
  ${resource_retriever_interfaces_TARGETS}
)

ament_export_targets(executive HAS_LIBRARY_TARGET)
ament_export_dependencies(${dep_pkgs})

#===============================================================================

add_library(flowstate_ros_bridge_interface INTERFACE )
target_link_libraries(flowstate_ros_bridge_interface
  INTERFACE
    rclcpp::rclcpp
    world
    executive
)
target_include_directories(flowstate_ros_bridge_interface
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

#===============================================================================

add_library(flowstate_ros_builtin_bridges SHARED
  src/bridges/executive_bridge.cpp
  src/bridges/image_bridge.cpp
  src/bridges/world_bridge.cpp
)
target_include_directories(flowstate_ros_builtin_bridges
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${pluginlib_INCLUDE_DIRS}
    ${rclcpp_action_INCLUDE_DIRS}
)
target_compile_features(flowstate_ros_builtin_bridges PUBLIC INTERFACE cxx_std_17)
target_link_libraries(flowstate_ros_builtin_bridges
  PUBLIC
    world
    executive
    ${flowstate_interfaces_TARGETS}
    flowstate_ros_bridge_interface
    intrinsic_sdk_cmake::intrinsic_sdk_cmake
    nlohmann_json::nlohmann_json
    absl::strings
    absl::log
    pluginlib::pluginlib
    ${resource_retriever_interfaces_TARGETS}
    ${sensor_msgs_TARGETS}
    ${std_msgs_TARGETS}
    ${std_srvs_TARGETS}
    ${tf2_msgs_TARGETS}
    tf2_ros::tf2_ros
    ${visualization_msgs_TARGETS}
)

ament_export_libraries(flowstate_ros_builtin_bridges)
pluginlib_export_plugin_description_file(flowstate_ros_bridge src/bridges/plugins.xml)

#===============================================================================

# create composable node for flowstate_ros_bridge.
add_library(flowstate_ros_bridge_component SHARED
  src/flowstate_ros_bridge.cpp
)
target_include_directories(flowstate_ros_bridge_component
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${pluginlib_INCLUDE_DIRS}
    ${rclcpp_action_INCLUDE_DIRS}
)
target_link_libraries(flowstate_ros_bridge_component
  absl::flat_hash_map
  executive
  world
  flowstate_ros_bridge_interface
  intrinsic_sdk_cmake::intrinsic_sdk_cmake
  pluginlib::pluginlib
  rclcpp::rclcpp
  rclcpp_action::rclcpp_action
  rclcpp_components::component
  rclcpp_lifecycle::rclcpp_lifecycle
  nlohmann_json::nlohmann_json
  ${sensor_msgs_TARGETS}
  ${std_msgs_TARGETS}
  ${std_srvs_TARGETS}
  ${tf2_msgs_TARGETS}
  tf2_ros::tf2_ros
  ${visualization_msgs_TARGETS}
  PkgConfig::LTTNG_UST
)
target_compile_features(flowstate_ros_bridge_component PUBLIC cxx_std_17)
set_property(TARGET flowstate_ros_bridge_component PROPERTY POSITION_INDEPENDENT_CODE ON)
rclcpp_components_register_node(flowstate_ros_bridge_component
  PLUGIN "flowstate_ros_bridge::FlowstateROSBridge"
  EXECUTABLE flowstate_ros_bridge
  EXECUTOR EventsExecutor
)

# TEMPORARY WORKAROUND
# TODO(b-corry): Update with rclcpp_components_register_node that is able to access the
# runtime context for the service config. Delete ${SERVICE_NAME}_main when done
# Generate the skill's protobuf files, protobuf library, and descriptor set file.
set(SERVICE_NAME "flowstate_ros_bridge")

intrinsic_sdk_generate_service_manifest(
  SERVICE_NAME ${SERVICE_NAME}
  MANIFEST ${SERVICE_NAME}.manifest.textproto
  PARAMETER_DESCRIPTOR ${SERVICE_NAME}.proto
  DEFAULT_CONFIGURATION ${SERVICE_NAME}_default_config.pbtxt
  PROTOS_TARGET ${SERVICE_NAME}_protos
)
add_executable(${SERVICE_NAME}_main
 src/${SERVICE_NAME}_main.cpp
)
target_include_directories(${SERVICE_NAME}_main PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(${SERVICE_NAME}_main
  ${SERVICE_NAME}_component
  ${SERVICE_NAME}_protos
)

# installs
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS
  executive
  world
  ${SERVICE_NAME}_component
  ${SERVICE_NAME}_interface
  flowstate_ros_builtin_bridges
  ${SERVICE_NAME}_main
  EXPORT executive
  RUNTIME DESTINATION lib/${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install proto descriptor file
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_NAME}_protos.desc
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
