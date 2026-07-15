#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decoder.hpp"
#include "miniaudio.h"

static void data_callback(ma_device *device, void *output, const void *input,
                          ma_uint32 frame_count) {
  (void)input;
  Decoder *st = (Decoder *)device->pUserData;

  ma_uint32 bytes_per_frame =
      ma_get_bytes_per_frame(st->out_format, st->out_channels);
  ma_uint8 *out = (ma_uint8 *)output;
  ma_uint32 frames_written = 0;

  while (frames_written < frame_count) {
    if (st->pcm_buf_pos >= st->pcm_buf_len) {
      if (st->eof && st->play() != 0) {
        break; /* Nothing left to play. */
      }
      if (st->play() == AVERROR_EOF) {
        break;
      }
    }

    int available_bytes = st->pcm_buf_len - st->pcm_buf_pos;
    int available_frames = available_bytes / (int)bytes_per_frame;
    int frames_to_copy = (int)(frame_count - frames_written);
    if (frames_to_copy > available_frames)
      frames_to_copy = available_frames;

    if (frames_to_copy <= 0)
      break;

    memcpy(out + frames_written * bytes_per_frame,
           st->pcm_buf + st->pcm_buf_pos, frames_to_copy * bytes_per_frame);

    st->pcm_buf_pos += frames_to_copy * (int)bytes_per_frame;
    frames_written += (ma_uint32)frames_to_copy;
  }

  if (frames_written < frame_count) {
    memset(out + frames_written * bytes_per_frame, 0,
           (frame_count - frames_written) * bytes_per_frame);
  }
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr,
            "Usage: %s <audio/video file> <frame_limit> <silence_threshold = "
            "0.01> <min_silent_frames = 3>\n",
            argv[0]);
    return 1;
  }

  Decoder st = Decoder(argv[1]);
  int frame_limit = std::stoi(argv[2]);
  double silence_threshold = 0.01;
  int min_silent_frames = 3;
  if (argc > 3)
    silence_threshold = std::stod(argv[3]);
  if (argc > 4)
    min_silent_frames = std::stoi(argv[4]);

  st.setup_resampler();

  printf("Decoding %d frames (%0.2f silence threshold, %d min silent frames)",
         frame_limit, silence_threshold, min_silent_frames);

  st.decode(frame_limit, silence_threshold, min_silent_frames);

  printf("Decoding '%s': %d Hz, %u channel(s)\n", argv[1], st.out_sample_rate,
         st.out_channels);

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = st.out_format;
  config.playback.channels = st.out_channels;
  config.sampleRate = st.out_sample_rate;
  config.dataCallback = data_callback;
  config.pUserData = &st;

  ma_device device;
  if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
    fprintf(stderr, "Failed to init playback device\n");
    return 1;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    fprintf(stderr, "Failed to start playback device\n");
    ma_device_uninit(&device);
    return 1;
  }

  printf("Playing... press Enter to stop.\n");
  getchar();

  ma_device_uninit(&device);

  /* Cleanup */
  av_packet_free(&st.pkt);
  if (st.swr_ctx)
    swr_free(&st.swr_ctx);
  if (st.dec_ctx)
    avcodec_free_context(&st.dec_ctx);
  if (st.fmt_ctx)
    avformat_close_input(&st.fmt_ctx);
  av_free(st.pcm_buf);

  return 0;
}
