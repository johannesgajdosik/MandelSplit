LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := freetype2
LOCAL_SRC_FILES := \
  ../../android/jni/freetype2/src/autofit/autofit.c \
  ../../android/jni/freetype2/src/base/basepic.c \
  ../../android/jni/freetype2/src/base/ftapi.c \
  ../../android/jni/freetype2/src/base/ftbase.c \
  ../../android/jni/freetype2/src/base/ftbbox.c \
  ../../android/jni/freetype2/src/base/ftbitmap.c \
  ../../android/jni/freetype2/src/base/ftdbgmem.c \
  ../../android/jni/freetype2/src/base/ftdebug.c \
  ../../android/jni/freetype2/src/base/ftglyph.c \
  ../../android/jni/freetype2/src/base/ftinit.c \
  ../../android/jni/freetype2/src/base/ftpic.c \
  ../../android/jni/freetype2/src/base/ftstroke.c \
  ../../android/jni/freetype2/src/base/ftsynth.c \
  ../../android/jni/freetype2/src/base/ftsystem.c \
  ../../android/jni/freetype2/src/cff/cff.c \
  ../../android/jni/freetype2/src/pshinter/pshinter.c \
  ../../android/jni/freetype2/src/psnames/psnames.c \
  ../../android/jni/freetype2/src/raster/raster.c \
  ../../android/jni/freetype2/src/sfnt/sfnt.c \
  ../../android/jni/freetype2/src/smooth/smooth.c \
  ../../android/jni/freetype2/src/truetype/truetype.c

LOCAL_CFLAGS := -g -O3 -DFT2_BUILD_LIBRARY=1
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../android/jni/freetype2/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../android/jni/freetype2/include
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := gmp612
LOCAL_SRC_FILES := /home/johannes/Progs/Android/gmp/gmp-6.1.2/.libs/libgmp.a
LOCAL_EXPORT_C_INCLUDES := /home/johannes/Progs/Android/gmp/gmp-6.1.2
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := gmpxx612
LOCAL_SRC_FILES := /home/johannes/Progs/Android/gmp/gmp-6.1.2/.libs/libgmpxx.a
LOCAL_EXPORT_C_INCLUDES := /home/johannes/Progs/Android/gmp/gmp-6.1.2
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := mandel-split
LOCAL_SRC_FILES := \
../../android/jni/Julia.C \
../../android/jni/GmpFixedPoint.C \
../../android/jni/main.C \
../../android/jni/Job.C \
../../android/jni/MandelDrawer.C \
../../android/jni/ThreadPool.C \
../../android/jni/MyNativeActivity.C \
../../android/jni/src/Logger.C \
../../android/jni/src/ConfigData.C \
../../android/jni/src/GlResourceCache.C \
../../android/jni/src/LoadGlProgram.C \
../../android/jni/src/MMapArea.C \
../../android/jni/src/GlunaticUI/Font.C \
../../android/jni/src/GlunaticUI/GuiShader.C \
../../android/jni/src/GlunaticUI/HoldingMode.C

#LOCAL_STATIC_LIBRARIES := freetype2 gmpxx gmp
LOCAL_STATIC_LIBRARIES := freetype2 gmp612

LOCAL_LDLIBS    := -llog -landroid -lGLESv2 -lEGL
LOCAL_CPP_EXTENSION := .C
LOCAL_CFLAGS := -DANDROID_LOGGER_TAG=mandel-split -ffast-math -g -O3 -funroll-loops --std=c++11
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../android/jni/src $(LOCAL_PATH)/../../android/jni/boost_1_54_0 $(LOCAL_PATH)/../../android/jni/freetype2/include $(LOCAL_PATH)/../../android/jni

include $(BUILD_SHARED_LIBRARY)

