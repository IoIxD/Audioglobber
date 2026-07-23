#include "gui.hpp"

#include "../sound.hpp"
#include <MNFM.h>


GUI::GUI() {
  MwSizeHints hints;

  hints.min_width = hints.max_width = 560;
  hints.min_height = hints.max_height = 225;

  MwLibraryInit();
  MNFMLibraryInit();

  mWindow = MwVaCreateWidget(MwWindowClass, NULL, NULL, MwDEFAULT, MwDEFAULT,
                             hints.min_width, hints.min_height, MwNtitle,
                             "Audioglobber", MwNsizeHints, &hints, MwNacceptsDnD, MwTRUE,
                             NULL);

  MwAddUserHandler(mWindow, MwNdragAndDropHandler, GUI::drag_and_drop, this);

  MwStep(mWindow);

  this->setup_icons(mWindow);

  mVertBox = MwVaCreateWidget(MwBoxClass, NULL, mWindow, 25, 25,
                              hints.max_width - 80, hints.max_height - 50,
                              MwNorientation, MwVERTICAL, MwNratio, 1, NULL);

  mInputBox =
      MwVaCreateWidget(MwBoxClass, NULL, mVertBox, 0, 0, 1, 1, MwNratio, 1);
  mInputLabel = MwVaCreateWidget(MwLabelClass, NULL, mInputBox, 0, 0, 1, 1,
                                 MwNtext, "Input File:", NULL);
  mInputPreview = MwVaCreateWidget(MwLabelClass, NULL, mInputBox, 0, 0, 1, 1,
                                   MwNdisabled, 1, MwNratio, 6, NULL);

  mScrambleButton = MwVaCreateWidget(MwButtonClass, NULL, mVertBox, 0, 0, 1, 1,
                                     MwNtext, "Scramble", MwNdisabled, 0, NULL);
  MwAddUserHandler(mScrambleButton, MwNactivateHandler,
                   GUI::scramble_button_handler, this);

  mExportButton =
      MwVaCreateWidget(MwButtonClass, NULL, mWindow, 480 + 32, 34 + 25, 34, 34,
                       MwNpixmap, mSaveImage, NULL);
  MwAddUserHandler(mExportButton, MwNactivateHandler, GUI::save_button_handler,
                   this);
  mFileButton = MwVaCreateWidget(MwButtonClass, NULL, mWindow, 480 + 32, 25, 34,
                                 34, MwNpixmap, mOpenImage, NULL);
  MwAddUserHandler(mFileButton, MwNactivateHandler, GUI::file_button_handler,
                   this);
  mPlayStopButton =
      MwVaCreateWidget(MwButtonClass, NULL, mWindow, 480 + 32, 34 + 34 + 25, 34,
                       34, MwNpixmap, mPlayImage, NULL);
  MwAddUserHandler(mPlayStopButton, MwNactivateHandler, GUI::play_stop, this);

  mOptionsBox = MwVaCreateWidget(MwBoxClass, NULL, mVertBox, 0, 0, 2, 1,
                                 MwNorientation, MwVERTICAL, MwNratio, 5, NULL);
  mOptionsBoxFrameLimit =
      MwCreateWidget(MwBoxClass, NULL, mOptionsBox, 0, 0, 1, 1);
  mOptionsBoxMinSilentFrames =
      MwCreateWidget(MwBoxClass, NULL, mOptionsBox, 0, 0, 1, 1);
  mOptionsBoxSilenceThreshold =
      MwCreateWidget(MwBoxClass, NULL, mOptionsBox, 0, 0, 1, 1);

  mOptionsLabelFrameLimit =
      MwVaCreateWidget(MwLabelClass, NULL, mOptionsBoxFrameLimit, 0, 0, 640, 1,
                       MwNtext, "Frame Limit", MwNratio, 1, NULL);
  mOptionsLabelMinSilentFrames = MwVaCreateWidget(
      MwLabelClass, NULL, mOptionsBoxMinSilentFrames, 0, 0, 640, 1, MwNtext,
      "Minimum Silent Frames", MwNratio, 1, NULL);
  mOptionsLabelSilenceThreshold =
      MwVaCreateWidget(MwLabelClass, NULL, mOptionsBoxSilenceThreshold, 0, 0,
                       640, 1, MwNtext, "Silence Threshold", MwNratio, 1, NULL);

  mOptionsInputFrameLimit = MwVaCreateWidget(
      MwEntryClass, NULL, mOptionsBoxFrameLimit, 0, 0, 1, 1, NULL);
  mOptionsInputMinSilentFrames =
      MwVaCreateWidget(MwEntryClass, NULL, mOptionsBoxMinSilentFrames, 0, 0, 1,
                       1, MwNtext, "3", NULL);
  mOptionsInputSilenceThreshold =
      MwVaCreateWidget(MwEntryClass, NULL, mOptionsBoxSilenceThreshold, 0, 0, 1,
                       1, MwNtext, "0.01", NULL);

  mStatusBar = MwVaCreateWidget(MwLabelClass, NULL, mVertBox, 0, 0, 1, 1,
                                MwNratio, 1, NULL);
}

void GUI::loop() {
  while (!MwWindowShouldClose(mWindow)) {
    while (MwPending(mWindow)) {
      MwStep(mWindow);
    }
    if (mDoDecoding) {
      this->scramble_tick();
    }
  }
}

void GUI::scramble_button_handler(MwWidget handle, void *user_data,
                                  void *call_data) {
  GUI *self = (GUI *)user_data;
  int err;
  const char* inputText = self->mFileName;

  if(inputText[0] != '\0') {
      self->mDecoder.unload();
      self->mDecoder.load(inputText);
      self->mDecoder.setup_resampler();

      self->mDecoder.reset_frames();

      memset(&self->mConfig, 0, sizeof(self->mConfig));
      self->mConfig = ma_device_config_init(ma_device_type_playback);
      self->mConfig.playback.format = self->mDecoder.out_format;
      self->mConfig.playback.channels = self->mDecoder.out_channels;
      self->mConfig.sampleRate = self->mDecoder.out_sample_rate;
      self->mConfig.dataCallback = snd_callback;
      self->mConfig.pUserData = &self->mDecoder;

      memset(&self->mDevice, 0, sizeof(self->mDevice));
      self->mDoDecoding = MwTRUE;
      MwVaApply(self->mScrambleButton, MwNdisabled, 1, NULL);
  } else {
      printf("no file\n");
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

    MwVaApply(mStatusBar, MwNtext, "Scrambling...", NULL);
    MwForceRender(mStatusBar);

    mDecoder.finish_decoding(silence_threshold, min_silent_frames);

    MwVaApply(mStatusBar, MwNtext, "Done", NULL);
    MwForceRender(mStatusBar);

    mDoDecoding = MwFALSE;

    MwVaApply(mScrambleButton, MwNdisabled, 0, NULL);

    MwForceRender(mWindow);
    MwForceRender(mPlayStopButton);
    MwForceRender(mExportButton);

    mDecoder.reset();
  } else {
    snprintf(prg, 255, "%d frames decoded", progress, NULL);
    MwVaApply(mStatusBar, MwNtext, prg, NULL);
  }
}

void GUI::play_stop(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  self->mDecoder.reset();

  if(!self->mMaInited) {
    int err;
    if ((err = ma_device_init(NULL, &self->mConfig, &self->mDevice)) !=
        MA_SUCCESS) {
        printf("Failed to init playback device, %d\n", err);
    }
    self->mMaInited = true;
  }

  if (!self->mPlaying) {
    ma_device_start(&self->mDevice);
    MwSetVoid(self->mPlayStopButton, MwNpixmap, self->mStopImage);
  } else {
    ma_device_stop(&self->mDevice);
    MwSetVoid(self->mPlayStopButton, MwNpixmap, self->mPlayImage);
  }
  self->mPlaying = !self->mPlaying;
};
