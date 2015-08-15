LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := freetype2
LOCAL_SRC_FILES := \
  freetype2/src/autofit/autofit.c \
  freetype2/src/base/basepic.c \
  freetype2/src/base/ftapi.c \
  freetype2/src/base/ftbase.c \
  freetype2/src/base/ftbbox.c \
  freetype2/src/base/ftbitmap.c \
  freetype2/src/base/ftdbgmem.c \
  freetype2/src/base/ftdebug.c \
  freetype2/src/base/ftglyph.c \
  freetype2/src/base/ftinit.c \
  freetype2/src/base/ftpic.c \
  freetype2/src/base/ftstroke.c \
  freetype2/src/base/ftsynth.c \
  freetype2/src/base/ftsystem.c \
  freetype2/src/cff/cff.c \
  freetype2/src/pshinter/pshinter.c \
  freetype2/src/psnames/psnames.c \
  freetype2/src/raster/raster.c \
  freetype2/src/sfnt/sfnt.c \
  freetype2/src/smooth/smooth.c \
  freetype2/src/truetype/truetype.c

LOCAL_CFLAGS := -g -O4 -DFT2_BUILD_LIBRARY=1
LOCAL_C_INCLUDES := $(LOCAL_PATH)/freetype2/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/freetype2/include
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := gmp
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../gmp/gmp-6.0.0-20141214/.libs/libgmp.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../gmp/gmp-6.0.0-20141214
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := gmpxx
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../gmp/gmp-6.0.0-20141214/.libs/libgmpxx.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../gmp/gmp-6.0.0-20141214
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := mandel-split
LOCAL_SRC_FILES := \
Julia.C \
GmpFixedPoint.C \
main.C \
Job.C \
MandelDrawer.C \
ThreadPool.C \
MyNativeActivity.C \
src/Logger.C \
src/ConfigData.C \
src/GlResourceCache.C \
src/LoadGlProgram.C \
src/MMapArea.C \
src/GlunaticUI/Font.C \
src/GlunaticUI/GuiShader.C \
src/GlunaticUI/HoldingMode.C

LOCAL_STATIC_LIBRARIES := freetype2 gmpxx gmp

LOCAL_LDLIBS    := -llog -landroid -lGLESv2 -lEGL
LOCAL_CPP_EXTENSION := .C
LOCAL_CFLAGS := -DANDROID_LOGGER_TAG=mandel-split -ffast-math -O4 -funroll-loops
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src $(LOCAL_PATH)/boost_1_54_0 $(LOCAL_PATH)/freetype2/include $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)

