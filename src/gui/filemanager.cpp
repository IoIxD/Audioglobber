#include "gui.hpp"
#include <MNFM.h>

void GUI::file_handler(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  char *file_data = (char *)call_data;

  if (call_data) {
    MwVaApply(self->mInputPreview, MwNtext, file_data, NULL);
    MwVaApply(self->mScrambleButton, MwNdisabled, 0, NULL);
    MwVaApply(self->mPlayPauseButton, MwNdisabled, 1, NULL);
    MwVaApply(self->mStopButton, MwNdisabled, 1, NULL);
  }
};

void GUI::file_button_handler(MwWidget handle, void *user_data,
                              void *call_data) {
  GUI *self = (GUI *)user_data;

  self->mFileChooser = MNFMOpen(handle, "Choose file", MNFMFILE);
  MwAddUserHandler(self->mFileChooser, MwNfileChosenHandler, GUI::file_handler,
                   self);
};
