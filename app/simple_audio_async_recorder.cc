/*
 * simple_audio_async_recorder.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "simple_audio_async_recorder.h"
#include "media/audio/audio.h"

#include <vector>

namespace ave {

SimpleAudioAsyncRecorder::SimpleAudioAsyncRecorder(
    base::TaskRunnerFactory* task_runner_factory,
    std::shared_ptr<media::AudioDevice> audio_device)
    : task_runner_(std::make_unique<base::TaskRunner>(
          task_runner_factory->CreateTaskRunner(
              "AudioRecorder",
              base::TaskRunnerFactory::Priority::HIGH))),
      audio_device_(std::move(audio_device)) {}

SimpleAudioAsyncRecorder::~SimpleAudioAsyncRecorder() {}

void SimpleAudioAsyncRecorder::Init(int32_t sample_rate, int32_t channels) {
  task_runner_->PostTask([this, sample_rate, channels]() {
    // Initialize audio recorder resources here
    media::AudioConfig audio_config;
    audio_config.sample_rate = sample_rate;
    audio_config.channel_layout = channels == 1 ? media::CHANNEL_LAYOUT_MONO
                                                : media::CHANNEL_LAYOUT_STEREO;
    audio_config.format = media::AUDIO_FORMAT_PCM_16_BIT;
    audio_config.frame_size = channels * 2;  // 16-bit = 2 bytes per sample

    audio_record_ = audio_device_->CreateAudioRecord();
    audio_record_->Open(audio_config);
  });
}

void SimpleAudioAsyncRecorder::Release() {
  task_runner_->PostTaskAndWait([this]() {
    // Release audio recorder resources here
    if (audio_record_) {
      audio_record_->Close();
      audio_record_.reset();
    }
  });
}

bool SimpleAudioAsyncRecorder::Start() {
  bool success = false;
  task_runner_->PostTaskAndWait([this, &success]() {
    if (audio_record_) {
      success = audio_record_->Start() == 0;
    }
  });
  return success;
}

void SimpleAudioAsyncRecorder::Stop() {
  task_runner_->PostTaskAndWait([this]() {
    if (audio_record_) {
      audio_record_->Stop();
    }
  });
}

size_t SimpleAudioAsyncRecorder::Read(const void* data, size_t size) {
  size_t bytes_read = 0;
  task_runner_->PostTaskAndWait([this, data, size, &bytes_read]() {
    if (audio_record_ && audio_record_->ready()) {
      ssize_t result =
          audio_record_->Read(const_cast<void*>(data), size, false);
      if (result > 0) {
        bytes_read = static_cast<size_t>(result);
      }
    }
  });
  return bytes_read;
}

}  // namespace ave
