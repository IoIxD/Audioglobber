#define MINIAUDIO_IMPLEMENTATION
#ifdef _WIN32
#define MA_COINIT_VALUE COINIT_APARTMENTTHREADED
#define MA_NO_DSOUND
#endif
#include "miniaudio.h"
#include "sound.hpp"

void snd_callback(ma_device *device, void *output, const void *input,
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
