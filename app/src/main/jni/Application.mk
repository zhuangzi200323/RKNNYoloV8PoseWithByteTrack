APP_PLATFORM := android-16
APP_ABI := armeabi-v7a arm64-v8a

#将libc++_shared.so包入apk中，libopencv_java4.so会使用它
APP_STL := c++_shared

#APP_OPTIM := debug
APP_OPTIM := release

APP_CPPFLAGS += -fexceptions -frtti