#!/bin/sh

# Note: make WEBRTC correct!
export WEBRTC=/home/armers/projects/webrtc/google/webrtc/webrtc-checkout/src

export LIBWEBRTC_INCLUDE_PATH=$WEBRTC
# Note: make LIBWEBRTC_BINARY_PATH correct!
export LIBWEBRTC_BINARY_PATH=$WEBRTC/out/m84-arm64r/obj

cmake . -Bbuild-arm64 \
  -DLIBWEBRTC_INCLUDE_PATH:PATH=$LIBWEBRTC_INCLUDE_PATH \
  -DLIBWEBRTC_BINARY_PATH:PATH=$LIBWEBRTC_BINARY_PATH \
  -DBUILD_CPR_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE=arm64.cmake \
  -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -g"
