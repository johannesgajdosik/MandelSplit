/*
    Author and Copyright: Johannes Gajdosik, 2013

    This file is part of MandelSplit.

    MandelSplit is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MandelSplit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MandelSplit.  If not, see <http://www.gnu.org/licenses/>.
*/

/*


~/android/android-ndk-r9/ndk-build -j8 NDK_DEBUG=1
ant debug
adb install -r bin/MandelSplit-debug.apk
adb logcat mandel-split:D *:S DEBUG:I

~/android/android-ndk-r9/ndk-build -j8 && ant debug && adb install -r bin/MandelSplit-debug.apk && adb logcat mandel-split:D *:S

Release:
~/android/android-ndk-r9/ndk-build -j8
ant release
jarsigner -storepass cygrks5j -verbose -sigalg SHA1withRSA -digestalg SHA1 \
-keystore ~/glunatic/glunatic/google_key/johannes-gajdosik-release-key.keystore \
bin/MandelSplit-release-unsigned.apk johannes-gajdosik-google-release
rm bin/MandelSplit-0.1.1.apk
~/android/android-sdk-linux/tools/zipalign -v 4 bin/MandelSplit-release-unsigned.apk bin/MandelSplit-0.1.1.apk
~/android/android-sdk-linux/build-tools/17.0.0/aapt dump badging bin/MandelSplit-0.1.1.apk
adb install -r bin/MandelSplit-0.1.1.apk


*/

#include "MyNativeActivity.H"
#include "MandelDrawer.H"
#include "Vector.H"
#include "Logger.H"

#include "GlResourceCache.H"
#include "GlunaticUI/Font.H"

#include <android/native_activity.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/looper.h>


#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <sys/time.h>
//#include <unistd.h> // sysconf

#include <stdlib.h> // malloc
#include <stdio.h>  // snprintf

#include <sstream>
#include <string>

#define RGBA_TO_COLOR(r,g,b,a) (((((((a)<<8)|(b))<<8)|(g))<<8)|(r))

// compile with --no-exceptions
#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
  void throw_exception(std::exception const &e) {
    cout << "Boost calls boost::throw_exception(\"" << e.what() << "\")"
         << endl; 
    ABORT();
  }
}
#endif

static long long int GetNow(void) {
  struct timeval tv;
  gettimeofday(&tv,0);
  return tv.tv_sec * 1000000LL + tv.tv_usec;
}

class UpfCounter {
public:
  UpfCounter(const char *name) : last_stamp(GetNow()),name(name),
                                 value(0),last_value_stamp(last_stamp) {}
  int getMean(void) const {return value;}
  void step(void) {
    const long long int now = GetNow();
    value = (15*value + (now - last_value_stamp)) / 16;
    last_value_stamp = now;
    count++;
    const unsigned int diff = now - last_stamp;
    if (diff >= 10000000) {
      if (last_stamp) {
        cout << "UpfCounter(" << name << "): "
             << ((diff+(count>>1))/count) << endl;
      }
      last_stamp = now;
      count = 0;
    }
  }
private:
  long long int last_stamp;
  unsigned int count;
  const char *const name;
  int value;
  long long int last_value_stamp;
};

static UpfCounter render_frame_counter(   "frame      ");



static const char *EglErrorToString(EGLint x) {
  switch (x) {
    case EGL_SUCCESS            : return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED    : return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS         : return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC          : return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE      : return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG         : return "EGL_BAD_CONFIG";
    case EGL_BAD_CONTEXT        : return "EGL_BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY        : return "EGL_BAD_DISPLAY";
    case EGL_BAD_MATCH          : return "EGL_BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP  : return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW  : return "EGL_BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER      : return "EGL_BAD_PARAMETER";
    case EGL_BAD_SURFACE        : return "EGL_BAD_SURFACE";
    case EGL_CONTEXT_LOST       : return "EGL_CONTEXT_LOST";
  }
  static char buffer[64];
  snprintf(buffer,sizeof(buffer),"EglError(0x%04x)",x);
  buffer[sizeof(buffer)-1] = '\0';
  return buffer;
}

static const char *GlErrorToString(GLenum x) {
  switch (x) {
    case GL_NO_ERROR         : return "GL_NO_ERROR";
    case GL_INVALID_ENUM     : return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE    : return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
#ifndef GL_ES_VERSION_2_0
    case GL_STACK_OVERFLOW   : return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW  : return "GL_STACK_UNDERFLOW";
#endif
    case GL_OUT_OF_MEMORY    : return "GL_OUT_OF_MEMORY";
  }
  static char buffer[64];
  snprintf(buffer,sizeof(buffer),"GlError(0x%04x)",x);
  buffer[sizeof(buffer)-1] = '\0';
  return buffer;
}


static void CheckEglError(const char *description) {
  const EGLint rc = eglGetError();
  if (rc != EGL_SUCCESS) {
    cout << description << ": " << EglErrorToString(rc) << endl;
    abort();
  }
}

void CheckGlError(const char *description) {
  const GLenum rc = glGetError();
  if (rc != GL_NO_ERROR) {
    cout << description << ": " << GlErrorToString(rc) << endl;
    abort();
  }
}

static void PrintGLString(const char *name,GLenum s) {
  cout << "GL " << name << " = " << glGetString(s) << endl;
}


static
GLuint LoadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    cout << "Could not compile "
                         << ((shaderType==GL_VERTEX_SHADER)?"vertex":"fragment")
                         << "-shader " << pSource << "\n" << buf << endl;
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

static
GLuint CreateProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        CheckGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        CheckGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    cout << "Could not link program:\n" << buf << endl;
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}


static
GLuint CreateUnlinkedProgram(const char* pVertexSource,
                             const char* pFragmentSource) {
    GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        CheckGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        CheckGlError("glAttachShader");
    }
    return program;
}

static bool LinkProgram(GLuint &program) {
    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint bufLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
        if (bufLength) {
            char* buf = (char*) malloc(bufLength);
            if (buf) {
                glGetProgramInfoLog(program, bufLength, NULL, buf);
                cout << "Could not link program:\n" << buf << endl;
                free(buf);
            }
        }
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}



class MNA : public MyNativeActivity {
  struct SavedState {
    double center_re,center_im,size_re_im;
    int max_iter;
  };
public: 
  MNA(ANativeActivity *activity,
      void *savedState,size_t savedStateSize);
private:
  ~MNA(void);
  void initializeFont(const char *font_file,float font_size);
  void *onSaveInstanceState(size_t &outSize);
  int32_t onInputEvent(AInputEvent *event);
  void maxIterChanged(int x);
  void maxIterStartStop(int start_stop);
  void displayInfo(int enable_info) {
    enable_display_info = enable_info;
    draw_once = true;
  }
  void startMouseDrag(void);
  void finishMouseDrag(void);
  void performMouseDrag(void);
  void printText(float pos_x,float pos_y,const char *text) const;
  void main(void);
private:
    // OpenGl
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  GLuint program;
  GLuint contents_texture;
  GLint uniform_loc_factors;

  int pointer_id[2];
  Vector<float,2> pointer_2d[2];
  Vector<double,2> drag_start_pos;
  double drag_start_scale;
  float drag_start_distanceq;
  int max_iter_slider_value;
  MandelDrawer mandel_drawer;
  GlunaticUI::Font font;
  int64_t tap_start_time;
  Vector<float,2> tap_start_pos;
  bool tap_moved;
  bool draw_once;
  bool enable_display_info;
};

extern "C" {
void ANativeActivity_onCreate(ANativeActivity *activity,
                              void *savedState,size_t savedStateSize) {
  new MNA(activity,savedState,savedStateSize);
}
}

void MNA::initializeFont(const char *font_file,float font_size) {
  AAsset *asset = AAssetManager_open(getAssetManager(),
                                     font_file,AASSET_MODE_UNKNOWN);
  if (asset) {
    font.initialize(font_file,
                    AAsset_getBuffer(asset),AAsset_getLength(asset),
                    font_size);
    AAsset_close(asset);
  } else {
    cout << "FATAL: Asset not found: " << font_file << endl;
    abort();
  }
}

MNA::MNA(ANativeActivity *activity,
         void *savedState,size_t savedStateSize)
    :MyNativeActivity(activity),program(0) {
  max_iter_slider_value = 0;
  draw_once = false;
  enable_display_info = true;
  initializeFont("DejaVuSans.ttf",floorf(sqrtf(getScreenHeight())));

#define FLAG_KEEP_SCREEN_ON 0x80
  addWindowFlags(FLAG_KEEP_SCREEN_ON);

  pointer_id[0] = -1;
  pointer_id[1] = -1;

  cout << pthread_self() << " MNA::MNA" << endl;

  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    cout << "eglGetDisplay failed" << endl;
    ABORT();
  }
  EGLint major,minor;
  if (!eglInitialize(display,&major,&minor)) {
    CheckEglError("eglInitialize");
    ABORT();
  }
  cout << pthread_self() << " eglInitialize: version "
       << major << '.' << minor << endl;
  

  const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };
  EGLint numConfigs;
  if (!eglChooseConfig(display,config_attribs,&config,1,&numConfigs)) {
    CheckEglError("eglChooseConfig");
    ABORT();
  }
  if (numConfigs <= 0) {
    cout << "no EGLConfig found" << endl;
    ABORT();
  }

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION,2,
    EGL_NONE
  };
  context = eglCreateContext(display,config,0,context_attribs);
  if (context == EGL_NO_CONTEXT) {
    CheckEglError("eglCreateContext");
    ABORT();
  }

  if (savedState) {
    if (savedStateSize != sizeof(SavedState)) {
      cout << "bad savedStateSize: " << savedStateSize << endl;
      ABORT();
    }
    cout << "resetting from saved instance state" << endl;
    mandel_drawer.reset(((SavedState*)savedState)->center_re,
                        ((SavedState*)savedState)->center_im,
                        ((SavedState*)savedState)->size_re_im,
                        ((SavedState*)savedState)->max_iter);
  }
}

void *MNA::onSaveInstanceState(size_t &outSize) {
  cout << pthread_self() << " MNA::onSaveInstanceState" << endl;
  outSize = sizeof(SavedState);
  SavedState *rval = (SavedState*)malloc(sizeof(SavedState));
  rval->center_re = mandel_drawer.getCenterRe();
  rval->center_im = mandel_drawer.getCenterIm();
  rval->size_re_im = mandel_drawer.getSizeReIm();
  rval->max_iter = mandel_drawer.getMaxIter();
  return rval;
}

MNA::~MNA(void) {
  cout << pthread_self() << " MNA::~MNA" << endl;
  if (!eglTerminate(display)) {
    CheckEglError("eglTerminate w");
  }
  display = EGL_NO_DISPLAY;
  context = EGL_NO_CONTEXT;
}

int32_t MNA::onInputEvent(AInputEvent *event) {
  switch (AInputEvent_getType(event)) {
    case AINPUT_EVENT_TYPE_KEY:
      switch (AKeyEvent_getAction(event)) {
        case AKEY_EVENT_ACTION_DOWN:
          switch (AKeyEvent_getKeyCode(event)) {
            case AKEYCODE_VOLUME_UP:
              cout << "increase max iter" << endl;
              return 1;
            case AKEYCODE_VOLUME_DOWN:
              cout << "decrease max iter" << endl;
              return 1;
            default:
              break;
          }
            // do not handle
          break;
        case AKEY_EVENT_ACTION_UP: {
          const int flags = AKeyEvent_getFlags(event);
          if (flags & (AKEY_EVENT_FLAG_WOKE_HERE |
                       AKEY_EVENT_FLAG_CANCELED |
                       AKEY_EVENT_FLAG_LONG_PRESS |
                       AKEY_EVENT_FLAG_CANCELED_LONG_PRESS |
                       AKEY_EVENT_FLAG_TRACKING)) {
            break;
          }
          switch (AKeyEvent_getKeyCode(event)) {
            case AKEYCODE_BACK:
              showDialog(0);
              return 1;
                // the system needs this for handling the "back" button,
                // do not handle
              break;
            case AKEYCODE_MENU:
              showMaxIterDialog(mandel_drawer.getMaxIter(),
                                mandel_drawer.getCenterRe(),
                                mandel_drawer.getCenterIm(),
                                mandel_drawer.XYToReImScale(),
                                enable_display_info);
//              cout << "here comes my menu" << endl;
              return 1;
          }
        } break;
        case AKEY_EVENT_ACTION_MULTIPLE:
            // not interested, do not handle
          break;
      }
        // do not handle: the system needs this for handling the "back" button
      return 0;
    case AINPUT_EVENT_TYPE_MOTION: {
      const int32_t action_and_index = AMotionEvent_getAction(event);
      const int32_t action = action_and_index & AMOTION_EVENT_ACTION_MASK;
      switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
          const size_t p_index
            = (action_and_index & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
          const int32_t p_id
            = AMotionEvent_getPointerId(event,p_index);
          if (pointer_id[0] >= 0) {
            if (pointer_id[1] >= 0) {
              // not interested in 3rd or 4th finger
            } else {
              pointer_id[1] = p_id;
              pointer_2d[1][0] = AMotionEvent_getX(event,p_index);
              pointer_2d[1][1] = AMotionEvent_getY(event,p_index);
              startMouseDrag();
            }
            tap_moved = true;
//            cout << "no tap because of second pointer" << endl;
          } else {
            pointer_id[0] = p_id;
            pointer_2d[0][0] = AMotionEvent_getX(event,p_index);
            pointer_2d[0][1] = AMotionEvent_getY(event,p_index);
            startMouseDrag();
            tap_moved = false;
            tap_start_time = AMotionEvent_getEventTime(event);
            tap_start_pos = pointer_2d[0];
//            cout << "tap started: " << tap_start_pos << endl;
          }
        } break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
          const size_t p_index
            = (action_and_index & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
          const int32_t p_id
            = AMotionEvent_getPointerId(event,p_index);
          if (p_id == pointer_id[0]) {
            if (pointer_id[1] < 0 && !tap_moved &&
                AMotionEvent_getEventTime(event) <
                  tap_start_time + 600*1000000LL) {
                  showMaxIterDialog(mandel_drawer.getMaxIter(),
                                    mandel_drawer.getCenterRe(),
                                    mandel_drawer.getCenterIm(),
                                    mandel_drawer.XYToReImScale(),
                                    enable_display_info);
            } else {
              pointer_2d[0][0] = AMotionEvent_getX(event,p_index);
              pointer_2d[0][1] = AMotionEvent_getY(event,p_index);
            }
            pointer_id[0] = -1;
            finishMouseDrag();
          } else if (p_id == pointer_id[1]) {
            pointer_id[1] = -1;
            pointer_2d[1][0] = AMotionEvent_getX(event,p_index);
            pointer_2d[1][1] = AMotionEvent_getY(event,p_index);
            finishMouseDrag();
          }
        } break;
        case AMOTION_EVENT_ACTION_MOVE: {
          const size_t pointer_count = AMotionEvent_getPointerCount(event);
          for (size_t p_index=0;p_index<pointer_count;p_index++) {
            const int32_t p_id = AMotionEvent_getPointerId(event,p_index);
            if (p_id == pointer_id[0]) {
              const Vector<float,2> curr_pos(AMotionEvent_getX(event,p_index),
                                             AMotionEvent_getY(event,p_index));
              if (tap_moved) {
                pointer_2d[0] = curr_pos;
              } else if ((curr_pos-tap_start_pos).length2() >= 1.4f) {
                tap_moved = true;
                pointer_2d[0] = curr_pos;
              }
            } else if (p_id == pointer_id[1]) {
              pointer_2d[1][0] = AMotionEvent_getX(event,p_index);
              pointer_2d[1][1] = AMotionEvent_getY(event,p_index);
            }
          }
          if (tap_moved) performMouseDrag();
        } break;
      }
    } return 1; // handled
    default:
      break;
  }
  return 0;
}

void MNA::startMouseDrag(void) {
  if (pointer_id[0] >= 0) {
    if (pointer_id[1] >= 0) {
      mandel_drawer.XYToReIm(0.5f*(pointer_2d[0]+pointer_2d[1]),drag_start_pos);
      drag_start_distanceq = (pointer_2d[0]-pointer_2d[1]).length2();
    } else {
      mandel_drawer.XYToReIm(pointer_2d[0],drag_start_pos);
    }
  } else {
    mandel_drawer.XYToReIm(pointer_2d[1],drag_start_pos);
  }
  drag_start_scale = mandel_drawer.XYToReImScale();
}

void MNA::finishMouseDrag(void) {
  if (pointer_id[0] >= 0 || pointer_id[1] >= 0) {
    startMouseDrag();
  } else {
    mandel_drawer.startRecalc();
  }
}

void MNA::performMouseDrag(void) {
  if (pointer_id[0] >= 0) {
    if (pointer_id[1] >= 0) {
      const float dq = (pointer_2d[0]-pointer_2d[1]).length2();
      mandel_drawer.fitReIm(
                      (pointer_2d[0]+pointer_2d[1])*0.5f,drag_start_pos,
                      drag_start_scale*sqrtf(drag_start_distanceq/dq));
//      double h = mandel_drawer.XYToReImScale();
  //    h = -log(h);
  //    double hs = sqrt(h);
  //    h = h*h*h*h;

//      h = sqrt(10.0/h);
//      int n = (int)(h);
//      mandel_drawer.setMaxIter(200+n);
    } else {
      mandel_drawer.fitReIm(pointer_2d[0],drag_start_pos);
    }
  } else {
    mandel_drawer.fitReIm(pointer_2d[1],drag_start_pos);
  }
}

void MNA::maxIterChanged(int x) {
  max_iter_slider_value = x;
}

void MNA::maxIterStartStop(int start_stop) {
  if (!start_stop) {
    mandel_drawer.setMaxIter(max_iter_slider_value);
    mandel_drawer.startRecalc();
  }
}



static const char vertex_shader[] =
    "precision highp float;\n"
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texcoor;\n"
    "varying vec2 v_texcoor;\n"
    "void main() {\n"
    "  gl_Position = a_position;\n"
    "  v_texcoor = a_texcoor;\n"
    "}\n";
/*
static const char fragment_shader1[] =
    "precision mediump float;\n"
//    "precision lowp float;\n"
    "uniform sampler2D contents_texture;\n"
    "uniform vec4 factors;\n"
    "varying vec2 v_texcoor;\n"
    "void main() {\n"
    "  float v = dot(vec3(1.0,256.0,65536.0),"
                    "texture2D(contents_texture,v_texcoor).rgb);\n"
    "  gl_FragColor = (v>=factors.a) ? vec4(0.0,0.0,0.0,0.0)"
                                   " : fract(v*factors);\n"
    "}\n";
*/
static const char fragment_shader[] =
    "precision mediump float;\n"
//    "precision lowp float;\n"
    "uniform sampler2D contents_texture;\n"
    "uniform vec4 factors;\n"
    "varying vec2 v_texcoor;\n"
    "void main() {\n"
    "  const vec4 potences = vec4(4.0/256.0,4.0,4.0*256.0,0.0);\n"
    "  vec4 v = texture2D(contents_texture,v_texcoor);\n"
    "  float val = dot(potences,v);\n"
    "  v *= potences;\n"
    "  gl_FragColor = (val >= factors.a)\n"
                  " ? vec4(0.0,0.0,0.0,0.0)\n"
                  " : fract(fract(v.rrra*factors)"
                          "+fract(v.ggga*factors)"
                          "+fract(v.bbba*factors));\n"
    "}\n";


void MNA::printText(float pos_x,float pos_y,const char *text) const {
  const float size_x = font.getWidth(text);
  const float size_y = font.getHeight();
  font.render(pos_x,pos_y,size_x,size_y,
              text,RGBA_TO_COLOR(192,192,192,192));
}

static const GLfloat texture_coordinates[8] = {
  0.f,0.f,
  1.f,0.f,
  0.f,1.f,
  1.f,1.f
};

void MNA::main(void) {
  cout << "MNA::main: begin" << endl;
  /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
   * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
   * As soon as we picked a EGLConfig, we can safely reconfigure the
   * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
  EGLint egl_native_visual_id;
  if (!eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,
                          &egl_native_visual_id)) {
    CheckEglError("eglGetConfigAttrib(EGL_NATIVE_VISUAL_ID)");
    ABORT();
  }

  ANativeWindow_setBuffersGeometry(getNativeWindow(),0,0,egl_native_visual_id);
  EGLSurface surface = eglCreateWindowSurface(display,config,getNativeWindow(),
                                              NULL);
  if (surface == EGL_NO_SURFACE) {
    CheckEglError("eglCreateWindowSurface");
    ABORT();
  }
  if (eglMakeCurrent(display,surface,surface,context) == EGL_FALSE) {
    CheckEglError("eglMakeCurrent");
    ABORT();
  }
  EGLint width = 0;
  if (!eglQuerySurface(display,surface,EGL_WIDTH,&width)) {
    CheckEglError("eglQuerySurface EGL_WIDTH");
  }
  EGLint height = 0;
  if (!eglQuerySurface(display,surface,EGL_HEIGHT,&height)) {
    CheckEglError("eglQuerySurface EGL_HEIGHT");
  }

  const GLuint position_location = 0;
  const GLuint texcoor_location = 1;
  if (program) {
    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D,contents_texture);
  } else {
    mandel_drawer.initialize(width,height);
    cout << "MNA::main: initializing OpenGL resources" << endl;
    IntrusivePtr<GlResourceCache> resource_cache = GlResourceCache::create();
    font.initializeDrawing(*resource_cache);
    PrintGLString("Version", GL_VERSION);


    int range[2],precision;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_LOW_FLOAT,
                               range,&precision);
    cout << "VERTEX_SHADER,LOW_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_MEDIUM_FLOAT,
                               range,&precision);
    cout << "VERTEX_SHADER,MEDIUM_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_HIGH_FLOAT,
                               range,&precision);
    cout << "VERTEX_SHADER,HIGH_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_LOW_INT,
                               range,&precision);
    cout << "VERTEX_SHADER,LOW_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_MEDIUM_INT,
                               range,&precision);
    cout << "VERTEX_SHADER,MEDIUM_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_HIGH_INT,
                               range,&precision);
    cout << "VERTEX_SHADER,HIGH_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;


    glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_LOW_FLOAT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,LOW_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_MEDIUM_FLOAT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,MEDIUM_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_HIGH_FLOAT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,HIGH_FLOAT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_LOW_INT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,LOW_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_MEDIUM_INT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,MEDIUM_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_HIGH_INT,
                               range,&precision);
    cout << "FRAGMENT_SHADER,HIGH_INT"
            ": [" << range[0] << ',' << range[1] << "], " << precision << endl;

/*
returns the range and precision for different numeric formats supported by the
shader compiler. shadertype must be VERTEX_SHADER or FRAGMENT_SHADER.
precisiontype must be one of LOW_FLOAT, MEDIUM_FLOAT, HIGH_FLOAT, LOW_-
INT, MEDIUM_INT or HIGH_INT. range points to an array of two integers in which
encodings of the format’s numeric range are returned. If min and max are the
smallest and largest values representable in the format, then the values returned are
defined to be
                             range[0] = log2 (|min|)
                             range[1] = log2 (|max|)
precision points to an integer in which the log2 value of the number of bits of
precision of the format is returned. If the smallest representable value greater than
1 is 1 + , then *precision will contain −log2 ( ) , and every value in the range
                                [−2range[0] , 2range[1] ]
can be represented to at least one part in 2∗precision . For example, an IEEE single-
precision floating-point format would return range[0] = 127, range[1] = 127,
and ∗precision = 23, while a 32-bit twos-complement integer format would re-
turn range[0] = 31, range[1] = 30, and ∗precision = 0.
*/

    program = CreateUnlinkedProgram(vertex_shader,fragment_shader);
    if (!program) {
      cout << "Could not create program." << endl;
      ABORT();
    }
    glBindAttribLocation(program,position_location,"a_position");
    glBindAttribLocation(program,texcoor_location,"a_texcoor");
    CheckGlError("glBindAttribLocation");
    if (!LinkProgram(program)) {
      cout << "linking failed." << endl;
      ABORT();
    }
    const GLint uniform_loc_contents_texture
                  = glGetUniformLocation(program,"contents_texture");
    uniform_loc_factors = glGetUniformLocation(program,"factors");
    CheckGlError("glGetUniformLocation");

    glUseProgram(program);
    glUniform1i(uniform_loc_contents_texture,0); // texture unit 0
    CheckGlError("glUseProgram");

    glGenTextures(1,&contents_texture);
    glBindTexture(GL_TEXTURE_2D,contents_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,
                 0,GL_RGBA,GL_UNSIGNED_BYTE,0);
    {
      unsigned char *data = new unsigned char[width*height*4];
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      0,0,width,height,
                      GL_RGBA,GL_UNSIGNED_BYTE,
                      data);
      delete[] data;
    }
    CheckGlError("contents_texture");
cout << "glTexImage2D ok: " << width << 'x' << height << endl;
  }


  glDisable(GL_DEPTH_TEST);

  glClearColor(0,0,0,1);
  CheckGlError("glClearColor");

CheckGlError("main 100");

  glViewport(0,0,width,height);
  CheckGlError("glViewport");
  glDisable(GL_CULL_FACE);

  cout << "MNA::main: starting loop" << endl;
  bool first_time = true;
  while (continue_looping) {
    CheckGlError("start_loop");
    if (!(mandel_drawer.step() || draw_once)) {
      if (pointer_id[0] < 0 && pointer_id[1] < 0) {
        if (!first_time) {
            // busy waiting
          usleep(30000);
          continue;
        }
      }
    }
    draw_once = false;
    first_time = false;
    CheckGlError("mandel_drawer.step");
//    cout << "MNA::main: rendering" << endl;

//    render_frame_counter.step();
    glClear(GL_COLOR_BUFFER_BIT);
    CheckGlError("glClear");
    glDisable(GL_BLEND);

    {
      const float h = 1.f/cbrtf(mandel_drawer.getMaxIter());
      const GLfloat color_factors[4] = {
        64.f*256.f/mandel_drawer.getMaxIter(),
        64.f*256.f*h*h,
        64.f*256.f*h,
        (mandel_drawer.getMaxIter()-0.5)*(1.f/(64.f*255.9f))
      };
      glUniform4fv(uniform_loc_factors,1,color_factors);
    }
    glEnableVertexAttribArray(position_location);
    glEnableVertexAttribArray(texcoor_location);
    GLfloat entire_screen[8];
    mandel_drawer.getOpenGLScreenCoordinates(entire_screen);
    glVertexAttribPointer(position_location,2,GL_FLOAT,GL_FALSE,0,
                          entire_screen);
    glVertexAttribPointer(texcoor_location,2,GL_FLOAT,GL_FALSE,0,
                          texture_coordinates);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    CheckGlError("glDrawArrays");
    glDisableVertexAttribArray(texcoor_location);
    glDisableVertexAttribArray(position_location);

    const float progress = mandel_drawer.getProgress();
    if (enable_display_info || progress < 1.f) {
      font.prepareDrawing(width,height,GlunaticUI::mode_portrait);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
      if (enable_display_info) {
        const float h = floorf(1.2f*font.getHeight());
        char tmp[128];
        snprintf(tmp,sizeof(tmp),"max iter: %d",
                 mandel_drawer.getMaxIter());
        printText(10,6+2*h,tmp);
        const double pixel_size = mandel_drawer.XYToReImScale();
        snprintf(tmp,sizeof(tmp),"size: %4.2e x %4.2e",
                 width*pixel_size,
                 height*pixel_size);
        printText(10,6+h,tmp);
        const int prec = -(int)floor(log10(pixel_size));
        snprintf(tmp,sizeof(tmp),"center: %*.*f +i* %*.*f",
                 prec+2,prec,
                 mandel_drawer.getCenterRe(),
                 prec+2,prec,
                 mandel_drawer.getCenterIm());
        printText(10,6,tmp);
      }
      if (progress < 1.f) {
        font.rectangle(0,height*0.95f,width*progress,height*0.05f,
                       RGBA_TO_COLOR(128,96,32,192));
      }
      font.finishDrawing();
      glBindTexture(GL_TEXTURE_2D,contents_texture);
      glUseProgram(program);
    }

    if (!eglSwapBuffers(display,surface)) {
      CheckEglError("eglSwapBuffers");
    }

  }

  eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  surface = EGL_NO_SURFACE;
  cout << "MNA::main: end" << endl;
}

