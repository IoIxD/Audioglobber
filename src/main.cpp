#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decoder.hpp"
#include "gui/gui.hpp"
#include "miniaudio.h"

int main(int argc, char **argv) {
  GUI gui;

  gui.loop();

  // ma_device_uninit(&device);

  // /* Cleanup */
  // av_packet_free(&st.pkt);
  // if (st.swr_ctx)
  //   swr_free(&st.swr_ctx);
  // if (st.dec_ctx)
  //   avcodec_free_context(&st.dec_ctx);
  // if (st.fmt_ctx)
  //   avformat_close_input(&st.fmt_ctx);
  // av_free(st.pcm_buf);

  return 0;
}
