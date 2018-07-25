/*
    Author and Copyright: Johannes Gajdosik, 2014,2017

    This file is part of MandelSplit64.

    MandelSplit64 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MandelSplit64 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MandelSplit64.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Author and Copyright
 * Johannes Gajdosik, 2014
 *
 */

package gajdosik.johannes.MandelSplit64;

import android.app.NativeActivity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.app.Dialog;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.SeekBar;
import android.widget.Button;
import android.widget.ToggleButton;
import android.widget.CompoundButton;
import android.widget.Toast;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.WindowManager;
import android.view.Window;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.HapticFeedbackConstants;
import android.view.Gravity;

import android.graphics.drawable.ColorDrawable;
import android.graphics.Color;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.os.Handler;
//import android.os.Vibrator;
import android.util.Log;
import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.graphics.drawable.ColorDrawable;
import java.lang.Math;
import java.util.Formatter;

import java.lang.reflect.*;


public class MyNativeActivity extends NativeActivity {
  

  private static final int MIN_ITER = (1<<3);
  private static final int MAX_ITER = (1<<20);
  private static final double MIN_ITER4
    = (double)MIN_ITER*(double)MIN_ITER*(double)MIN_ITER*(double)MIN_ITER;
  private static final double MAX_ITER4
    = (double)MAX_ITER*(double)MAX_ITER*(double)MAX_ITER*(double)MAX_ITER;
  private static int Decode(int max_iter) {
    return (int)Math.floor(0.5+Math.sqrt(Math.sqrt(
                                 MIN_ITER4
                                   +(MAX_ITER4-MIN_ITER4)
                                   *(max_iter-MIN_ITER)/(MAX_ITER-MIN_ITER)
                                 ))) - MIN_ITER;
  }
  private static int Encode(int x) {
    double x4 = MIN_ITER + x; // MIN_ITER <= x4 <= MAX_ITER
    x4 *= x4;
    x4 *= x4; // MIN_ITER4 <= x4 <= MAX_ITER4
    int max_iter = MIN_ITER
                 +(int)Math.floor(0.5
                         +(x4-MIN_ITER4)
                         *((MAX_ITER-MIN_ITER)/(MAX_ITER4-MIN_ITER4)));
      // use only the 10 higest bits
    int highest_bit = 0;
    for (int i=max_iter;i>1;i>>=1) {highest_bit++;}
    if (highest_bit > 9) {max_iter &= (1023<<(highest_bit-9));}
    return max_iter;
  }

    // works, but needs permission
//  public void vibrate(long milli_seconds) {
//    Vibrator v = (Vibrator)getSystemService(Context.VIBRATOR_SERVICE);
//    v.vibrate(40);
//  }

  private final class ColorPaletteButton extends Button {
    public ColorPaletteButton(int color_palette_) {
      super(MyNativeActivity.this);
      color_palette = color_palette_;
      setText(color_label[color_palette]);
      setOnClickListener(
        new Button.OnClickListener() {
          @Override public void onClick(View v) {
            color_palette++;
            if (color_palette > 5) color_palette = 0;
            setText(color_label[color_palette]);
            MyNativeActivity.setColorPalette(color_palette);
          }
        });
    }
    private int color_palette;
    private String[] color_label = {
                       "BGR\n",
                       "BRG\n",
                       "RBG\n",
                       "RGB\n",
                       "GRB\n",
                       "GBR\n"
                     };
  };

  private final class MaxIterDialog extends Dialog {
    public MaxIterDialog(int max_iter,
                         CharSequence text,
                         boolean info_enabled,
                         boolean turning_enabled,
                         int color_palette) {
      super(MyNativeActivity.this);
      final LinearLayout main_layout = new LinearLayout(MyNativeActivity.this);
      main_layout.setOrientation(LinearLayout.VERTICAL);
      text_view = new TextView(MyNativeActivity.this);
      text_part_1 = "max iterations: ";
      text_part_2 = text;
      text_view.setText(text_part_1+max_iter+text_part_2);

      seek_bar = new SeekBar(MyNativeActivity.this);
      seek_bar.setMax(MAX_ITER-MIN_ITER);
      seek_bar.setOnSeekBarChangeListener(
                 new SeekBar.OnSeekBarChangeListener() {
                   @Override
                   public void onProgressChanged(SeekBar seekBar,int progress,
                                                 boolean fromUser) {
                     final int x = Encode(progress);
                       // you can call a nonstatic function like this:
                       // MyNativeActivity.this.nonstaticmenberfunc();
                     MyNativeActivity.maxIterChanged(x);
               //Log.i("mandel-split","onProgressChanged " + x);
                     text_view.setText(MaxIterDialog.this.text_part_1+
                                       x+
                                       MaxIterDialog.this.text_part_2);
                   }
                   public void onStartTrackingTouch(SeekBar seekBar) {
                     MyNativeActivity.maxIterStartStop(1);
                   }
                   public void onStopTrackingTouch(SeekBar seekBar) {
//                     seekBar.performHapticFeedback(
//                               HapticFeedbackConstants.LONG_PRESS);
                     MyNativeActivity.maxIterStartStop(0);
                     dismiss();
                   }
                 });
      seek_bar.setProgress(Decode(max_iter));
      main_layout.addView(seek_bar);

      LinearLayout line = new LinearLayout(MyNativeActivity.this);
      line.setOrientation(LinearLayout.HORIZONTAL);
      line.setLayoutParams(
              new LinearLayout.LayoutParams(
                                 LinearLayout.LayoutParams.MATCH_PARENT,
                                 LinearLayout.LayoutParams.WRAP_CONTENT));
      line.addView(text_view);

      main_layout.addView(line);
      
      line = new LinearLayout(MyNativeActivity.this);
      line.setOrientation(LinearLayout.HORIZONTAL);
      line.setLayoutParams(
              new LinearLayout.LayoutParams(
                                 LinearLayout.LayoutParams.MATCH_PARENT,
                                 LinearLayout.LayoutParams.WRAP_CONTENT));

      ToggleButton toggle = new ToggleButton(MyNativeActivity.this);
      toggle.setTextOff("Info");
      toggle.setTextOn("Info");
      toggle.setChecked(info_enabled);
      toggle.setOnCheckedChangeListener(
               new CompoundButton.OnCheckedChangeListener() {
                 @Override
                 public void onCheckedChanged(CompoundButton buttonView,
                                              boolean isChecked) {
//                   buttonView.performHapticFeedback(
//                                HapticFeedbackConstants.LONG_PRESS);
                   MyNativeActivity.displayInfo(isChecked);
                 }
               });
      line.addView(toggle);

      toggle = new ToggleButton(MyNativeActivity.this);
      toggle.setTextOff("Turning");
      toggle.setTextOn("Turning");
      toggle.setChecked(turning_enabled);
      toggle.setOnCheckedChangeListener(
               new CompoundButton.OnCheckedChangeListener() {
                 @Override
                 public void onCheckedChanged(CompoundButton buttonView,
                                              boolean isChecked) {
//                   buttonView.performHapticFeedback(
//                                HapticFeedbackConstants.LONG_PRESS);
                   MyNativeActivity.enableTurning(isChecked);
                   dismiss();
                   //vibrate(40);
                 }
               });
      line.addView(toggle);

      Button button = new Button(MyNativeActivity.this);
      button.setText("minimize\nmax iter");
      button.setOnClickListener(
               new Button.OnClickListener() {
                 @Override
                 public void onClick(View v) {
                   int max_iter = MyNativeActivity.minimizeMaxIter();
                   text_view.setText(text_part_1+
                                     max_iter+
                                     text_part_2);
                   seek_bar.setProgress(Decode(max_iter));
                   dismiss();
                 }
               });
      line.addView(button);

      button = new ColorPaletteButton(color_palette);
      line.addView(button);

      button = new Button(MyNativeActivity.this);
      button.setText("Reset\n");
      button.setOnClickListener(
               new Button.OnClickListener() {
                 @Override
                 public void onClick(View v) {
                   MyNativeActivity.resetParams();
                   dismiss();
                 }
               });
      line.addView(button);

      LinearLayout spacer = new LinearLayout(MyNativeActivity.this);
      spacer.setOrientation(LinearLayout.HORIZONTAL);
      spacer.setLayoutParams(
               new LinearLayout.LayoutParams(
                                  LinearLayout.LayoutParams.WRAP_CONTENT,
                                  LinearLayout.LayoutParams.WRAP_CONTENT,
                                  1.f));
      line.addView(spacer);

      button = new Button(MyNativeActivity.this);
      button.setText("Help\n");
      button.setOnClickListener(
               new Button.OnClickListener() {
                 @Override
                 public void onClick(View v) {
//                   v.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                   Builder builder = new AlertDialog.Builder(MyNativeActivity.this);
                   builder.setMessage(R.string.help_dialog_msg);
                   AlertDialog dialog = builder.create();
                   dialog.show();
//          Intent browserIntent =  new Intent(Intent.ACTION_VIEW,Uri.parse("http://www.mkyong.com"));
//          startActivity(browserIntent);
                   dismiss();
                 }
               });
      line.addView(button);

      button = new Button(MyNativeActivity.this);
      button.setText("About\n");
      button.setOnClickListener(
               new Button.OnClickListener() {
                 @Override
                 public void onClick(View v) {
//                   v.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                   Builder builder = new AlertDialog.Builder(MyNativeActivity.this);
                   builder.setIcon(R.drawable.ic_launcher);
                   String dialog_title = getResources().getString(R.string.about_dialog_title);
                   try {
                     PackageManager manager = getPackageManager();
                     PackageInfo info = manager.getPackageInfo(getPackageName(),0);
                     dialog_title += (" version " + info.versionName);
                   } catch (Exception e) {}
                   builder.setTitle(dialog_title);
                   builder.setMessage(R.string.about_dialog_msg);
                   AlertDialog dialog = builder.create();
                   dialog.show();
                   ((TextView)dialog.findViewById(android.R.id.message))
                     .setMovementMethod(LinkMovementMethod.getInstance());
                   dismiss();
                 }
               });
      line.addView(button);

      main_layout.addView(line);

      requestWindowFeature(Window.FEATURE_NO_TITLE);
      setContentView(main_layout);
      final Window window = getWindow();
      window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
      window.setBackgroundDrawable(new ColorDrawable(0x80202020));
    }
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
      boolean rval = super.dispatchTouchEvent(ev);
      if (!rval) {
        dismiss();
      }
      return true;
    }
    private SeekBar seek_bar;
    private TextView text_view;
    private final String text_part_1;
    private final CharSequence text_part_2;
  };
  
  public float getDisplayDensity() {
    DisplayMetrics metrics = new DisplayMetrics();
    ((WindowManager)getApplicationContext()
      .getSystemService(Context.WINDOW_SERVICE))
      .getDefaultDisplay().getMetrics(metrics);
// sgs2: density =1.5, scaledDensity=1.5, densityDpi=240, widthPixels=800, heightPixels=480, xdpi=217.71428, ydpi=218.49463
// sgtabs: density =2.0, scaledDensity=2.0, densityDpi=320, widthPixels=2560, heightPixels=1600, xdpi=359.646, ydpi=361.244
// sgs3mini(236dpi): density =1.5, scaledDensity=1.5, densityDpi=240, widthPixels=800, heightPixels=480, xdpi=160.42105, ydpi=160.0
// "android": density =1.5, scaledDensity=1.5, densityDpi=240, widthPixels=800, heightPixels=480, xdpi=240.0, ydpi=240.0
// cubeu30gt2: density =1.5, scaledDensity=1.5, densityDpi=240, widthPixels=1920, heightPixels=1128, xdpi=225.77777, ydpi=224.73732
    float rval = (metrics.xdpi+metrics.ydpi)/320;
    if (rval < metrics.density || 2*metrics.density < rval) {
      rval = metrics.density;
    }
    if (metrics.scaledDensity > metrics.density)
    rval *= (metrics.scaledDensity/metrics.density);
    Log.i("mandel-split",
          "DisplayMetrics: "
          + "density ="+metrics.density
          +", scaledDensity="+metrics.scaledDensity
          +", densityDpi="+metrics.densityDpi
          +", widthPixels="+metrics.widthPixels
          +", heightPixels="+metrics.heightPixels
          +", xdpi="+metrics.xdpi
          +", ydpi="+metrics.ydpi
          +", heuristic density="+rval
         );
    return rval;
  }

  public int getScreenWidth() {
    return ((WindowManager)getApplicationContext()
              .getSystemService(Context.WINDOW_SERVICE))
             .getDefaultDisplay()
             .getWidth();
  }

//  public int getScreenHeight() {
//    return ((WindowManager)getApplicationContext()
//              .getSystemService(Context.WINDOW_SERVICE))
//             .getDefaultDisplay()
//             .getHeight();
//  }

  public void showMaxIterDialog(int max_iter,
                                CharSequence text,
                                boolean info_enabled,
                                boolean turning_enabled,
                                int color_palette) {
    final MaxIterDialog dialog
      = new MaxIterDialog(max_iter,text,
                          info_enabled,turning_enabled,color_palette);
    dialog.show();
      // must come after dialog.show:
    WindowManager.LayoutParams params = dialog.getWindow().getAttributes();
    params.width = (MyNativeActivity.this.getScreenWidth()*7)/8;
    dialog.getWindow().setAttributes(params);
  }

  private static final int DIALOG_QUIT = 0;
  private static final int DIALOG_ABOUT = 1;
  private static final int DIALOG_HELP = 2;

  @Override
  protected Dialog onCreateDialog(int id) {
    switch (id) {
      case DIALOG_QUIT: {
        Builder builder = new AlertDialog.Builder(this);
        builder.setMessage("Really quit?");
        builder.setCancelable(true);
        builder.setPositiveButton("Ok",
                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                      MyNativeActivity.this.finish();
                    }
                  });
        builder.setNegativeButton("Cancel",
                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {}
                  });
        builder.setNeutralButton("Menu",
                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                      MyNativeActivity.showMaxIterDialog();
                    }
                  });
        AlertDialog dialog = builder.create();
        dialog.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        dialog.show();
      } break;
      case DIALOG_ABOUT: {
        Builder builder = new AlertDialog.Builder(this);
//        builder.setIcon(R.drawable.ic_launcher);
        builder.setTitle(R.string.about_dialog_title);
        builder.setMessage(R.string.about_dialog_msg);
        AlertDialog dialog = builder.create();
        dialog.show();
        ((TextView)dialog.findViewById(android.R.id.message))
          .setMovementMethod(LinkMovementMethod.getInstance());
        break;
      }
      case DIALOG_HELP: {
        Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(R.string.help_dialog_msg);
        AlertDialog dialog = builder.create();
        dialog.show();
        break;
      }
    }
    return super.onCreateDialog(id);
  }

  public void showToast(CharSequence text,boolean long_duration) {
//Log.i("mandel-split","showToast: long_duration="+long_duration);
    Toast toast = Toast.makeText(this,text,
                                 long_duration ? Toast.LENGTH_LONG
                                               : Toast.LENGTH_SHORT);
    toast.show();
  }

  private final class CallFromJavaThreadRunnable implements Runnable {
    public CallFromJavaThreadRunnable(int ud) {
      user_data = ud;
    }
    @Override public void run() {
      MyNativeActivity.this.calledFromJava(user_data);
    }
    final int user_data;
  };
  private final class LongPressTimeoutRunnable implements Runnable {
    @Override public void run() {MyNativeActivity.this.longPressTimeout();}
  };
  private Runnable long_press_timeout_runnable;

  public void callFromJavaThread(int delay_millis,int user_data) {
//    Log.i("mandel-split","callFromJavaThread:"+delay_millis+","+user_data);
    if (delay_millis > 0) {
      call_from_java_thread_handler.postDelayed(
        new CallFromJavaThreadRunnable(user_data),
        delay_millis);
    } else {
      call_from_java_thread_handler.post(
        new CallFromJavaThreadRunnable(user_data));
    }
  }
  public void resetLongPressTimeout(int delay_millis) {
    call_from_java_thread_handler.removeCallbacks(long_press_timeout_runnable);
    call_from_java_thread_handler.postDelayed(long_press_timeout_runnable,
                                              delay_millis);
  }
  private Handler call_from_java_thread_handler;

  public void performHapticFeedback() {
    haptic_view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
  }
  private View haptic_view;
  
  @Override protected void onCreate(Bundle icicle) {
    super.onCreate(icicle);
    haptic_view = new View(this);
    haptic_view.setHapticFeedbackEnabled(true);
    setContentView(haptic_view);
    long_press_timeout_runnable = new LongPressTimeoutRunnable();
    call_from_java_thread_handler = new Handler();
  }

  static {
    System.loadLibrary("mandel-split");
  }
  public static native int minimizeMaxIter();
  public static native void showMaxIterDialog();
  public static native void maxIterChanged(int x);
  public static native void maxIterStartStop(int start_stop);
  public static native void setColorPalette(int color_palette);
  public static native void displayInfo(boolean enable_info);
  public static native void enableTurning(boolean enable_rotation);
  public static native void calledFromJava(int user_data);
  public static native void longPressTimeout();
  public static native void resetParams();
}
