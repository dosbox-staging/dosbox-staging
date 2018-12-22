LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
SDL_NET_PATH := ../SDL_net

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
	$(LOCAL_PATH)/$(SDL_NET_PATH) \
	../../../include

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CPPFLAGS += -DC_TARGETCPU=ARMV7LE
endif
ifeq ($(TARGET_ARCH_ABI),armeabi)
	LOCAL_CPPFLAGS += -DC_TARGETCPU=ARMV4LE
endif
ifeq ($(TARGET_ARCH_ABI),x86)
	LOCAL_CPPFLAGS += -DC_TARGETCPU=X86 -DC_FPU_X86=1
endif
ifeq ($(TARGET_ARCH_ABI),mips)
	LOCAL_CPPFLAGS += -DC_TARGETCPU=MIPSEL
endif
# Some sizes for a few types (for now architecture independent but possibly not)
LOCAL_CPPFLAGS += -DSIZEOF_INT_P=4 \
	-DSIZEOF_UNSIGNED_CHAR=1 \
	-DSIZEOF_UNSIGNED_INT=4 \
	-DSIZEOF_UNSIGNED_LONG=4 \
	-DSIZEOF_UNSIGNED_LONG_LONG=8 \
	-DSIZEOF_UNSIGNED_SHORT=2

LOCAL_SRC_PATH := ../../../src

LOCAL_CPP_FEATURES += exceptions

# Note: We don't want any cdrom cpp file differing from cdrom_image.cpp.
# Furthermore, we compile zmbv.cpp only... and even that can't be done
# without zlib. So, let's give up for now.
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(wildcard $(LOCAL_SRC_PATH)/dosbox.cpp) \
	\
	$(wildcard $(LOCAL_SRC_PATH)/cpu/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/cpu/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/core_dynrec/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/core_dynrec/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/core_dyn_x86/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/core_dyn_x86/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/core_full/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/core_full/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/core_normal/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/core_normal/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/debug/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/debug/*.cpp) \
	\
	$(wildcard $(LOCAL_SRC_PATH)/dos/cdrom.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/dos/cdrom_image.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/dos/d*.cpp) \
	\
	$(wildcard $(LOCAL_SRC_PATH)/fpu/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/fpu/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/gui/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/gui/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/hardware/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/hardware/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/hardware/serialport/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/hardware/serialport/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/ints/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/ints/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/libs/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/libs/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/misc/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/misc/*.cpp) \
	$(wildcard $(LOCAL_SRC_PATH)/shell/*.c) \
	$(wildcard $(LOCAL_SRC_PATH)/shell/*.cpp)

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_net

LOCAL_LDLIBS := -lGLESv1_CM -llog

include $(BUILD_SHARED_LIBRARY)
