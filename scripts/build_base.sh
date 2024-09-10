#!/usr/bin/env bash

IMAGES_DIR=./images
BUILDER_NAME=container-builder

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )


docker build -t ghcr.io/mjcarroll/flowstate_ros_base:latest \
    --file $SCRIPT_DIR/../resources/Dockerfile.base \
    .
