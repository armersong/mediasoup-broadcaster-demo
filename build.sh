#!/bin/sh

# Note: make WEBRTC correct!
export WEBRTC=/home/armers/projects/webrtc/google/webrtc/webrtc-checkout/src

export LIBWEBRTC_INCLUDE_PATH=$WEBRTC
# Note: make LIBWEBRTC_BINARY_PATH correct!
export LIBWEBRTC_BINARY_PATH=$WEBRTC/out/m84/obj

#cmake . -Bcmake-build-debug \
cmake . -Bbuild \
  -DLIBWEBRTC_INCLUDE_PATH:PATH=$LIBWEBRTC_INCLUDE_PATH \
  -DLIBWEBRTC_BINARY_PATH:PATH=$LIBWEBRTC_BINARY_PATH
