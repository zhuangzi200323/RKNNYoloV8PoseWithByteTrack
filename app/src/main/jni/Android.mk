PROJ_PATH	:= $(call my-dir)
include $(CLEAR_VARS)
include $(PROJ_PATH)/3rdparty/Android.mk
include $(PROJ_PATH)/rknn_yolov8pose/Android.mk
include $(PROJ_PATH)/bytetrack/Android.mk