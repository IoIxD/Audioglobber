#include "gui.hpp"
#include <MNFM.h>

static MwBool doErrorBox;

void MWAPI GUI::error_box_ok(MwWidget handle, void *user, void *call) {
  GUI *self = (GUI *)user;
  (void)handle;
  (void)call;
  doErrorBox = MwFALSE;
}

void GUI::file_handler(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  char *file_data = (char *)call_data;

  if (call_data) {
    MwVaApply(self->mInputPreview, MwNtext, file_data, NULL);
    MwVaApply(self->mScrambleButton, MwNdisabled, 0, NULL);
  }
};

void GUI::filesave_handler(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  char *file_data = (char *)call_data;
  char statusBar[4096];

  if (!strstr(file_data, ".wav")) {
    auto error = MwMessageBox(handle, "Only saving to .wav files is supported",
                              "Error", MwMB_BUTTONOK);
    doErrorBox = MwTRUE;
    MwAddUserHandler(MwMessageBoxGetChild(error, MwMB_BUTTONOK),
                     MwNactivateHandler, GUI::error_box_ok, self);
    MwAddUserHandler(error, MwNcloseHandler, GUI::error_box_ok, self);

    while (doErrorBox) {
      MwStep(error);
    }

    MwDestroyWidget(error);

    GUI::save_button_handler(handle, user_data, call_data);
  } else {
    snprintf(statusBar, sizeof(statusBar), "Saved to %s", file_data);

    self->mDecoder.write(file_data);
    MwVaApply(self->mStatusBar, MwNtext, statusBar, NULL);
  }
};

void GUI::file_button_handler(MwWidget handle, void *user_data,
                              void *call_data) {
  GUI *self = (GUI *)user_data;

  self->mFileChooser = MNFMOpen(handle, "Choose file", MNFMFILE);
  MwAddUserHandler(self->mFileChooser, MwNfileChosenHandler, GUI::file_handler,
                   self);
};

void GUI::save_button_handler(MwWidget handle, void *user_data,
                              void *call_data) {
  GUI *self = (GUI *)user_data;

  self->mFileChooser = MNFMOpen(handle, "Save file", MNFMSAVE);
  MwAddUserHandler(self->mFileChooser, MwNfileChosenHandler,
                   GUI::filesave_handler, self);
};

void GUI::drag_and_drop(MwWidget handle, void *user_data, void *call_data) {
  GUI *self = (GUI *)user_data;
  unsigned char *file_data = (unsigned char *)call_data;
  int i = 0;
  MwBool is_accepted = MwTRUE;

  if (file_data) {
      for(i = 0; i < strlen((char *)file_data); i++) {
          if(file_data[i] >= 127) {
              is_accepted = MwFALSE;
              break;
          }
      }
      if(!is_accepted) {
          MwVaApply(self->mStatusBar, MwNtext, "Filename cannot have non-special characters in it.", NULL);
      } else {
          memset(self->mFileName, 0, sizeof(self->mFileName));
          snprintf(self->mFileName, sizeof(self->mFileName), "%s", file_data);
          MwVaApply(self->mInputPreview, MwNtext, self->mFileName, NULL);
          MwVaApply(self->mScrambleButton, MwNdisabled, 0, NULL);
      }
  }
};
