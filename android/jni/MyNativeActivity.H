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

#ifndef MY_NATIVE_ACTIVITY_H_
#define MY_NATIVE_ACTIVITY_H_

#include <pthread.h>

#include <string>

#include <jni.h>

class ANativeActivity;
class ANativeWindow;
class AInputEvent;
class AInputQueue;
class ARect;
struct AAssetManager;

enum AndroidScreenOrientation {
  SCREEN_ORIENTATION_UNSPECIFIED = -1,
  SCREEN_ORIENTATION_LANDSCAPE = 0,
  SCREEN_ORIENTATION_PORTRAIT = 1,
  SCREEN_ORIENTATION_USER = 2,
  SCREEN_ORIENTATION_BEHIND = 3,
  SCREEN_ORIENTATION_SENSOR = 4,
  SCREEN_ORIENTATION_NOSENSOR = 5,
  SCREEN_ORIENTATION_SENSOR_LANDSCAPE = 6,
  SCREEN_ORIENTATION_SENSOR_PORTRAIT = 7,
  SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 8,
  SCREEN_ORIENTATION_REVERSE_PORTRAIT = 9,
  SCREEN_ORIENTATION_FULL_SENSOR = 10,
  SCREEN_ORIENTATION_USER_LANDSCAPE = 11,
  SCREEN_ORIENTATION_USER_PORTRAIT = 12,
  SCREEN_ORIENTATION_FULL_USER = 13,
  SCREEN_ORIENTATION_LOCKED = 14,
};

class MyNativeActivity {
public: 
  MyNativeActivity(ANativeActivity *activity);
  virtual ~MyNativeActivity(void);
//  const std::string &internalFilesDir(void) const {return internal_files_dir;}
//  const std::string &externalFilesDir(void) const {return external_files_dir;}
  std::string getInternalFilesDir(void) const;
  std::string getExternalFilesDir(void) const;
  std::string getPackageName(void) const;
  std::string getExternalStorageDirectory(void) const;
  std::string getExpansionFileDir(const std::string &package_name) const {
    return getExternalStorageDirectory()+"Android/obb/"+package_name;
  }

  ANativeWindow *getNativeWindow(void) const {return window;}
  AAssetManager *getAssetManager(void) const;
  int getSdkVersion(void) const;
  void callJavaFinish(void);
  void setRequestedOrientation(AndroidScreenOrientation x) const;
  void showMaxIterDialog(int curr_value,
                         double center_re,double center_im,
                         double d_re_im,
                         int info_enabled) const;
  void showDialog(int id) const;
  void showToast(const char *text,bool long_duration) const;
  int getFuncResult(const char *func_name) const;
  void setSystemUiVisibility(int visibility);
  void addWindowFlags(int flags);
  virtual void maxIterChanged(int x) {}
  virtual void maxIterStartStop(int start_stop) {}
  virtual void displayInfo(int enable_info) {}
protected:
  virtual void onResume(void);
  virtual void onPause(void);
private:
  virtual void *onSaveInstanceState(size_t &outSize) {outSize=0;return 0;}
  virtual int32_t onInputEvent(AInputEvent *event)
    {return 0;}
  virtual void main(void) = 0;
  virtual void onConfigurationChanged(void) {}
  void onNativeWindowCreated(ANativeWindow *w);
  void onNativeWindowResized(ANativeWindow *w) {}
  void onNativeWindowRedrawNeeded(ANativeWindow *w) {}
  void onNativeWindowDestroyed(ANativeWindow *w);
private:
  static void OnDestroy(ANativeActivity *activity);
  static void OnStart(ANativeActivity *activity);
  static void OnStop(ANativeActivity *activity);
  static void *OnSaveInstanceState(ANativeActivity *activity,
                                   size_t *outSize);
  static void OnResume(ANativeActivity *activity);
  static void OnPause(ANativeActivity *activity);
  static void OnNativeWindowCreated(ANativeActivity *activity,
                                    ANativeWindow *window);
  static void OnNativeWindowResized(ANativeActivity *activity,
                                    ANativeWindow *window);
  static void OnNativeWindowRedrawNeeded(ANativeActivity *activity,
                                         ANativeWindow *window);
  static void OnNativeWindowDestroyed(ANativeActivity *activity,
                                      ANativeWindow *window);
  static void OnInputQueueCreated(ANativeActivity *activity,
                                  AInputQueue *queue);
  static int OnProcessInput(int fd,int events,void *data);
  static void OnInputQueueDestroyed(ANativeActivity *activity,
                                    AInputQueue *queue);
  static void OnContentRectChanged(ANativeActivity *activity,
                                   const ARect *rect);
  static void OnConfigurationChanged(ANativeActivity *activity);
  static void OnLowMemory(ANativeActivity *activity);
  static void *Main(void *context);
protected:
  ANativeActivity *const activity;
private:
  ANativeWindow *window;
  AInputQueue *input_queue;
  pthread_t thread;
    // jni global references
  jobject obj_Display;
  jmethodID mid_getRotation;
  jmethodID mid_getWidth;
  jmethodID mid_getHeight;
protected:
  bool continue_looping;
};

#endif
