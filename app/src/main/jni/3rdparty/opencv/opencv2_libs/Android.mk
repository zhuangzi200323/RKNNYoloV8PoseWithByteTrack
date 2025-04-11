LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := opencv2-shared
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/libopencv_java4.so
include $(PREBUILT_SHARED_LIBRARY)