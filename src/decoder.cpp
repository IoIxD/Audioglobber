#include "decoder.hpp"

#include <algorithm>
#include <random>

Decoder::Decoder() { pkt = av_packet_alloc(); }

void Decoder::load(std::string path) {
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

void Decoder::unload() {
  if (this->fmt_ctx)
    avformat_free_context(this->fmt_ctx);
  if (this->dec_ctx)
    avcodec_free_context(&this->dec_ctx);
  if (this->swr_ctx)
    swr_free(&this->swr_ctx);

  this->fmt_ctx = nullptr;
  this->fmt_ctx = nullptr;
  this->swr_ctx = nullptr;
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

// Writes all frames from all segments to a single WAV file at `path`,
// concatenated in order.
void Decoder::write(std::string path) {
  AVFormatContext *out_ctx = nullptr;
  int ret =
      avformat_alloc_output_context2(&out_ctx, nullptr, "wav", path.c_str());
  if (!out_ctx) {
    fprintf(stderr, "Could not create output context for '%s'\n", path.c_str());
    return;
  }

  const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
  if (!encoder) {
    fprintf(stderr, "PCM S16LE encoder not found\n");
    avformat_free_context(out_ctx);
    return;
  }

  AVStream *stream = avformat_new_stream(out_ctx, nullptr);
  if (!stream) {
    fprintf(stderr, "Failed to create output stream\n");
    avformat_free_context(out_ctx);
    return;
  }

  AVCodecContext *enc_ctx = avcodec_alloc_context3(encoder);
  enc_ctx->sample_rate = dec_ctx->sample_rate;
  av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
  enc_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
  enc_ctx->time_base = (AVRational){1, dec_ctx->sample_rate};

  if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  ret = avcodec_open2(enc_ctx, encoder, nullptr);
  if (ret < 0) {
    fprintf(stderr, "Failed to open encoder\n");
    avcodec_free_context(&enc_ctx);
    avformat_free_context(out_ctx);
    return;
  }

  avcodec_parameters_from_context(stream->codecpar, enc_ctx);
  stream->time_base = enc_ctx->time_base;

  if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&out_ctx->pb, path.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open '%s' for writing\n", path.c_str());
      avcodec_free_context(&enc_ctx);
      avformat_free_context(out_ctx);
      return;
    }
  }

  ret = avformat_write_header(out_ctx, nullptr);
  if (ret < 0) {
    fprintf(stderr, "Failed to write header\n");
    goto cleanup;
  }

  {
    SwrContext *swr = nullptr;
    swr_alloc_set_opts2(&swr, &enc_ctx->ch_layout, AV_SAMPLE_FMT_S16,
                        enc_ctx->sample_rate, &dec_ctx->ch_layout,
                        dec_ctx->sample_fmt, dec_ctx->sample_rate, 0, nullptr);
    if (!swr || swr_init(swr) < 0) {
      fprintf(stderr, "Failed to init resampler\n");
      ret = -1;
      goto cleanup;
    }

    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *conv_frame = av_frame_alloc();
    int64_t pts = 0;

    // Walk every frame in every segment, in order, as one continuous stream.
    for (const Segment &segment : mSegments) {
      for (AVFrame *src : segment.frames) {
        conv_frame->format = AV_SAMPLE_FMT_S16;
        conv_frame->sample_rate = enc_ctx->sample_rate;
        av_channel_layout_copy(&conv_frame->ch_layout, &enc_ctx->ch_layout);
        conv_frame->nb_samples = swr_get_out_samples(swr, src->nb_samples);

        if (av_frame_get_buffer(conv_frame, 0) < 0) {
          fprintf(stderr, "Failed to allocate conversion buffer\n");
          break;
        }

        int converted =
            swr_convert(swr, conv_frame->data, conv_frame->nb_samples,
                        (const uint8_t **)src->extended_data, src->nb_samples);
        if (converted < 0) {
          fprintf(stderr, "swr_convert failed\n");
          av_frame_unref(conv_frame);
          break;
        }
        conv_frame->nb_samples = converted;
        conv_frame->pts = pts;
        pts += converted;

        ret = avcodec_send_frame(enc_ctx, conv_frame);
        av_frame_unref(conv_frame);
        if (ret < 0) {
          fprintf(stderr, "avcodec_send_frame failed\n");
          break;
        }

        while (ret >= 0) {
          ret = avcodec_receive_packet(enc_ctx, out_pkt);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            break;
          } else if (ret < 0) {
            fprintf(stderr, "avcodec_receive_packet failed\n");
            break;
          }
          av_packet_rescale_ts(out_pkt, enc_ctx->time_base, stream->time_base);
          out_pkt->stream_index = stream->index;
          av_interleaved_write_frame(out_ctx, out_pkt);
          av_packet_unref(out_pkt);
        }
      }
    }

    // Flush encoder.
    avcodec_send_frame(enc_ctx, nullptr);
    while (avcodec_receive_packet(enc_ctx, out_pkt) == 0) {
      av_packet_rescale_ts(out_pkt, enc_ctx->time_base, stream->time_base);
      out_pkt->stream_index = stream->index;
      av_interleaved_write_frame(out_ctx, out_pkt);
      av_packet_unref(out_pkt);
    }

    av_frame_free(&conv_frame);
    av_packet_free(&out_pkt);
    swr_free(&swr);
  }

  av_write_trailer(out_ctx);
  ret = 0;

cleanup:
  if (!(out_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&out_ctx->pb);
  avcodec_free_context(&enc_ctx);
  avformat_free_context(out_ctx);
  return;
}
bool Decoder::decode_once(int frame_limit, int *progress) {
  int ret = 0;
  AVFrame *frame = av_frame_alloc();

  *progress = mFrames.size();

  ret = avcodec_receive_frame(this->dec_ctx, frame);
  if (ret == 0) {
    mFrames.push_back(frame);
    if (frame_limit != -1) {
      if (mFrames.size() >= frame_limit) {
        printf("frame_limit hit\n");
        return true;
      }
    }
  } else if (ret == AVERROR(EAGAIN)) {
    /* Decoder wants more packets. */
    int pret = av_read_frame(this->fmt_ctx, this->pkt);
    if (pret < 0) {
      /* Flush decoder at EOF. */
      avcodec_send_packet(this->dec_ctx, NULL);
      this->eof = 1;
      return true;
    }
    if (this->pkt->stream_index == this->stream_index) {
      avcodec_send_packet(this->dec_ctx, this->pkt);
    }
    av_frame_unref(frame);
    av_packet_unref(this->pkt);
    return false;
  } else {
    av_frame_unref(frame);
    if (ret != AVERROR_EOF) {
      fprintf(stderr, "avcodec_receive_frame error\n");
    }
    return true;
  }

  return false;
}

void Decoder::finish_decoding(double silence_threshold, int min_silent_frame) {
  printf("prepared (%ld frames)\n", mFrames.size());

  mSegments = split_on_silence(mFrames, silence_threshold, min_silent_frame);

  auto rng = std::default_random_engine{};
  std::shuffle(std::begin(mSegments), std::end(mSegments), rng);

  printf("split into %zu segments\n", mSegments.size());
}

/* Decode the next packet(s) until we get a frame's worth of converted PCM
 * into st->pcm_buf. Returns 0 on success, AVERROR_EOF at end of stream. */
int Decoder::play() {
  // mProgressMutex.lock();
  // mProgressNum++;
  // mProgressMutex.unlock();

  while (1) {
    if (seg_counter < mSegments.size() - 1 && mSegments.size() != 0) {
      auto frames = mSegments.at(seg_counter).frames;
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
      } else {
        seg_counter++;
        frame_counter = 0;
      }
      return 0;
    }
  }

  return true;
}

void Decoder::reset() {
  seg_counter = 0;
  frame_counter = 0;
  pcm_buf = NULL;
  pcm_buf_capacity = 0;
  pcm_buf_len = 0;
  pcm_buf_pos = 0;
  eof = 0;
  // mProgressNum = 0;
}

void Decoder::reset_frames() {
  for (auto seg : mSegments) {
    for (auto frame : seg.frames) {
      av_frame_unref(frame);
    }
  }
  for (auto frame : mFrames) {
    av_frame_unref(frame);
  }
  mSegments.erase(mSegments.begin(), mSegments.end());
  mFrames.erase(mFrames.begin(), mFrames.end());
}
