#include "decoder.hpp"

#include <algorithm>
#include <random>

Decoder::Decoder(std::string path) {
  pkt = av_packet_alloc();
  if (avformat_open_input(&this->fmt_ctx, path.c_str(), NULL, NULL) < 0) {
    fprintf(stderr, "Failed to open input '%s'\n", path.c_str());
    exit(-1);
  }
  if (avformat_find_stream_info(this->fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Failed to find stream info\n");
    exit(-1);
  }

  this->stream_index =
      av_find_best_stream(this->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (this->stream_index < 0) {
    fprintf(stderr, "No audio stream found\n");
    exit(-1);
  }

  AVCodecParameters *params =
      this->fmt_ctx->streams[this->stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(params->codec_id);
  if (!codec) {
    fprintf(stderr, "Unsupported codec\n");
    exit(-1);
  }

  this->dec_ctx = avcodec_alloc_context3(codec);
  if (avcodec_parameters_to_context(this->dec_ctx, params) < 0) {
    fprintf(stderr, "Failed to copy codec params\n");
    exit(-1);
  }

  if (avcodec_open2(this->dec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "Failed to open codec\n");
    exit(-1);
  }

  return;
}

/* Map an AVSampleFormat (post-conversion, always planar-or-interleaved S16
 * or FLT in this example) to a miniaudio ma_format. We standardize output
 * to signed 16-bit interleaved, which is the safest/most-compatible target. */
void Decoder::setup_resampler() {
  AVChannelLayout out_ch_layout;
  av_channel_layout_default(&out_ch_layout,
                            this->dec_ctx->ch_layout.nb_channels);

  int ret = swr_alloc_set_opts2(
      &this->swr_ctx, &out_ch_layout, /* out layout   */
      AV_SAMPLE_FMT_S16,              /* out format: interleaved 16-bit PCM */
      this->dec_ctx->sample_rate,     /* out rate: keep native rate */
      &this->dec_ctx->ch_layout,      /* in layout    */
      this->dec_ctx->sample_fmt,      /* in format    */
      this->dec_ctx->sample_rate,     /* in rate      */
      0, NULL);
  if (ret < 0 || !this->swr_ctx) {
    fprintf(stderr, "Failed to allocate resampler\n");
    exit(-1);
  }
  if (swr_init(this->swr_ctx) < 0) {
    fprintf(stderr, "Failed to init resampler\n");
    exit(-1);
  }

  this->out_format = ma_format_s16;
  this->out_channels = (ma_uint32)out_ch_layout.nb_channels;
  this->out_sample_rate = (ma_uint32)this->dec_ctx->sample_rate;

  av_channel_layout_uninit(&out_ch_layout);
  return;
}

// Compute RMS amplitude of a single AVFrame, normalized to [0, 1].
// Handles the common sample formats; extend as needed.
static double frame_rms(AVFrame *frame) {
  int nb_samples = frame->nb_samples;
  int channels = frame->ch_layout.nb_channels;
  AVSampleFormat fmt = (AVSampleFormat)frame->format;

  double sum_sq = 0.0;
  int64_t total = 0;

  bool planar = av_sample_fmt_is_planar(fmt);
  AVSampleFormat packed_fmt = av_get_packed_sample_fmt(fmt);

  auto accumulate = [&](auto get_sample) {
    for (int ch = 0; ch < channels; ch++) {
      const uint8_t *data =
          planar ? frame->extended_data[ch] : frame->extended_data[0];
      int stride = planar ? 1 : channels;
      for (int i = 0; i < nb_samples; i++) {
        double s = get_sample(data, planar ? i : i * channels + ch);
        sum_sq += s * s;
        total++;
      }
    }
  };

  switch (packed_fmt) {
  case AV_SAMPLE_FMT_S16:
    accumulate([](const uint8_t *d, int idx) {
      return ((const int16_t *)d)[idx] / 32768.0;
    });
    break;
  case AV_SAMPLE_FMT_S32:
    accumulate([](const uint8_t *d, int idx) {
      return ((const int32_t *)d)[idx] / 2147483648.0;
    });
    break;
  case AV_SAMPLE_FMT_FLT:
    accumulate([](const uint8_t *d, int idx) {
      return (double)((const float *)d)[idx];
    });
    break;
  case AV_SAMPLE_FMT_DBL:
    accumulate(
        [](const uint8_t *d, int idx) { return ((const double *)d)[idx]; });
    break;
  default:
    // Unhandled format — treat as non-silent so we don't
    // accidentally split on garbage.
    return 1.0;
  }

  if (total == 0)
    return 0.0;
  return std::sqrt(sum_sq / total);
}

std::vector<Decoder::Segment>
Decoder::split_on_silence(const std::vector<AVFrame *> &frames,
                          double silence_threshold, int min_silent_frames) {
  std::vector<Segment> segments;
  Segment current;
  int silent_run = 0;

  for (AVFrame *f : frames) {
    double rms = frame_rms(f);
    bool is_silent = rms < silence_threshold;

    if (is_silent) {
      silent_run++;
    } else {
      silent_run = 0;
    }

    current.frames.push_back(f);

    if (silent_run == min_silent_frames) {
      // Close out this segment (drop the trailing silent frames if
      // you don't want them attached to the previous segment).
      segments.push_back(current);
      current = Segment{};
    }
  }

  if (!current.frames.empty()) {
    segments.push_back(current);
  }

  return segments;
}

void Decoder::decode(int frame_limit, double silence_threshold,
                     int min_silent_frame) {
  int ret = 0;
  std::vector<AVFrame *> frames;
  while (1) {
    AVFrame *frame = av_frame_alloc();
    ret = avcodec_receive_frame(this->dec_ctx, frame);
    if (ret == 0) {
      frames.push_back(frame);
      if (frames.size() >= 2500) {
        break;
      }
    } else if (ret == AVERROR(EAGAIN)) {
      /* Decoder wants more packets. */
      int pret = av_read_frame(this->fmt_ctx, this->pkt);
      if (pret < 0) {
        /* Flush decoder at EOF. */
        avcodec_send_packet(this->dec_ctx, NULL);
        this->eof = 1;
        continue;
      }
      if (this->pkt->stream_index == this->stream_index) {
        avcodec_send_packet(this->dec_ctx, this->pkt);
      }
      av_frame_unref(frame);
      av_packet_unref(this->pkt);
    } else {
      av_frame_unref(frame);
      if (ret != AVERROR_EOF) {
        fprintf(stderr, "avcodec_receive_frame error\n");
      }
      break;
    }
  }

  // auto rng = std::default_random_engine{};
  // std::shuffle(std::begin(frames), std::end(frames), rng);

  printf("prepared (%ld frames)\n", frames.size());

  mSegments = split_on_silence(frames, silence_threshold, min_silent_frame);

  auto rng = std::default_random_engine{};
  std::shuffle(std::begin(mSegments), std::end(mSegments), rng);

  printf("split into %zu segments\n", mSegments.size());
}

/* Decode the next packet(s) until we get a frame's worth of converted PCM
 * into st->pcm_buf. Returns 0 on success, AVERROR_EOF at end of stream. */
int Decoder::play() {

  printf("segment %d, frame %d\n", seg_counter, frame_counter);

  while (1) {
    if (seg_counter < mSegments.size() - 1) {
      auto frames = mSegments[seg_counter].frames;
      if (frame_counter++ < frames.size() - 1 && frames.size() != 0) {
        AVFrame *frame = frames[frame_counter];
        /* Got a frame: convert it. */
        int out_samples = swr_get_out_samples(this->swr_ctx, frame->nb_samples);
        int needed_bytes = out_samples * this->out_channels *
                           av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

        if (needed_bytes > this->pcm_buf_capacity) {
          this->pcm_buf = (uint8_t *)av_realloc(this->pcm_buf, needed_bytes);
          this->pcm_buf_capacity = needed_bytes;
        }

        uint8_t *out_planes[1] = {this->pcm_buf};
        int converted = swr_convert(this->swr_ctx, out_planes, out_samples,
                                    (const uint8_t **)frame->extended_data,
                                    frame->nb_samples);
        if (converted < 0) {
          fprintf(stderr, "swr_convert failed\n");
          return -1;
        }

        this->pcm_buf_len = converted * this->out_channels *
                            av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        this->pcm_buf_pos = 0;
        av_frame_unref(frame);
      } else {
        seg_counter++;
        frame_counter = 0;
      }
      return 0;
    }
  }
  return true;
}
