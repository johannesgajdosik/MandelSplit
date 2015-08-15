/*
 * Author and Copyright
 * Johannes Gajdosik, 2013,2014
 *
 *  This file is part of glunatic.
 *
 *  glunatic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  glunatic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with glunatic.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MyNativeActivity.H"
#include "Logger.H"

#include <android/native_activity.h>
//#include <android/looper.h>


#include <stdlib.h>
#include <errno.h>

void MyNativeActivity::callJavaFinish(void) {
  ANativeActivity_finish(activity);
}

int MyNativeActivity::getSdkVersion(void) const {
  return activity->sdkVersion;
}

AAssetManager *MyNativeActivity::getAssetManager(void) const {
  return activity->assetManager;
}

float  MyNativeActivity::getDisplayDensity(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "getDisplayDensity","()F");
  jni_env->DeleteLocalRef(cls_NativeActivity);
  return jni_env->CallFloatMethod(activity->clazz,mid);
}

void MyNativeActivity::setRequestedOrientation(
                         AndroidScreenOrientation x) const {
  JNIEnv *jni_env = activity->env;
  jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                       "setRequestedOrientation","(I)V");
  jni_env->CallVoidMethod(activity->clazz,mid,(int)x);
  jni_env->DeleteLocalRef(cls_NativeActivity);
}

void MyNativeActivity::showMaxIterDialog(int max_iter,
                                         const char *text,
                                         bool info_enabled,
                                         bool turning_enabled,
                                         int color_palette) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "showMaxIterDialog",
                                             "(ILjava/lang/CharSequence;ZZI)V");
  jni_env->DeleteLocalRef(cls_NativeActivity);
  if (mid) {
    jstring j_text = jni_env->NewStringUTF(text);
    jni_env->CallVoidMethod(activity->clazz,mid,max_iter,j_text,
                            info_enabled,turning_enabled,color_palette);
    jni_env->DeleteLocalRef(j_text);
  }
}

void MyNativeActivity::performHapticFeedback(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "performHapticFeedback","()V");
  jni_env->DeleteLocalRef(cls_NativeActivity);
  jni_env->CallVoidMethod(activity->clazz,mid);
}

void MyNativeActivity::showDialog(int id) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "showDialog","(I)V");
  jni_env->DeleteLocalRef(cls_NativeActivity);
  jni_env->CallVoidMethod(activity->clazz,mid,id);
}

void MyNativeActivity::showToast(const char *text,bool long_duration) const {
//  cout << "MyNativeActivity::showToast(" << text
//       << ',' << long_duration << ')' << endl;
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "showToast",
                                             "(Ljava/lang/CharSequence;Z)V");
  if (mid) {
    jstring j_text = jni_env->NewStringUTF(text);
    jni_env->CallVoidMethod(activity->clazz,mid,j_text,long_duration);
    jni_env->DeleteLocalRef(j_text);
  }
}

int MyNativeActivity::getFuncResult(const char *func_name) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                                   func_name,"()I");
  int rval = 0;
  if (mid) {
    rval = jni_env->CallIntMethod(activity->clazz,mid);
  }
  // done automatically?
  jni_env->DeleteLocalRef(cls_NativeActivity);
  return rval;
}


int MyNativeActivity::getDisplayRotation(void) const {
  return activity->env->CallIntMethod(obj_Display, mid_getRotation);
}

int MyNativeActivity::getScreenWidth(void) const {
  return activity->env->CallIntMethod(obj_Display, mid_getWidth);
}

int MyNativeActivity::getScreenHeight(void) const {
  return activity->env->CallIntMethod(obj_Display, mid_getHeight);
}


void MyNativeActivity::callFromJavaThread(const boost::function<void(void)> &f,
                                          int delay_millis) {
    // This function can be called from the java thread only,
    // otherwise GetObjectClass etc.. will segfault terribly.
    // Therefore no mutex is needed.
  int index;
  for (;;) {
//    MutexLock lock(java_cb_map_mutex);
    index = java_cb_map_sequence++;
    boost::function<void(void)> &func(java_cb_map[index]);
    if (!func) {func = f;break;}
  }
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  const jmethodID mid = jni_env->GetMethodID(cls_NativeActivity,
                                             "callFromJavaThread","(II)V");
  jni_env->DeleteLocalRef(cls_NativeActivity);
  jni_env->CallVoidMethod(activity->clazz,mid,delay_millis,index);
}

void MyNativeActivity::calledFromJava(int index) {
  boost::function<void(void)> f;
  {
//    MutexLock lock(java_cb_map_mutex);
    JavaCbMap::iterator it(java_cb_map.find(index));
    if (it == java_cb_map.end()) ABORT();
    f.swap(it->second);
    java_cb_map.erase(it);
  }
  f();
}





std::string MyNativeActivity::getInternalFilesDir(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  jmethodID mid_getFilesDir = jni_env->GetMethodID(cls_NativeActivity,"getFilesDir","()Ljava/io/File;");
  jobject obj_InternalFile = jni_env->CallObjectMethod(activity->clazz,mid_getFilesDir);
  jclass cls_File = jni_env->FindClass("java/io/File");
  jmethodID mid_getPath = jni_env->GetMethodID(cls_File,"getPath","()Ljava/lang/String;");
  jstring obj_InternalPath = (jstring)(jni_env->CallObjectMethod(obj_InternalFile,mid_getPath));
  const char *path = jni_env->GetStringUTFChars(obj_InternalPath,NULL);
  const std::string rval = std::string(path) + "/";
  jni_env->ReleaseStringUTFChars(obj_InternalPath,path);
  jni_env->DeleteLocalRef(obj_InternalPath);
  jni_env->DeleteLocalRef(cls_File);
  jni_env->DeleteLocalRef(obj_InternalFile);
  jni_env->DeleteLocalRef(cls_NativeActivity);
  return rval;
}

std::string MyNativeActivity::getExternalFilesDir(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  jmethodID mid_getExternalFilesDir = jni_env->GetMethodID(cls_NativeActivity,"getExternalFilesDir","(Ljava/lang/String;)Ljava/io/File;");
  jobject obj_ExternalFile = jni_env->CallObjectMethod(activity->clazz,mid_getExternalFilesDir,NULL);
  jclass cls_File = jni_env->FindClass("java/io/File");
  jmethodID mid_getPath = jni_env->GetMethodID(cls_File,"getPath","()Ljava/lang/String;");
  jstring obj_ExternalPath = (jstring) jni_env->CallObjectMethod(obj_ExternalFile,mid_getPath);
  const char *path = jni_env->GetStringUTFChars(obj_ExternalPath,NULL);
  const std::string rval = std::string(path) + "/";
  jni_env->ReleaseStringUTFChars(obj_ExternalPath,path);
  jni_env->DeleteLocalRef(obj_ExternalPath);
  jni_env->DeleteLocalRef(cls_File);
  jni_env->DeleteLocalRef(obj_ExternalFile);
  jni_env->DeleteLocalRef(cls_NativeActivity);
  return rval;
}

std::string MyNativeActivity::getPackageName(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_NativeActivity = jni_env->GetObjectClass(activity->clazz);
  jmethodID mid_getPackageName = jni_env->GetMethodID(cls_NativeActivity,"getPackageName","()Ljava/lang/String;");
  jstring obj_PackageName = (jstring)(jni_env->CallObjectMethod(activity->clazz,mid_getPackageName));
  const char *name = jni_env->GetStringUTFChars(obj_PackageName,NULL);
  const std::string rval = std::string(name);
  jni_env->ReleaseStringUTFChars(obj_PackageName,name);
  jni_env->DeleteLocalRef(obj_PackageName);
  jni_env->DeleteLocalRef(cls_NativeActivity);
  return rval;
}

std::string MyNativeActivity::getExternalStorageDirectory(void) const {
  JNIEnv *jni_env = activity->env;
  const jclass cls_Environment = jni_env->FindClass("android/os/Environment");
  jmethodID mid_getExternalStorageDirectory = jni_env->GetStaticMethodID(cls_Environment,"getExternalStorageDirectory","()Ljava/io/File;");
  jobject obj_ExternalStorage = jni_env->CallStaticObjectMethod(cls_Environment,mid_getExternalStorageDirectory);
  jclass cls_File = jni_env->FindClass("java/io/File");
  jmethodID mid_getPath = jni_env->GetMethodID(cls_File,"getPath","()Ljava/lang/String;");
  jstring obj_ExternalPath = (jstring) jni_env->CallObjectMethod(obj_ExternalStorage,mid_getPath);
  const char *path = jni_env->GetStringUTFChars(obj_ExternalPath,NULL);
  const std::string rval = std::string(path) + "/";
  jni_env->ReleaseStringUTFChars(obj_ExternalPath,path);
  jni_env->DeleteLocalRef(obj_ExternalPath);
  jni_env->DeleteLocalRef(cls_File);
  jni_env->DeleteLocalRef(obj_ExternalStorage);
  jni_env->DeleteLocalRef(cls_Environment);
  return rval;
}


/**
* http://developer.android.com/reference/a...FULLSCREEN
* Possible param values can be found above.
* SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512 (0x00000200)
* SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2 (0x00000002)
* are the most interesting for me
*/
//SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024

//SYSTEM_UI_FLAG_LOW_PROFILE  = 1
//SYSTEM_UI_FLAG_IMMERSIVE = 2048 (ab API level 19=4.3)
//SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096 (ab API level 19=4.3)

void MyNativeActivity::setSystemUiVisibility(int visibility) {

  //setSystemUiVisibility has been in since api 11 http://developer.android.com/reference/a...ility(int)
  //if (CAndroidFeatures::GetVersion() < 11)
  //return;

  JNIEnv *const env = activity->env;
  const jclass cls_Activity = env->GetObjectClass(activity->clazz);
  const jmethodID mid_getWindow = env->GetMethodID(cls_Activity,
                                                   "getWindow",
                                                   "()Landroid/view/Window;");
  const jobject window = env->CallObjectMethod(activity->clazz,mid_getWindow);
  const jclass cls_Window = env->GetObjectClass(window);
  const jmethodID mid_getDecorView = env->GetMethodID(cls_Window,
                                                      "getDecorView",
                                                      "()Landroid/view/View;");
  const jobject decor_view = env->CallObjectMethod(window,mid_getDecorView);
  const jclass cls_View = env->GetObjectClass(decor_view);
  const jmethodID mid_setSystemUiVisibility = env->GetMethodID(
                                                     cls_View,
                                                     "setSystemUiVisibility",
                                                     "(I)V");
  if (mid_setSystemUiVisibility) {
    env->CallVoidMethod(decor_view,mid_setSystemUiVisibility,visibility);
  }
  // done automatically?
  env->DeleteLocalRef(cls_View);
  env->DeleteLocalRef(decor_view);
  env->DeleteLocalRef(cls_Window);
  env->DeleteLocalRef(window);
  env->DeleteLocalRef(cls_Activity);
}

void MyNativeActivity::addWindowFlags(int flags) {
  JNIEnv *const env = activity->env;
  const jclass cls_Activity = env->GetObjectClass(activity->clazz);
  const jmethodID mid_getWindow = env->GetMethodID(cls_Activity,
                                                   "getWindow",
                                                   "()Landroid/view/Window;");
  const jobject window = env->CallObjectMethod(activity->clazz,mid_getWindow);
  const jclass cls_Window = env->GetObjectClass(window);
  const jmethodID mid_addFlags = env->GetMethodID(cls_Window,
                                                  "addFlags",
                                                  "(I)V");
  if (mid_addFlags) {
    env->CallVoidMethod(window,mid_addFlags,flags);
  }
  env->DeleteLocalRef(cls_Window);
  env->DeleteLocalRef(window);
  env->DeleteLocalRef(cls_Activity);
}

static MyNativeActivity *static_my_native_activity = 0;

MyNativeActivity::MyNativeActivity(ANativeActivity *activity)
  : activity(activity),window(0),input_queue(0),java_cb_map_sequence(0),
    continue_looping(true),loop_sem_waiting(false) {
//setSystemUiVisibility(2+512);
  cout << "MyNativeActivity::MyNativeActivity, "
          "SDK version: " << activity->sdkVersion << endl;
  {
    JNIEnv *const jni_env = activity->env;
    const jclass cls_Context = jni_env->FindClass("android/content/Context");
    const jmethodID mid_getSystemService = jni_env->GetMethodID(cls_Context,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
    const jfieldID fid_WINDOW_SERVICE = jni_env->GetStaticFieldID(cls_Context,"WINDOW_SERVICE","Ljava/lang/String;");
    const jobject obj_WINDOW_SERVICE = jni_env->GetStaticObjectField(cls_Context,fid_WINDOW_SERVICE);
    jni_env->DeleteLocalRef(cls_Context);
    const jobject obj_WindowManager = jni_env->CallObjectMethod(activity->clazz, mid_getSystemService, obj_WINDOW_SERVICE);
    jni_env->DeleteLocalRef(obj_WINDOW_SERVICE);
    const jclass cls_WindowManager = jni_env->GetObjectClass(obj_WindowManager);
    const jmethodID mid_getDefaultDisplay = jni_env->GetMethodID(cls_WindowManager, "getDefaultDisplay", "()Landroid/view/Display;");
    jni_env->DeleteLocalRef(cls_WindowManager);
    const jobject obj_Display_local = jni_env->CallObjectMethod(obj_WindowManager, mid_getDefaultDisplay);
    jni_env->DeleteLocalRef(obj_WindowManager);
    obj_Display = jni_env->NewGlobalRef(obj_Display_local);
    jni_env->DeleteLocalRef(obj_Display_local);
    const jclass cls_Display = jni_env->GetObjectClass(obj_Display);
    mid_getRotation = jni_env->GetMethodID(cls_Display, "getRotation", "()I");
    mid_getWidth = jni_env->GetMethodID(cls_Display, "getWidth", "()I");
    mid_getHeight = jni_env->GetMethodID(cls_Display, "getHeight", "()I");
    jni_env->DeleteLocalRef(cls_Display);
  }

  activity->instance = this;
  activity->callbacks->onStart = OnStart;
  activity->callbacks->onResume = OnResume;
  activity->callbacks->onSaveInstanceState = OnSaveInstanceState;
  activity->callbacks->onPause = OnPause;
  activity->callbacks->onStop = OnStop;
  activity->callbacks->onDestroy = OnDestroy;
  activity->callbacks->onConfigurationChanged = OnConfigurationChanged;
  activity->callbacks->onLowMemory = OnLowMemory;
//  activity->callbacks->onWindowFocusChanged = OnWindowFocusChanged;
  activity->callbacks->onNativeWindowCreated = OnNativeWindowCreated;
  activity->callbacks->onNativeWindowDestroyed = OnNativeWindowDestroyed;
  activity->callbacks->onInputQueueCreated = OnInputQueueCreated;
  activity->callbacks->onInputQueueDestroyed = OnInputQueueDestroyed;

  static_my_native_activity = this;
}

MyNativeActivity::~MyNativeActivity(void) {
  JNIEnv *const jni_env = activity->env;
  jni_env->DeleteGlobalRef(obj_Display);obj_Display = 0;
  static_my_native_activity = 0;
  cout << "MyNativeActivity::~MyNativeActivity" << endl;
}

void MyNativeActivity::OnDestroy(ANativeActivity *activity) {
  cout << "OnDestroy" << endl;
  delete ((MyNativeActivity*)(activity->instance));
}

void MyNativeActivity::OnStart(ANativeActivity *activity) {
  cout << "OnStart" << endl;
}

void MyNativeActivity::OnStop(ANativeActivity *activity) {
  cout << "OnStop" << endl;
}

void *MyNativeActivity::OnSaveInstanceState(ANativeActivity *activity,
                                            size_t *outSize) {
  cout << "OnSaveInstanceState" << endl;
  return ((MyNativeActivity*)(activity->instance))->onSaveInstanceState(
                                                      *outSize);
}

void MyNativeActivity::OnResume(ANativeActivity *activity) {
  cout << "OnResume start" << endl;
  ((MyNativeActivity*)(activity->instance))->onResume();
  cout << "OnResume end" << endl;
}

void MyNativeActivity::OnPause(ANativeActivity *activity) {
  cout << "OnPause" << endl;
  ((MyNativeActivity*)(activity->instance))->onPause();
}


void MyNativeActivity::OnNativeWindowCreated(ANativeActivity *activity,
                                             ANativeWindow *window) {
  cout << "OnNativeWindowCreated" << endl;
  ((MyNativeActivity*)(activity->instance))->onNativeWindowCreated(window);
}

void MyNativeActivity::OnNativeWindowResized(ANativeActivity *activity,
                                             ANativeWindow *window) {
  cout << "OnNativeWindowResized" << endl;
  ((MyNativeActivity*)(activity->instance))->onNativeWindowResized(window);
}

void MyNativeActivity::OnNativeWindowRedrawNeeded(ANativeActivity *activity,
                                                  ANativeWindow *window) {
  cout << "OnNativeWindowRedrawNeeded" << endl;
  ((MyNativeActivity*)(activity->instance))->onNativeWindowRedrawNeeded(window);
}

void MyNativeActivity::OnNativeWindowDestroyed(ANativeActivity *activity,
                                               ANativeWindow *window) {
  cout << "OnNativeWindowDestroyed" << endl;
  ((MyNativeActivity*)(activity->instance))->onNativeWindowDestroyed(window);
}

void MyNativeActivity::OnInputQueueCreated(ANativeActivity *activity,
                                           AInputQueue *queue) {
  cout << "OnInputQueueCreated" << endl;
  ((MyNativeActivity*)(activity->instance))->input_queue = queue;
  AInputQueue_attachLooper(queue,
                           ALooper_forThread(), // use looper of main thread
                           0, // ident, not needed
                           &MyNativeActivity::OnProcessInput,
                           activity);
}

int MyNativeActivity::OnProcessInput(int fd,int events,void *data) {
  ANativeActivity *a_native_activity((ANativeActivity*)data);
  MyNativeActivity &my_native_activity(*((MyNativeActivity*)
                                         (a_native_activity->instance)));
  if (events & ALOOPER_EVENT_INPUT) {
    AInputEvent *event = 0;
    if (AInputQueue_getEvent(my_native_activity.input_queue,&event) < 0) {
      cout << "MyNativeActivity::OnProcessInput: "
              "AInputQueue_getEvent failed: " << strerror(errno) << endl;
    } else {
      if (AInputQueue_preDispatchEvent(my_native_activity.input_queue,event)) {
          // nothing to do
      } else {
        const int32_t handled = my_native_activity.onInputEvent(event);
        AInputQueue_finishEvent(my_native_activity.input_queue,event,handled);
      }
    }
  }
    // keep input channel open:
  return 1;
}

void MyNativeActivity::OnInputQueueDestroyed(ANativeActivity *activity,
                                             AInputQueue *queue) {
  cout << "OnInputQueueDestroyed" << endl;
  AInputQueue_detachLooper(queue);
  ((MyNativeActivity*)(activity->instance))->input_queue = 0;
}

void MyNativeActivity::OnContentRectChanged(ANativeActivity *activity,
                                            const ARect *rect) {
  cout << "OnContentRectChanged" << endl;
}

void MyNativeActivity::OnConfigurationChanged(ANativeActivity *activity) {
//  cout << "OnConfigurationChanged" << endl;
  ((MyNativeActivity*)(activity->instance))->onConfigurationChanged();
}

void MyNativeActivity::OnLowMemory(ANativeActivity *activity) {
  cout << "OnLowMemory" << endl;
}

void *MyNativeActivity::Main(void *context) {
  ((MyNativeActivity*)context)->main();
  return 0;
}

void MyNativeActivity::onResume(void) {
}

void MyNativeActivity::onPause(void) {
}

void MyNativeActivity::onNativeWindowCreated(ANativeWindow *w) {
  cout << "MyNativeActivity::onNativeWindowCreated" << endl;
  window = w;
  continue_looping = true;
  if (0 > pthread_create(&thread,0,MyNativeActivity::Main,this)) {
    cout << "onNativeWindowCreated: pthread_create failed" << endl;
    ABORT();
  }
}

void MyNativeActivity::onNativeWindowDestroyed(ANativeWindow *w) {
  cout << "MyNativeActivity::onNativeWindowDestroyed" << endl;
  continue_looping = false;
  loop_sem.post();
  if (0 > pthread_join(thread,0)) {
    cout << "onNativeWindowDestroyed: pthread_join failed" << endl;
    ABORT();
  }
  window = 0;
}

  // implementation of global function
void ReportErrorToUser(const char *text) {
  if (static_my_native_activity) {
    static_my_native_activity->showToast(text,true);
    static_my_native_activity->callJavaFinish();
  }
}

extern "C" {

JNIEXPORT jint JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_minimizeMaxIter(
                         JNIEnv *env,jobject obj) {
  if (static_my_native_activity)
    return static_my_native_activity->minimizeMaxIter();
  else
    return 8;
//  cout << "Java_gajdosik_johannes_MandelSplit_MyNativeActivity_maxIterChanged: "
//       << x << endl;
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_maxIterChanged(
                         JNIEnv *env,jobject obj,
                         jint x) {
  if (static_my_native_activity)
    static_my_native_activity->maxIterChanged(x);
//  cout << "Java_gajdosik_johannes_MandelSplit_MyNativeActivity_maxIterChanged: "
//       << x << endl;
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_maxIterStartStop(
                         JNIEnv *env,jobject obj,
                         jint start_stop) {
  if (static_my_native_activity)
    static_my_native_activity->maxIterStartStop(start_stop);
//  cout << "Java_gajdosik_johannes_MandelSplit_MyNativeActivity_maxIterChanged: "
//       << x << endl;
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_setColorPalette(
                         JNIEnv *env,jobject obj,
                         jint color_palette) {
  if (static_my_native_activity)
    static_my_native_activity->setColorPalette(color_palette);
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_displayInfo(
                         JNIEnv *env,jobject obj,
                         jboolean enable) {
  if (static_my_native_activity)
    static_my_native_activity->displayInfo(enable);
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_enableTurning(
                         JNIEnv *env,jobject obj,
                         jboolean enable) {
  if (static_my_native_activity)
    static_my_native_activity->enableTurning(enable);
}

JNIEXPORT void JNICALL Java_gajdosik_johannes_MandelSplit_MyNativeActivity_calledFromJava(
                         JNIEnv *env,jobject obj,
                         jint user_data) {
//  cout << "Java_gajdosik_johannes_MandelSplit_MyNativeActivity_calledFromJava: "
//       << user_data << endl;
  if (static_my_native_activity)
    static_my_native_activity->calledFromJava(user_data);
}
}
