#pragma once

#include "miniaudio.h"
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <string>
class Decoder {

  struct Segment {
    std::vector<AVFrame *> frames;
  };

public:
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *dec_ctx = NULL;
  SwrContext *swr_ctx = NULL;
  int stream_index = 0;

  uint8_t *pcm_buf = NULL;
  int pcm_buf_capacity = 0;
  int pcm_buf_len = 0; /* bytes currently valid   */
  int pcm_buf_pos = 0; /* read cursor into pcm_buf */

  AVPacket *pkt = NULL;
  std::vector<Segment> mSegments;
  int seg_counter = 0;
  int frame_counter = 0;
  AVFrame *holding_frame = nullptr;
  // AVFrame *frame = NULL;

  int eof = 0;

  ma_format out_format;
  ma_uint32 out_channels;
  ma_uint32 out_sample_rate;

  std::vector<Segment>
  split_on_silence(const std::vector<AVFrame *> &frames,
                   double silence_threshold, // RMS below this = "silent"
                   int min_silent_frames);

public:
  Decoder(std::string name);
  void setup_resampler();

  void decode(int frame_limit, double silence_threshold, int min_silent_frames);
  void write(std::string out_path);
  int play();
};
