# Flowstate ROS Bridge

There are two main ways to deploy the `flowstate_ros_bridge`:
1. **Running locally**: Build and run the bridge locally, and test it with a physical connection to a Flowstate workcell over your local LAN.
2. **Sideloading to Flowstate**: Build a Docker image and deploy the process directly as a service container within Flowstate.

## Option 1: Running in a local environment

### Building the flowstate_ros_bridge locally

Before we can build the `flowstate_ros_bridge`, you must follow the [Getting Started](https://github.com/intrinsic-ai/sdk-ros/blob/main/README.md#getting-started) instructions to set up your local SDK-ROS environment.

Source ROS and build the `flowstate_ros_bridge` within your workspace:
```bash
source /opt/ros/jazzy/setup.bash
cd ~/intrinsic_ws/
colcon build \
  --cmake-args -DCMAKE_BUILD_TYPE=Release \
  --event-handlers=console_direct+ \
  --packages-up-to flowstate_ros_bridge
```

### Testing the flowstate_ros_bridge (over local LAN)

Start a zenoh router to connect to the in-cluster router of the Flowstate IPC.

```bash
source /opt/ros/jazzy/setup.bash
# Replace $IPC_ADDRESS with the IP address of the IPC
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/$IPC_ADDRESS:17447"]'
ros2 run rmw_zenoh_cpp rmw_zenohd
```

In a separate terminal, ensure `flowstate_ros_bridge` topics are being published by listing ROS 2 topics.

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 topic list --no-daemon
```

To quickly look at the TFs being published you can export the TF tree using `tf2_tools`.

```bash
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 run tf2_tools view_frames
```

## Option 2: Sideloading to Flowstate as a service

### Building the flowstate_ros_bridge bundle (Docker)

Similar to how a Flowstate service that uses ROS is built, a couple of scripts are provided to build and bundle the flowstate_ros_bridge. The following steps will assume the `sdk-ros` repository is in the `src` folder of your workspace.

In the root of a colcon workspace, install the bundle build CLI tool:

```bash
pip install -e ./src/sdk-ros/intrinsic_sdk_bundle_library_py
```

Then, create the bundle with the `build_service_bundle.sh` script. This will compile the packages in a docker container and bundle that container in a tarball.

```bash
./src/sdk-ros/flowstate_ros_bridge/scripts/build_service_bundle.sh --ros_distro jazzy
```

> [!NOTE]
> Replace `jazzy` with the target ROS 2 distro for building `flowstate_ros_bridge` (e.g. `jazzy` or `lyrical`). 

The output of this command will be a tarball inside the `intrinsic_asset_bundles` directory of the colcon workspace which can be pushed to Flowstate as a new service.

### Sideloading the flowstate_ros_bridge to a running Flowstate solution

With a solution open in Flowstate, the generated service bundle can be sideloaded with `inctl`.

```bash
./inctl service install ./intrinsic_asset_bundles/flowstate_ros_bridge/flowstate_ros_bridge.bundle.tar --org $ORG --cluster $CLUSTER # replace with your org and cluster
```

## Documentation

* [Flowstate Robot State and Sensor ROS Bridge:](docs/robot_state_sensor.md) Explains the translation of internal Flowstate data (joint states, force/torque) into standard ROS 2 messages, with setup and testing instructions.
* [Rapid Object Visualization Integration Test:](test/README.md) Detailed instructions and CLI options for running the high load 3D mesh marker RViz test (`test_rapid_object_visualization.py`).

