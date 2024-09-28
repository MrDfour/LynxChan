#include <napi.h>
#include "captcha.h"
#include "imageBounds.h"
#include "videoHandler.h"
#include "imageThumb.h"

extern "C" {

#include <libavutil/log.h>
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {

  av_log_set_level (AV_LOG_QUIET);

  exports.Set(Napi::String::New(env, "buildCaptcha"),
      Napi::Function::New(env, buildCaptcha));
  exports.Set(Napi::String::New(env, "getImageBounds"),
      Napi::Function::New(env, getImageBounds));
  exports.Set(Napi::String::New(env, "getVideoBounds"),
      Napi::Function::New(env, getVideoBounds));
  exports.Set(Napi::String::New(env, "thumbNailVideo"),
      Napi::Function::New(env, thumbnailVideo));
  exports.Set(Napi::String::New(env, "imageThumb"),
      Napi::Function::New(env, imageThumb));
  return exports;
}

NODE_API_MODULE(native, Init)
