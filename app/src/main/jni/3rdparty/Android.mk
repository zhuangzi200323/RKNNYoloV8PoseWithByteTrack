THIRD_PARTY_PATH	:= $(call my-dir)
include $(CLEAR_VARS)
include $(THIRD_PARTY_PATH)/jpeg_turbo/Android.mk
include $(THIRD_PARTY_PATH)/librga/Android.mk
include $(THIRD_PARTY_PATH)/rknnrt/Android.mk
include $(THIRD_PARTY_PATH)/opencv/opencv2_libs/Android.mk