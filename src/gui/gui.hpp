#pragma once

#include <Mw/Milsko.h>

#include "../decoder.hpp"

class GUI {
  MwWidget mWindow;
  MwWidget mVertBox;

  MwWidget mInputBox;
  MwWidget mInputLabel;
  MwWidget mInputPreview;

  MwWidget mScrambleButton;

  MwWidget mPlayStopButton;
  MwWidget mExportButton;
  MwWidget mFileButton;

  MwWidget mStatusBar;

  MwWidget mFileChooser;

  MwWidget mOptionsBox;
  MwWidget mOptionsBoxFrameLimit;
  MwWidget mOptionsLabelFrameLimit;
  MwWidget mOptionsInputFrameLimit;
  MwWidget mOptionsBoxSilenceThreshold;
  MwWidget mOptionsLabelSilenceThreshold;
  MwWidget mOptionsInputSilenceThreshold;
  MwWidget mOptionsBoxMinSilentFrames;
  MwWidget mOptionsLabelMinSilentFrames;
  MwWidget mOptionsInputMinSilentFrames;

  Decoder mDecoder;
  ma_device_config mConfig;
  ma_device mDevice;

  MwBool mDoDecoding = MwFALSE;
  MwBool mPlaying = MwFALSE;

  MwLLPixmap mPlayImage;
  MwLLPixmap mStopImage;
  MwLLPixmap mSaveImage;
  MwLLPixmap mOpenImage;

  void setup_icons(MwWidget handle);

public:
  GUI();
  void loop();

  static void file_handler(MwWidget handle, void *user_data, void *call_data);
  static void filesave_handler(MwWidget handle, void *user_data,
                               void *call_data);
  static void file_button_handler(MwWidget handle, void *user_data,
                                  void *call_data);
  static void scramble_button_handler(MwWidget handle, void *user_data,
                                      void *call_data);
  static void error_box_ok(MwWidget handle, void *user, void *call);

  void start_dbus_filechooser(MwUserHandler handler);

  void scramble_tick();
  // static void play_tick(MwWidget handle, void *user_data, void *call_data);

  static void play_stop(MwWidget handle, void *user_data, void *call_data);
  static void save_button_handler(MwWidget handle, void *user_data,
                                  void *call_data);
};
