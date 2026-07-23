#include "gui.hpp"

#include "icons/open.h"
#include "icons/play.h"
#include "icons/save.h"
#include "icons/stop.h"

void GUI::setup_icons(MwWidget handle) {
  auto placeholder = MwGetInteger(handle, MwNdarkTheme);
  if (placeholder == 1) {
    int i = 0;
    #define BRIGHTEN(img) \
    for (i = 0; i < sizeof(img.pixel_data); i += 4) {\
      int r = img.pixel_data[i];\
      int g = img.pixel_data[i + 1];\
      int b = img.pixel_data[i + 2];\
      int a = img.pixel_data[i + 3];\
      if (r == 0 && g == 0 && b == 0 && a == 255) {\
        img.pixel_data[i] = 210;\
        img.pixel_data[i + 1] = 210;\
        img.pixel_data[i + 2] = 210;\
      }\
    }
    BRIGHTEN(gPlayImage);
    BRIGHTEN(gStopImage);
    BRIGHTEN(gSaveImage);
    BRIGHTEN(gOpenImage);
  }

    mPlayImage = MwLoadRaw(handle, (unsigned char *)gPlayImage.pixel_data,
                           gPlayImage.width, gPlayImage.height);
    mStopImage = MwLoadRaw(handle, (unsigned char *)gStopImage.pixel_data,
                           gStopImage.width, gStopImage.height);
    mSaveImage = MwLoadRaw(handle, (unsigned char *)gSaveImage.pixel_data,
                           gSaveImage.width, gSaveImage.height);
    mOpenImage = MwLoadRaw(handle, (unsigned char *)gOpenImage.pixel_data,
                           gOpenImage.width, gOpenImage.height);
}
