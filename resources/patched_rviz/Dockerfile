FROM ros:jazzy

WORKDIR /opt/ros/underlay

ENV ROS_HOME=/tmp
ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

# Build and install zenoh-plugins-dds with zenoh 0.7.2 from source.
RUN \
    mkdir -p temp_ws/src && \
    cd temp_ws/src && \
    git clone https://github.com/eclipse-zenoh/zenoh-plugin-dds \
        -b release-0.7.2-rc \
        --recurse-submodules && \
    cd .. && \
    apt-get update && \
    apt-get install -y clang-14 ros-jazzy-ament-cmake-vendor-package cargo && \
    . /opt/ros/jazzy/setup.sh && \
    colcon build \
        --packages-up-to zenoh_bridge_dds \
        --cmake-args -DCMAKE_C_COMPILER=clang-14 -DCMAKE_CXX_COMPILER=clang++-14 \
        --event-handlers=console_direct+ \
        --install-base /opt/ros/underlay \
        --merge-install \
        --cmake-args \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTING:BOOL=OFF && \
    cd .. && \
    rm -rf temp_ws && \
    rm -rf /var/lib/apt/lists/*

# Install some dependencies for the next build step.
RUN \
    apt-get update && \
    apt-get install -y ros-jazzy-desktop-full ros-jazzy-rmw-cyclonedds-cpp && \
    rm -rf /var/lib/apt/lists/*

# Build a patched version of resource retriever and rviz and inject them into
# the existing jazzy workspace.
RUN \
    mkdir -p temp_ws/src && \
    cd temp_ws/src && \
    git clone https://github.com/ros/resource_retriever.git -b mjcarroll/plugins && \
    git clone https://github.com/ros2/rviz.git -b wjwwood/jazzy/update_resource_retriever && \
    cd .. && \
    apt-get update && \
    . /opt/ros/jazzy/setup.sh && \
    rosdep update && \
    rosdep install --from-paths ./src --ignore-src --rosdistro jazzy -y && \
    colcon build \
        --packages-above resource_retriever resource_retriever_msgs rviz_common rviz_default_plugins rviz_rendering \
        --event-handlers console_direct+ \
        --install-base /opt/ros/jazzy \
        --merge-install \
        --cmake-args \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DBUILD_TESTING:BOOL=OFF && \
    cd .. && \
    rm -rf temp_ws && \
    apt-get install --reinstall ros-jazzy-ros-workspace && \
    rm -rf /var/lib/apt/lists/*

# Ensure some additional dependencies are installed.
RUN \
    apt-get update && \
    apt-get install -y ros-jazzy-cv-bridge && \
    rm -rf /var/lib/apt/lists/*

# Setup ROS workspace for use with the intrinsic_sdk_ros in it and built.
RUN mkdir -p /opt/ws_fs_ros/src/intrinsic_sdk_ros
ADD grpc_vendor /opt/ws_fs_ros/src/intrinsic_sdk_ros/grpc_vendor
ADD intrinsic_sdk /opt/ws_fs_ros/src/intrinsic_sdk_ros/intrinsic_sdk
RUN \
    cd /opt/ws_fs_ros && \
    apt-get update && \
    . /opt/ros/jazzy/setup.sh && \
    rosdep update && \
    rosdep install --from-paths ./src --ignore-src --rosdistro jazzy -y && \
    colcon build \
        --event-handlers console_direct+ \
        --merge-install \
        --cmake-args \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DBUILD_TESTING:BOOL=OFF && \
    rm -rf /var/lib/apt/lists/*
RUN chmod -R 777 /opt/ws_fs_ros

# Modify the entrypoint script to run the zenoh dds bridge in the background and source the underlay.
RUN \
    sed --in-place \
        --expression \
            '$isource "/opt/ros/underlay/setup.bash"' \
        /ros_entrypoint.sh && \
    sed --in-place \
        --expression \
            '5 a/opt/ros/underlay/lib/zenoh_bridge_dds/zenoh_bridge_dds -m client -e tcp/localhost:17447 --no-multicast-scouting &' \
        /ros_entrypoint.sh
