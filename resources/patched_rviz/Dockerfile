FROM ghcr.io/sloretz/ros:jazzy-desktop-full

# Hacks, terrible dirty hacks, please avert your eyes...
# Build a patched version of resource retriever and rviz and inject them into
# the existing jazzy workspace.
# Also build and install zenoh 0.7.2 from source.
RUN \
    mkdir -p temp_rviz_ws/src && \
    cd temp_rviz_ws/src && \
    git clone https://github.com/ros/resource_retriever.git -b mjcarroll/plugins && \
    git clone https://github.com/ros2/rviz.git -b wjwwood/jazzy/update_resource_retriever && \
    cd .. && \
    . /opt/ros/jazzy/setup.sh && \
    apt-get update && \
    apt-get install -y clang-14 ros-jazzy-ament-cmake-vendor-package cargo && \
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
    rm -rf temp_rviz_ws && \
    apt install --reinstall ros-jazzy-ros-workspace && \
    rm -rf /var/lib/apt/lists/*

    # git clone https://github.com/eclipse-zenoh/zenoh-plugin-dds \
    #   -b release-0.7.2-rc \
    #   --recurse-submodules && \
    # colcon build \
    #     --packages-up-to zenoh_bridge_dds \
    #     --cmake-args -DCMAKE_C_COMPILER=clang-14 -DCMAKE_CXX_COMPILER=clang++-14 \
    #     --event-handlers=console_direct+ \
    #     --install-base /opt/ros/jazzy \
    #   --merge-install \
    #   --cmake-args \
    #       -DCMAKE_BUILD_TYPE=RelWithDebInfo && \


# Ensure some additional dependencies are installed.
RUN \
    apt-get update && \
    apt-get install -y ros-jazzy-cv-bridge && \
    rm -rf /var/lib/apt/lists/*
