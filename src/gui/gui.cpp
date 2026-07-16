#include "gui.hpp"

#include "../sound.hpp"
#include <MNFM.h>

GUI::GUI() {
  MwLibraryInit();
  MNFMLibraryInit();

  mWindow = MwVaCreateWidget(MwWindowClass, "", NULL, MwDEFAULT, MwDEFAULT, 690,
                             350, MwNtitle, "Audioglobber", NULL);

  mVertBox = MwVaCreateWidget(MwBoxClass, "", mWindow, 25, 50, 640, 250,
                              MwNorientation, MwVERTICAL, MwNratio, 1, NULL);

  mInputBox =
      MwVaCreateWidget(MwBoxClass, "", mVertBox, 0, 0, 1, 1, MwNratio, 1);
  mInputLabel = MwVaCreateWidget(MwLabelClass, "", mInputBox, 0, 0, 1, 1,
                                 MwNtext, "Input File", NULL);
  mInputPreview = MwVaCreateWidget(MwLabelClass, "", mInputBox, 0, 0, 1, 1,
                                   MwNdisabled, 1, MwNratio, 6, NULL);
  mInputChoose = MwVaCreateWidget(MwButtonClass, "", mInputBox, 0, 0, 1, 1,
                                  MwNtext, "Choose", NULL);

  MwAddUserHandler(mInputChoose, MwNactivateHandler, GUI::file_button_handler,
                   this);

  mScrambleButton = MwVaCreateWidget(MwButtonClass, "", mVertBox, 0, 0, 1, 1,
                                     MwNtext, "Scramble", MwNdisabled, 1, NULL);
  MwAddUserHandler(mScrambleButton, MwNactivateHandler,
                   GUI::scramble_button_handler, this);

  mFrameNumberShower =
      MwVaCreateWidget(MwLabelClass, "", mVertBox, 0, 0, 1, 1, MwNtext,
                       "0 frames decoded", MwNratio, 1, NULL);

  /* seperator */
  MwCreateWidget(MwSeparatorClass, "", mVertBox, 0, 0, 1, 1);

  mControlBox =
      MwVaCreateWidget(MwBoxClass, "", mVertBox, 0, 0, 1, 1, MwNratio, 1);
  mPlayPauseButton =
      MwVaCreateWidget(MwButtonClass, "", mControlBox, 0, 0, 1, 1, MwNtext,
                       "Start", MwNdisabled, 1, NULL);
  MwAddUserHandler(mPlayPauseButton, MwNactivateHandler, GUI::do_play, this);

  mStopButton = MwVaCreateWidget(MwButtonClass, "", mControlBox, 0, 0, 1, 1,
                                 MwNtext, "Stop", MwNdisabled, 1, NULL);
  MwAddUserHandler(mStopButton, MwNactivateHandler, GUI::stop, this);

  /* seperator */
  MwCreateWidget(MwSeparatorClass, "", mVertBox, 0, 0, 1, 1);

  mOptionsBox = MwVaCreateWidget(MwBoxClass, "", mVertBox, 0, 0, 2, 1,
                                 MwNorientation, MwVERTICAL, MwNratio, 5, NULL);
  mOptionsBoxFrameLimit =
      MwCreateWidget(MwBoxClass, "", mOptionsBox, 0, 0, 1, 1);
  mOptionsBoxMinSilentFrames =
      MwCreateWidget(MwBoxClass, "", mOptionsBox, 0, 0, 1, 1);
  mOptionsBoxSilenceThreshold =
      MwCreateWidget(MwBoxClass, "", mOptionsBox, 0, 0, 1, 1);

  mOptionsLabelFrameLimit =
      MwVaCreateWidget(MwLabelClass, "", mOptionsBoxFrameLimit, 0, 0, 640, 1,
                       MwNtext, "Frame Limit", MwNratio, 1, NULL);
  mOptionsLabelMinSilentFrames =
      MwVaCreateWidget(MwLabelClass, "", mOptionsBoxMinSilentFrames, 0, 0, 640,
                       1, MwNtext, "Minimum Silent Frames", MwNratio, 1, NULL);
  mOptionsLabelSilenceThreshold =
      MwVaCreateWidget(MwLabelClass, "", mOptionsBoxSilenceThreshold, 0, 0, 640,
                       1, MwNtext, "Silence Threshold", MwNratio, 1, NULL);

  mOptionsInputFrameLimit = MwVaCreateWidget(
      MwEntryClass, "", mOptionsBoxFrameLimit, 0, 0, 1, 1, NULL);
  mOptionsInputMinSilentFrames =
      MwVaCreateWidget(MwEntryClass, "", mOptionsBoxMinSilentFrames, 0, 0, 1, 1,
                       MwNtext, "3", NULL);
  mOptionsInputSilenceThreshold =
      MwVaCreateWidget(MwEntryClass, "", mOptionsBoxSilenceThreshold, 0, 0, 1,
                       1, MwNtext, "0.01", NULL);
}

void GUI::loop() {
  while (true) {
    MwStep(mWindow);
    if (mDoDecoding) {
      this->scramble_tick();
    }
  }
}

void GUI::scramble_button_handler(MwWidget handle, void *user_data,
                                  void *call_data) {
  GUI *self = (GUI *)user_data;
  int err;

  self->mDecoder.unload();
  self->mDecoder.load(MwGetText(self->mInputPreview, MwNtext));
  self->mDecoder.setup_resampler();

  self->mDecoder.reset_frames();

  self->mConfig = ma_device_config_init(ma_device_type_playback);
  self->mConfig.playback.format = self->mDecoder.out_format;
  self->mConfig.playback.channels = self->mDecoder.out_channels;
  self->mConfig.sampleRate = self->mDecoder.out_sample_rate;
  self->mConfig.dataCallback = snd_callback;
  self->mConfig.pUserData = &self->mDecoder;

  if ((err = ma_device_init(NULL, &self->mConfig, &self->mDevice)) !=
      MA_SUCCESS) {
    fprintf(stderr, "Failed to init playback device, %d\n", err);
  } else {
    self->mDoDecoding = MwTRUE;
    MwVaApply(self->mScrambleButton, MwNdisabled, 1, NULL);
    MwVaApply(self->mInputChoose, MwNdisabled, 1, NULL);
  }
}

void GUI::scramble_tick() {
  int progress = 0;
  char prg[255];
  int frame_limit = -1;
  const char *frame_limit_text = MwGetText(mOptionsInputFrameLimit, MwNtext);
  if (frame_limit_text) {
    try {
      frame_limit = std::stoi(frame_limit_text);
    } catch (std::exception ex) {
    }
  }

  if (mDecoder.decode_once(frame_limit, &progress) == true) {
    double silence_threshold = 0.01;
    int min_silent_frames = 3;
    const char *silence_threshold_text =
        MwGetText(mOptionsInputSilenceThreshold, MwNtext);
    const char *min_silent_frames_text =
        MwGetText(mOptionsInputMinSilentFrames, MwNtext);
    try {
      if (silence_threshold_text) {
        silence_threshold = std::stod(silence_threshold_text);
      }
    } catch (std::exception ex) {
      printf("WARNING: Couldn't parse input for silence threshold (%s); "
             "defaulting to 0.01\n",
             ex.what());
    }
    try {
      if (min_silent_frames_text) {
        min_silent_frames = std::stoi(min_silent_frames_text);
      }
    } catch (std::exception ex) {
      printf("WARNING: Couldn't parse input for  min silent frames (%s); "
             "defaulting to 3\n",
             ex.what());
    }

    printf("%0.2f %d\n", silence_threshold, min_silent_frames);

    mDecoder.finish_decoding(silence_threshold, min_silent_frames);

    mDoDecoding = MwFALSE;

    MwVaApply(mInputChoose, MwNdisabled, 0, NULL);

    MwVaApply(mPlayPauseButton, MwNdisabled, 0, NULL);
    MwVaApply(mStopButton, MwNdisabled, 0, NULL);
    MwVaApply(mScrambleButton, MwNdisabled, 0, NULL);
    MwVaApply(mInputChoose, MwNdisabled, 0, NULL);

    MwForceRender(mWindow);
    MwForceRender(mPlayPauseButton);
    MwForceRender(mStopButton);

    mDecoder.reset();
  }
  snprintf(prg, 255, "%d frames decoded", progress, NULL);
  MwVaApply(mFrameNumberShower, MwNtext, prg, NULL);
}

void GUI::do_play(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  self->mDecoder.reset();

  ma_device_start(&self->mDevice);
};
void GUI::stop(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;

  ma_device_stop(&self->mDevice);
  self->mDecoder.reset();
};
