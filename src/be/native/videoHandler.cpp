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

  VideoWorker(Napi::Function& callback, std::string input, std::string output,
      int width, int height) :
      Napi::AsyncWorker(callback), path(input), output(output), width(width), height(
          height) {
    thumbNail = true;
  }

  ~VideoWorker() {

    avformat_close_input(&inputFormatContext);

    if (!thumbNail) {
      return;
    }

    av_packet_unref(decodePacket);

    // flush the codec contexts with null packet
    avcodec_send_frame(encoderContext, nullptr);
    avcodec_send_packet(decoderContext, nullptr);

    av_packet_free(&encodePacket);
    av_packet_free(&decodePacket);
    av_frame_free(&outputFrame);
    av_frame_free(&inputFrame);

    av_free(outputFrameBuffer);

    sws_freeContext(swsCtx);

    avcodec_free_context(&encoderContext);
    avcodec_free_context(&decoderContext);
    avformat_free_context(outputFormatContext);

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

    int streamIndex = av_find_best_stream(inputFormatContext,
        AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

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

  void transcodeFrame() {

    if (!sws_scale(swsCtx, inputFrame->data, inputFrame->linesize, 0,
        inputFrame->height, outputFrame->data, outputFrame->linesize)) {
      error = AVERROR_UNKNOWN;
      return;
    }

    av_frame_unref(inputFrame);

    error = avcodec_send_frame(encoderContext, outputFrame);

    if (error) {
      return;
    }

    error = avcodec_receive_packet(encoderContext, encodePacket);

    if (error) {
      return;
    }

    FILE* pFile;
    pFile = fopen(output.c_str(), "wb");
    fwrite(encodePacket->data, 1, encodePacket->size, pFile);
    fclose(pFile);

    av_packet_unref(encodePacket);

  }

  bool read_frame_until_time(int64_t time_in_tb) {

    int tempError;

    while (true) {

      av_frame_unref(inputFrame);
      av_packet_unref(decodePacket);

      error = av_read_frame(inputFormatContext, decodePacket);

      if (error < 0) {
        return false;
      }

      if (decodePacket->stream_index != stream->index) {
        continue;
      }

      tempError = avcodec_send_packet(decoderContext, decodePacket);
      if (tempError < 0) {
        if (tempError == AVERROR(EAGAIN)) {
          continue;
        } else {
          error = tempError;
          return false;
        }
      }

      tempError = avcodec_receive_frame(decoderContext, inputFrame);

      if (tempError < 0) {

        if (tempError == AVERROR(EAGAIN)) {
          continue;
        } else {
          error = tempError;
          return false;
        }
      }

      if (inputFrame->best_effort_timestamp >= time_in_tb) {
        break;
      }

    }

    return true;
  }

  void ThumbNail() {

//Opening input
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
//Opened input

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

    encoderContext->time_base = av_make_q(1,
        (stream->avg_frame_rate.num + (stream->avg_frame_rate.den / 2))
            / stream->avg_frame_rate.den);

    error = avcodec_open2(encoderContext, outputFormatContext->video_codec,
        nullptr);

    if (error) {
      return;
    }
    //init encoder

    swsCtx = sws_getContext(decoderContext->width, decoderContext->height,
        decoderContext->pix_fmt, encoderContext->width, encoderContext->height,
        encoderContext->pix_fmt, 0, nullptr, nullptr, nullptr);

    if (!swsCtx) {
      error = AVERROR_UNKNOWN;
      return;
    }

    //init frames

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

    outputFrame->format = encoderContext->pix_fmt;
    outputFrame->width = encoderContext->width;
    outputFrame->height = encoderContext->height;

    // instead of directly sending the decoded frame to the encoder, we do a sws scale to convert the frame's pixel format and color range
    // prepare a buffer for the output frame to hold the scaled image data
    int outputFrameBufferSize = av_image_get_buffer_size(
        encoderContext->pix_fmt, encoderContext->width, encoderContext->height,
        1);

    if (outputFrameBufferSize < 0) {
      error = outputFrameBufferSize;
      return;
    }

    outputFrameBuffer = reinterpret_cast<uint8_t *>(av_malloc(
        outputFrameBufferSize));

    if (!outputFrameBuffer) {
      error = AVERROR_UNKNOWN;
      return;
    }

    int fillResult = av_image_fill_arrays(outputFrame->data,
        outputFrame->linesize, outputFrameBuffer, encoderContext->pix_fmt,
        encoderContext->width, encoderContext->height, 1)
        == outputFrameBufferSize;

    if (!fillResult) {
      error = AVERROR_UNKNOWN;
      return;
    }

    //init frames

    //transcode

    double video_start_time =
        static_cast<double>(inputFormatContext->start_time) / AV_TIME_BASE;

    const double transcode_start_time = video_start_time;

    int64_t curr_timestamp_in_tb = static_cast<int64_t>(transcode_start_time
        * stream->time_base.den / stream->time_base.num);

    if (!read_frame_until_time(curr_timestamp_in_tb)) {
      error = AVERROR_UNKNOWN;
      return;
    }

    transcodeFrame();

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
  int error;
  size_t width = 0, height = 0;

  AVFormatContext* inputFormatContext = NULL;
  AVFormatContext* outputFormatContext = NULL;

  AVFrame* inputFrame = NULL;
  AVFrame* outputFrame = NULL;

  AVPacket* decodePacket = NULL;
  AVPacket* encodePacket = NULL;

  uint8_t* outputFrameBuffer = NULL;

  SwsContext* swsCtx = nullptr;

  AVCodecContext* decoderContext = NULL;
  AVCodecContext* encoderContext = NULL;

  AVStream* stream = NULL;

  AVCodecParameters* codecParameters = NULL;

  bool thumbNail = false;
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
      args[0].As<Napi::String>(), args[1].As<Napi::String>(),
      args[2].As<Napi::Number>(), args[3].As<Napi::Number>());
  thumbWorker->Queue();

  return env.Undefined();

}
