#pragma once

#include <Mw/Milsko.h>

#include "../decoder.hpp"

#ifdef __linux__
#include <dbus/dbus.h>
struct file_chooser_handler_ctx {
  int status;
  MwUserHandler callback;
  class GUI *gui;
  void (*onError)(std::string err, void *ud);
  void *onError_ud;
};
#endif

class GUI {
  MwWidget mWindow;
  MwWidget mVertBox;

  MwWidget mInputBox;
  MwWidget mInputLabel;
  MwWidget mInputPreview;
  MwWidget mInputChoose;

  MwWidget mScrambleButton;

  MwWidget mControlBox;
  MwWidget mPlayPauseButton;
  MwWidget mStopButton;

  MwWidget mFrameNumberShower;

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

public:
  GUI();
  void loop();

  static void file_handler(MwWidget handle, void *user_data, void *call_data);
  static void file_button_handler(MwWidget handle, void *user_data,
                                  void *call_data);
  static void scramble_button_handler(MwWidget handle, void *user_data,
                                      void *call_data);
  void start_dbus_filechooser(MwUserHandler handler);

  void scramble_tick();
  // static void play_tick(MwWidget handle, void *user_data, void *call_data);

  static void do_play(MwWidget handle, void *user_data, void *call_data);
  static void stop(MwWidget handle, void *user_data, void *call_data);
};
