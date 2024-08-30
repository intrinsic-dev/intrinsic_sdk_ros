#!/usr/bin/env bash

IMAGES_DIR=images
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
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
done

mkdir -p $IMAGES_DIR
docker buildx inspect --builder $BUILDER_NAME || \
  docker buildx create --name="$BUILDER_NAME" --driver="docker-container"

echo "images
build
log" > ./.dockerignore
