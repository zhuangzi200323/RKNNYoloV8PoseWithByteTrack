LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

CFLAGS := -Werror

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/ \
		$(LOCAL_PATH)/../ \
		$(LOCAL_PATH)/../3rdparty \
		$(LOCAL_PATH)/../3rdparty/opencv \

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -ldl
LOCAL_LDLIBS += -llog
LOCAL_LDLIBS += -landroid
LOCAL_LDLIBS += -ljnigraphics

LOCAL_SHARED_LIBRARIES += rknnrt-shared \
        bytetrack \
        opencv2-shared

LOCAL_STATIC_LIBRARIES += rga-static
LOCAL_STATIC_LIBRARIES += turbojpeg-static

LOCAL_MODULE    := rknn_yolov8pose
LOCAL_SRC_FILES :=  \
	utils/file_utils.c \
	utils/image_drawing.c \
	utils/image_utils.c \
	main.cc \
	postprocess.cc \
	rknn_yolov8_jni.cc \
	yolov8-pose.cc \

include $(BUILD_SHARED_LIBRARY)