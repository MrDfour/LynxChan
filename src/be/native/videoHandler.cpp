extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "videoHandler.h"

class VideoWorker: public Napi::AsyncWorker {
public:

  VideoWorker(Napi::Function& callback, std::string path) :
      Napi::AsyncWorker(callback), path(path) {
  }

  VideoWorker(Napi::Function& callback, bool gif, std::string input,
      std::string output, int width, int height, int percentage) :
      Napi::AsyncWorker(callback), path(input), output(output), percentage(
          percentage), width(width), height(height), gifThumb(gif) {
    thumbNail = true;
  }

  ~VideoWorker() {

    avformat_close_input(&inputFormatContext);

    if (!thumbNail) {
      return;
    }

    if (decodePacket) {
      av_packet_unref(decodePacket);
      av_packet_free(&decodePacket);
    }

    if (encodePacket) {
      av_packet_unref(encodePacket);
      av_packet_free(&encodePacket);
    }

    // flush the codec contexts with null packet
    if (encoderContext) {
      avcodec_send_frame(encoderContext, nullptr);
      avcodec_free_context(&encoderContext);
    }

    if (decoderContext) {
      avcodec_send_packet(decoderContext, nullptr);
      avcodec_free_context(&decoderContext);
    }

    if (outputFrame) {
      av_freep(&outputFrame->data);
      av_frame_free(&outputFrame);
    }

    if (inputFrame) {
      av_frame_free(&inputFrame);
    }

    if (swsCtx) {
      sws_freeContext(swsCtx);
    }

    if (outputFormatContext) {
      avformat_free_context(outputFormatContext);
    }

  }

  bool OpenInput() {

    error = avformat_open_input(&inputFormatContext, path.c_str(), NULL, NULL);

    if (error) {
      return false;
    }

    error = avformat_find_stream_info(inputFormatContext, NULL);

    if (error) {
      return false;
    }

    streamIndex = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO,
        -1, -1, nullptr, 0);

    if (streamIndex < 0) {
      error = AVERROR_UNKNOWN;
      return false;
    }

    stream = inputFormatContext->streams[streamIndex];
    codecParameters = stream->codecpar;

    if (!codecParameters) {
      error = AVERROR_UNKNOWN;
      return false;
    }

    return true;

  }

  void ThumbNail() {

    //opening decoder
    AVCodec* decoderCodec = avcodec_find_decoder(codecParameters->codec_id);

    decoderContext = avcodec_alloc_context3(decoderCodec);
    if (!decoderContext) {
      error = AVERROR_UNKNOWN;
      return;
    }

    error = avcodec_parameters_to_context(decoderContext, codecParameters);

    if (error) {
      return;
    }

    decoderContext->thread_count = 0;
    error = avcodec_open2(decoderContext, decoderCodec, nullptr);

    if (error) {
      return;
    }
    //opening decoder

    //init frames and packets

    inputFrame = av_frame_alloc();

    if (!inputFrame) {
      error = AVERROR_UNKNOWN;
      return;
    }

    outputFrame = av_frame_alloc();

    if (!outputFrame) {
      error = AVERROR_UNKNOWN;
      return;
    }

    decodePacket = av_packet_alloc();

    if (!decodePacket) {
      error = AVERROR_UNKNOWN;
      return;
    }

    encodePacket = av_packet_alloc();

    if (!encodePacket) {
      error = AVERROR_UNKNOWN;
      return;
    }

    //init encoder
    avformat_alloc_output_context2(&outputFormatContext, NULL, NULL,
        output.c_str());

    outputFormatContext->video_codec_id = av_guess_codec(
        outputFormatContext->oformat, NULL, output.c_str(), NULL,
        AVMEDIA_TYPE_VIDEO);

    outputFormatContext->video_codec = avcodec_find_encoder(
        outputFormatContext->video_codec_id);

    if (!outputFormatContext->video_codec) {
      error = AVERROR_UNKNOWN;
      return;
    }

    encoderContext = avcodec_alloc_context3(outputFormatContext->video_codec);

    if (!encoderContext) {
      error = AVERROR_UNKNOWN;
      return;
    }

    encoderContext->width = width;
    encoderContext->height = height;

    encoderContext->pix_fmt = avcodec_find_best_pix_fmt_of_list(
        outputFormatContext->video_codec->pix_fmts, decoderContext->pix_fmt,
        true, NULL);

    if (stream->avg_frame_rate.den) {
      encoderContext->time_base = av_make_q(1,
          (stream->avg_frame_rate.num + (stream->avg_frame_rate.den / 2))
              / stream->avg_frame_rate.den);

      if (!gifThumb && percentage > 0 && percentage <= 100) {

        int timeBaseRatio = stream->time_base.den / stream->time_base.num;

        desiredTimeStamp = inputFormatContext->duration * timeBaseRatio
            / AV_TIME_BASE / (100 / percentage);

        error = av_seek_frame(inputFormatContext, streamIndex, desiredTimeStamp,
            AVSEEK_FLAG_BACKWARD);

        if (error < 0) {
          return;
        }

        avcodec_flush_buffers(decoderContext);

      }

    } else {
      encoderContext->time_base = av_make_q(1, 1);
    }

    error = avcodec_open2(encoderContext, outputFormatContext->video_codec,
        nullptr);

    if (error) {
      return;
    }
    //init encoder

    //init scaler
    swsCtx = sws_getContext(decoderContext->width, decoderContext->height,
        decoderContext->pix_fmt, encoderContext->width, encoderContext->height,
        encoderContext->pix_fmt, 0, nullptr, nullptr, nullptr);

    if (!swsCtx) {
      error = AVERROR_UNKNOWN;
      return;
    }
    //init scaler

    outputFrame->format = encoderContext->pix_fmt;
    outputFrame->width = encoderContext->width;
    outputFrame->height = encoderContext->height;

    //TODO figure why av_frame_get_buffer causes gifs to not animate
    //error = av_frame_get_buffer(outputFrame, 0);
    error = av_image_alloc(outputFrame->data, outputFrame->linesize,
        encoderContext->width, encoderContext->height, encoderContext->pix_fmt,
        32);

    if (error < 0) {
      return;
    }

    //init frames and packets

    AVStream* outputStream = avformat_new_stream(outputFormatContext, NULL);

    error = avcodec_parameters_from_context(outputStream->codecpar,
        decoderContext);

    if (error) {
      return;
    }

    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
      error = avio_open(&outputFormatContext->pb, output.c_str(),
          AVIO_FLAG_WRITE);
      if (error < 0) {
        return;
      }
    }

    AVDictionary *opts = NULL;

    if (gifThumb) {
      av_dict_set(&opts, "loop", "0", 0);
    }

    error = avformat_write_header(outputFormatContext, &opts);

    if (error) {
      return;
    }

    while (true) {

      av_frame_unref(inputFrame);
      av_packet_unref(decodePacket);

      //probably ended frames
      if (av_read_frame(inputFormatContext, decodePacket) < 0) {
        error = 0;
        break;
      }

      //different stream. ignore.
      if (decodePacket->stream_index != streamIndex) {
        continue;
      }

      //send packet
      error = avcodec_send_packet(decoderContext, decodePacket);

      if (error < 0) {

        //get next one
        if (error == AVERROR(EAGAIN)) {
          continue;
        } else {
          return;
        }
      }

      //send frame
      error = avcodec_receive_frame(decoderContext, inputFrame);

      if (error < 0) {

        //get next one
        if (error == AVERROR(EAGAIN)) {
          continue;
        } else {
          return;
        }
      }

      if (desiredTimeStamp
          && inputFrame->best_effort_timestamp < desiredTimeStamp) {
        continue;
      }

      if (!sws_scale(swsCtx, inputFrame->data, inputFrame->linesize, 0,
          inputFrame->height, outputFrame->data, outputFrame->linesize)) {
        error = AVERROR_UNKNOWN;
        return;
      }

      error = avcodec_send_frame(encoderContext, outputFrame);

      if (error) {
        return;
      }

      error = avcodec_receive_packet(encoderContext, encodePacket);

      if (error) {
        return;
      }

      encodePacket->pts = av_rescale_q_rnd(decodePacket->pts, stream->time_base,
          outputStream->time_base,
          static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
      encodePacket->dts = av_rescale_q_rnd(decodePacket->dts, stream->time_base,
          outputStream->time_base,
          static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
      encodePacket->duration = av_rescale_q(decodePacket->duration,
          stream->time_base, outputStream->time_base);
      encodePacket->pos = -1;

      error = av_interleaved_write_frame(outputFormatContext, encodePacket);

      if (error < 0) {
        return;
      } else if (!gifThumb) {
        break;
      }

    }

    error = av_write_trailer(outputFormatContext);

    if (error) {
      return;
    }

    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&outputFormatContext->pb);
    }

  }

  void Execute() {

    if (!OpenInput()) {
      return;
    }

    if (thumbNail) {
      return ThumbNail();
    }

    width = codecParameters->width;
    height = codecParameters->height;

    AVDictionaryEntry* tag = av_dict_get(stream->metadata, "rotate", NULL, 0);

    if (tag && (!strcmp(tag->value, "90") || !strcmp(tag->value, "270"))) {

      width += height;
      height = width - height;
      width -= height;
    }

  }

  void OnOK() {
    Napi::HandleScope scope(Env());

    if (error) {

      char errorBuffer[256];

      av_strerror(error, errorBuffer, 256);

      errorStr = "Error processing " + path + ": " + errorBuffer;
    }

    if (thumbNail) {
      Callback().Call(
          { error ? Napi::String::New(Env(), errorStr) : Env().Undefined() });
    } else {
      Callback().Call(
          { error ? Napi::String::New(Env(), errorStr) : Env().Undefined(),
              Napi::Number::New(Env(), width), Napi::Number::New(Env(), height) });
    }

  }

private:
  std::string path, output, errorStr;
  int error = 0, streamIndex = 0, percentage = 0;
  size_t width = 0, height = 0;

  AVFormatContext* inputFormatContext = NULL;
  AVFormatContext* outputFormatContext = NULL;

  AVFrame* inputFrame = NULL;
  AVFrame* outputFrame = NULL;

  AVPacket* decodePacket = NULL;
  AVPacket* encodePacket = NULL;

  SwsContext* swsCtx = nullptr;

  AVCodecContext* decoderContext = NULL;
  AVCodecContext* encoderContext = NULL;

  AVStream* stream = NULL;

  AVCodecParameters* codecParameters = NULL;

  bool thumbNail = false;
  bool gifThumb = false;

  int64_t desiredTimeStamp = 0;

};

Napi::Value getVideoBounds(const Napi::CallbackInfo& args) {

  Napi::Env env = args.Env();

  Napi::Function callback = args[1].As<Napi::Function>();

  VideoWorker* sizeWorker = new VideoWorker(callback,
      args[0].As<Napi::String>());

  sizeWorker->Queue();

  return env.Undefined();

}

Napi::Value thumbnailVideo(const Napi::CallbackInfo& args) {

  Napi::Env env = args.Env();

  Napi::Function callback = args[4].As<Napi::Function>();

  VideoWorker* thumbWorker = new VideoWorker(callback,
      args[5].As<Napi::Boolean>(), args[0].As<Napi::String>(),
      args[1].As<Napi::String>(), args[2].As<Napi::Number>(),
      args[3].As<Napi::Number>(), args[6].As<Napi::Number>());
  thumbWorker->Queue();

  return env.Undefined();

}
