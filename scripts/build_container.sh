#!/usr/bin/env bash

IMAGES_DIR=./images
BUILDER_NAME=container-builder

while [[ $# -gt 0 ]]; do
  case $1 in
    --images_dir)
      IMAGES_DIR="$2"
      shift # past argument
      shift # past value
      ;;
    --builder_name)
      BUILDER_NAME="$2"
      shift # past argument
      shift # past value
      ;;
    --service_name)
      SERVICE_NAME="$2"
      shift # past argument
      shift # past value
      ;;
    --service_package)
      SERVICE_PACKAGE="$2"
      shift # past argument
      shift # past value
      ;;
    --skill_name)
      SKILL_NAME="$2"
      shift # past argument
      shift # past value
      ;;
    --skill_package)
      SKILL_PACKAGE="$2"
      shift # past argument
      shift # past value
      ;;
    --dependencies)
      DEPENDENCIES="$2"
      shift # past argument
      shift # past value
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
done


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )


if [[ -n "$SERVICE_NAME" && -n "$SERVICE_PACKAGE" ]]; then
  mkdir -p $IMAGES_DIR/$SERVICE_NAME

  docker buildx build -t $SERVICE_PACKAGE:$SERVICE_NAME \
      --builder="$BUILDER_NAME" \
      --output="\
        type=docker,\
        dest=./images/$SERVICE_NAME/$SERVICE_NAME.tar,\
        compression=zstd,\
        push=false,\
        name=$SERVICE_PACKAGE:$SERVICE_NAME" \
      --file $SCRIPT_DIR/../resources/Dockerfile.service \
      --build-arg="SERVICE_PACKAGE=$SERVICE_PACKAGE" \
      --build-arg="SERVICE_NAME=$SERVICE_NAME" \
      --build-arg="SERVICE_EXECUTABLE_NAME=${SERVICE_NAME}_main" \
      --build-arg="DEPENDENCIES=${DEPENDENCIES}" \
      .
elif [[ -n "$SKILL_NAME" && -n "$SKILL_PACKAGE" ]]; then
  mkdir -p $IMAGES_DIR/$SKILL_NAME

  docker buildx build -t $SKILL_PACKAGE:$SKILL_NAME \
      --builder="$BUILDER_NAME" \
      --output="\
        type=docker,\
        dest=./images/$SKILL_NAME/$SKILL_NAME.tar,\
        compression=zstd,\
        push=false,\
        name=$SKILL_PACKAGE:$SKILL_NAME" \
      --file $SCRIPT_DIR/../resources/Dockerfile.skill \
      --build-arg="SKILL_PACKAGE=$SKILL_PACKAGE" \
      --build-arg="SKILL_NAME=$SKILL_NAME" \
      .
fi
