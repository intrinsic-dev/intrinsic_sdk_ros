cmake_minimum_required(VERSION 3.10)
project(grpc_vendor CXX)

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_vendor_package REQUIRED)

ament_vendor(grpc_vendor
  VCS_URL https://github.com/grpc/grpc
  VCS_VERSION v1.65.0
  CMAKE_ARGS
    -DCMAKE_CXX_STANDARD=20
    -DABSL_BUILD_TESTING:BOOL=OFF
    -DABSL_ENABLE_INSTALL:BOOL=ON
    -DABSL_PROPAGATE_CXX_STD:BOOL=ON
    -DRE2_BUILD_TESTING:BOOL=OFF
    -DgRPC_INSTALL:BOOL=ON
    -DgRPC_BUILD_TESTS:BOOL=OFF
    -DgRPC_BUILD_CSHARP_EXT:BOOL=OFF
    -DgRPC_BUILD_GRPC_CSHARP_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_GRPC_NODE_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_GRPC_PHP_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_GRPC_RUBY_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_NODE_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_OBJECTIVEC_PLUGIN:BOOL=OFF
    -DgRPC_BUILD_RUBY_PLUGIN:BOOL=OFF
)

if (TARGET grpc_vendor)
  # Install the gRPC proto definitions so that we can use them elsewhere
  ExternalProject_Get_Property(grpc_vendor SOURCE_DIR)
  install(DIRECTORY "${SOURCE_DIR}/src/proto/grpc" # source directory
          DESTINATION "opt/${PROJECT_NAME}/share/grpc-proto/src/proto/" # target directory
          FILES_MATCHING # install only matched files
          PATTERN "*.proto" # select header files
  )
endif()

ament_package()
