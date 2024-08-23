cmake_minimum_required(VERSION 3.20)
project(intrinsic_sdk LANGUAGES CXX)

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)

# Must find grpc_vendor first as gRPC/protobuf/absl are installed in non-default locations
find_package(grpc_vendor REQUIRED)
find_package(absl REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(re2 REQUIRED)

# Bring in additional googleapis necessary to build Intrinsic proto files
include(FetchContent)
FetchContent_Declare(
  googleapis
  GIT_REPOSITORY https://github.com/googleapis/googleapis
  GIT_TAG        master
)
FetchContent_MakeAvailable(googleapis)

FetchContent_Declare(
  grpc_gateway
  GIT_REPOSITORY https://github.com/grpc-ecosystem/grpc-gateway
  GIT_TAG        main
)
FetchContent_MakeAvailable(grpc_gateway)

# Generate source for Intrinsic proto files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/protos_gen)

file(GLOB_RECURSE intrinsic_proto_SRCS "sdk/**/*.proto")
list(FILTER intrinsic_proto_SRCS EXCLUDE REGEX "_test\\.proto$")

set(grpc_SOURCE_DIR "${gRPC_DIR}/../../../share/grpc-proto")

set(grpc_SRCS
  ${grpc_SOURCE_DIR}/src/proto/grpc/health/v1/health.proto
)
set(googleapis_SRCS
  ${googleapis_SOURCE_DIR}/google/api/annotations.proto
  ${googleapis_SOURCE_DIR}/google/api/client.proto
  ${googleapis_SOURCE_DIR}/google/api/field_behavior.proto
  ${googleapis_SOURCE_DIR}/google/api/http.proto
  ${googleapis_SOURCE_DIR}/google/api/launch_stage.proto
  ${googleapis_SOURCE_DIR}/google/longrunning/operations.proto
  ${googleapis_SOURCE_DIR}/google/rpc/code.proto
  ${googleapis_SOURCE_DIR}/google/rpc/status.proto
)
set(grpc_gateway_SRCS
  ${grpc_gateway_SOURCE_DIR}/protoc-gen-openapiv2/options/annotations.proto
  ${grpc_gateway_SOURCE_DIR}/protoc-gen-openapiv2/options/openapiv2.proto
)
add_library(intrinsic_sdk_protos STATIC ${intrinsic_proto_SRCS} ${googleapis_SRCS} ${grpc_gateway_SRCS} ${grpc_SRCS})
protobuf_generate(
  TARGET intrinsic_sdk_protos
  LANGUAGE cpp
  IMPORT_DIRS sdk ${googleapis_SOURCE_DIR} ${grpc_gateway_SOURCE_DIR} ${grpc_SOURCE_DIR}
  PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protos_gen
)
target_include_directories(intrinsic_sdk_protos
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/protos_gen>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
target_link_libraries(intrinsic_sdk_protos PUBLIC protobuf::libprotobuf)

set(intrinsic_services_SRCS
  sdk/intrinsic/assets/proto/asset_deployment.proto
  sdk/intrinsic/executive/proto/executive_service.proto
  sdk/intrinsic/frontend/cloud/api/clusterdeletion_api.proto
  sdk/intrinsic/frontend/cloud/api/clusterdiscovery_api.proto
  sdk/intrinsic/frontend/cloud/api/orgdiscovery_api.proto
  sdk/intrinsic/frontend/cloud/api/solutiondiscovery_api.proto
  sdk/intrinsic/frontend/cloud_portal/api/projectdiscovery_api.proto
  sdk/intrinsic/geometry/service/geometry_service.proto
  sdk/intrinsic/hardware/gpio/gpio_service.proto
  sdk/intrinsic/hardware/opcua_equipment/opcua_equipment_service.proto
  sdk/intrinsic/kubernetes/workcell_spec/proto/installer.proto
  sdk/intrinsic/logging/proto/logger_service.proto
  sdk/intrinsic/manipulation/service/grasp_planner_service.proto
  sdk/intrinsic/motion_planning/proto/motion_planner_service.proto
  sdk/intrinsic/perception/service/proto/camera_server.proto
  sdk/intrinsic/platform/file_upload/file_upload_service.proto
  sdk/intrinsic/resources/proto/resource_registry.proto
  sdk/intrinsic/simulation/service/proto/simulation_service.proto
  sdk/intrinsic/skills/internal/proto/behavior_tree_registry_internal.proto
  sdk/intrinsic/skills/internal/proto/skill_registry_internal.proto
  sdk/intrinsic/skills/proto/behavior_tree_registry.proto
  sdk/intrinsic/skills/proto/skill_registry.proto
  sdk/intrinsic/skills/proto/skill_service.proto
  sdk/intrinsic/storage/artifacts/proto/articat.proto
  sdk/intrinsic/util/grpc/testing/ping.proto
  sdk/intrinsic/world/proto/object_world_service.proto
)
add_library(intrinsic_sdk_services
  STATIC
    ${intrinsic_services_SRCS}
    ${googleapis_SOURCE_DIR}/google/longrunning/operations.proto
    ${grpc_SRCS}
)
protobuf_generate(
  TARGET intrinsic_sdk_services
  LANGUAGE grpc
  PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  IMPORT_DIRS sdk ${googleapis_SOURCE_DIR} ${grpc_gateway_SOURCE_DIR} ${grpc_SOURCE_DIR}
  PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protos_gen
  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)
target_include_directories(intrinsic_sdk_services
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/protos_gen>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
target_link_libraries(intrinsic_sdk_services PUBLIC intrinsic_sdk_protos)

add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/intrinsic_proto.desc
      COMMAND protobuf::protoc
      ARGS
        -I${CMAKE_CURRENT_SOURCE_DIR}/sdk
        -I${googleapis_SOURCE_DIR}
        -I${grpc_gateway_SOURCE_DIR}
        -I${grpc_SOURCE_DIR}
        --include_imports
        --descriptor_set_out=${CMAKE_CURRENT_BINARY_DIR}/intrinsic_proto.desc
        ${intrinsic_proto_SRCS}
      DEPENDS ${protobuf_PROTOC_EXE} ${intrinsic_proto_SRCS}
      COMMENT "Generating descriptor set"
      VERBATIM )
add_custom_target(intrinsic_proto_desc ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/intrinsic_proto.desc)


install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/protos_gen/"
        DESTINATION "include/${PROJECT_NAME}"
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/intrinsic_proto.desc"
        DESTINATION "share/${PROJECT_NAME}"
)

add_library(${PROJECT_NAME}
  STATIC
  sdk/intrinsic/assets/id_utils.cc
  sdk/intrinsic/icon/interprocess/binary_futex.cc
  sdk/intrinsic/icon/proto/cart_space_conversion.cc
  sdk/intrinsic/icon/proto/eigen_conversion.cc
  sdk/intrinsic/icon/release/grpc_time_support.cc
  sdk/intrinsic/icon/release/portable/init_xfa_absl.cc
  sdk/intrinsic/icon/utils/log.cc
  sdk/intrinsic/icon/utils/log_internal.cc
  sdk/intrinsic/icon/utils/log_sink.cc
  sdk/intrinsic/icon/utils/realtime_guard.cc
  sdk/intrinsic/icon/utils/realtime_log_sink.cc
  sdk/intrinsic/icon/utils/realtime_status.cc
  sdk/intrinsic/icon/utils/realtime_stack_trace.cc
  sdk/intrinsic/kinematics/types/cartesian_limits.cc
  sdk/intrinsic/kinematics/types/dynamic_limits_check_mode.cc
  sdk/intrinsic/kinematics/types/joint_limits.cc
  sdk/intrinsic/kinematics/types/joint_limits_xd.cc
  sdk/intrinsic/kinematics/types/to_fixed_string.cc
  sdk/intrinsic/kinematics/types/to_string.cc
  sdk/intrinsic/kinematics/validate_link_parameters.cc
  sdk/intrinsic/logging/data_logger_client.cc
  sdk/intrinsic/logging/structured_logging_client.cc
  sdk/intrinsic/math/proto_conversion.cc
  sdk/intrinsic/math/transform_utils.cc
  sdk/intrinsic/math/twist.cc
  sdk/intrinsic/motion_planning/conversions.cc
  sdk/intrinsic/motion_planning/motion_planner_client.cc
  sdk/intrinsic/platform/common/buffers/internal/event_fd.cc
  sdk/intrinsic/platform/common/buffers/realtime_write_queue.cc
  sdk/intrinsic/skills/cc/equipment_pack.cc
  sdk/intrinsic/skills/cc/skill_canceller.cc
  sdk/intrinsic/skills/cc/skill_interface_utils.cc
  sdk/intrinsic/skills/cc/skill_utils.cc
  sdk/intrinsic/skills/internal/default_parameters.cc
  sdk/intrinsic/skills/internal/equipment_utilities.cc
  sdk/intrinsic/skills/internal/error_utils.cc
  sdk/intrinsic/skills/internal/get_footprint_context_impl.cc
  sdk/intrinsic/skills/internal/preview_context_impl.cc
  sdk/intrinsic/skills/internal/runtime_data.cc
  sdk/intrinsic/skills/internal/single_skill_factory.cc
  sdk/intrinsic/skills/internal/skill_init.cc
  sdk/intrinsic/skills/internal/skill_proto_utils.cc
  sdk/intrinsic/skills/internal/skill_registry_client.cc
  sdk/intrinsic/skills/internal/skill_service_config_utils.cc
  sdk/intrinsic/skills/internal/skill_service_impl.cc
  sdk/intrinsic/util/grpc/channel.cc
  sdk/intrinsic/util/grpc/channel_interface.cc
  sdk/intrinsic/util/grpc/connection_params.cc
  sdk/intrinsic/util/grpc/grpc.cc
  sdk/intrinsic/util/page_fault_info.cc
  sdk/intrinsic/util/proto/descriptors.cc
  sdk/intrinsic/util/proto/get_text_proto.cc
  sdk/intrinsic/util/proto/merge.cc
  sdk/intrinsic/util/proto/source_code_info_view.cc
  sdk/intrinsic/util/proto_time.cc
  sdk/intrinsic/util/status/annotate.cc
  sdk/intrinsic/util/status/ret_check.cc
  sdk/intrinsic/util/status/ret_check_grpc.cc
  sdk/intrinsic/util/status/status_builder.cc
  sdk/intrinsic/util/status/status_builder_grpc.cc
  sdk/intrinsic/util/status/status_conversion_grpc.cc
  sdk/intrinsic/util/status/status_conversion_proto.cc
  sdk/intrinsic/util/status/status_conversion_rpc.cc
  sdk/intrinsic/util/thread/lockstep.cc
  sdk/intrinsic/util/thread/thread.cc
  sdk/intrinsic/util/thread/util.cc
  sdk/intrinsic/world/objects/frame.cc
  sdk/intrinsic/world/objects/kinematic_object.cc
  sdk/intrinsic/world/objects/object_entity_filter.cc
  sdk/intrinsic/world/objects/object_world_client.cc
  sdk/intrinsic/world/objects/object_world_client_utils.cc
  sdk/intrinsic/world/objects/object_world_ids.cc
  sdk/intrinsic/world/objects/transform_node.cc
  sdk/intrinsic/world/objects/world_object.cc
  sdk/intrinsic/world/robot_payload/robot_payload.cc
  sdk/intrinsic/world/robot_payload/robot_payload_base.cc
)

target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    Eigen3::Eigen
    absl::log_internal_check_op
    absl::flags_parse
    absl::flags_usage
    absl::log
    absl::time gRPC::grpc++
    intrinsic_sdk_protos intrinsic_sdk_services
)

add_executable(skill_service_config_main
  sdk/intrinsic/skills/internal/skill_service_config_main.cc
)
target_link_libraries(skill_service_config_main
  PUBLIC
  ${PROJECT_NAME}
  intrinsic_sdk_protos
  absl::log absl::status absl::statusor absl::flags absl::flags_usage absl::flags_parse
  re2::re2
)
target_include_directories(skill_service_config_main PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>")

install(
  TARGETS ${PROJECT_NAME} intrinsic_sdk_protos intrinsic_sdk_services skill_service_config_main EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  PROGRAMS
  scripts/textproto_to_binproto.py
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.cmake"
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/sdk/intrinsic"
  DESTINATION "include/${PROJECT_NAME}"
  FILES_MATCHING
  PATTERN "*.h"
)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/sdk/intrinsic" # source directory
  DESTINATION "share/${PROJECT_NAME}/protos" # target directory
  FILES_MATCHING # install only matched files
  PATTERN "*.proto" # select header files
)

ament_export_targets(${PROJECT_NAME})
ament_export_dependencies(grpc_vendor)
ament_export_dependencies(absl)
ament_export_dependencies(Protobuf)
ament_export_dependencies(gRPC)
ament_export_dependencies(Eigen3)

ament_package(CONFIG_EXTRAS cmake/intrinsic_sdk-extras.cmake.in)
