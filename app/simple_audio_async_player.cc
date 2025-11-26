/*
 * simple_audio_async_player.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "simple_audio_async_player.h"
#include "media/audio/audio.h"

#include <cmath>
#include <vector>

namespace ave {

namespace {
constexpr int32_t kDefaultTestBufferMs = 5000;
constexpr int32_t kSignalStartMs = 1000;
constexpr int32_t kWriteChunkSizeMs = 10;  // Write 10ms chunks at a time

std::vector<float> generate_sine_wave(float frequency,
                                      float duration,
                                      int32_t sample_rate,
                                      int32_t channels) {
  size_t num_samples = static_cast<size_t>(duration * sample_rate);
  std::vector<float> wave(num_samples * channels);

  for (size_t sample = 0; sample < num_samples; sample++) {
    float value =
        0.8f * std::sin(2.0f * M_PI * frequency * sample / sample_rate);
    // 为每个通道生成相同的值（交错排列）
    for (int32_t ch = 0; ch < channels; ch++) {
      wave[sample * channels + ch] = value;
    }
  }
  return wave;
}

std::vector<float> generate_test_signal_f(int32_t frequency,
                                          int32_t duration_ms,
                                          int32_t sample_rate,
                                          int32_t channels) {
  // total kDefaultTestBufferMs ms buffer
  // generate duration_ms data from kSignalStartMs
  size_t total_samples =
      static_cast<size_t>(sample_rate * kDefaultTestBufferMs / 1000);
  std::vector<float> buffer(total_samples * channels, 0.0f);

  size_t start_sample =
      static_cast<size_t>(sample_rate * kSignalStartMs / 1000);
  std::vector<float> signal = generate_sine_wave(
      frequency, duration_ms / 1000.0f, sample_rate, channels);

  // 将信号数据复制到缓冲区，考虑通道数
  size_t signal_samples = static_cast<size_t>(duration_ms * sample_rate / 1000);
  for (size_t i = 0; i < signal_samples; ++i) {
    if (start_sample + i < total_samples) {
      for (int32_t ch = 0; ch < channels; ch++) {
        size_t buffer_idx = (start_sample + i) * channels + ch;
        size_t signal_idx = i * channels + ch;
        if (buffer_idx < buffer.size() && signal_idx < signal.size()) {
          buffer[buffer_idx] = signal[signal_idx];
        }
      }
    }
  }
  return buffer;
}

std::vector<int16_t> float_to_int16(const std::vector<float>& input) {
  std::vector<int16_t> output(input.size());
  for (size_t i = 0; i < input.size(); i++) {
    float sample = input[i];
    sample = std::max(-1.0f, std::min(1.0f, sample));  // Clamp to [-1.0, 1.0]
    output[i] = static_cast<int16_t>(sample * 32767);  // Scale to int16 range
  }
  return output;
}

std::vector<int16_t> generate_test_signal(int32_t frequency,
                                          int32_t duration_ms,
                                          int sample_rate,
                                          int32_t channels) {
  std::vector<float> float_buffer =
      generate_test_signal_f(frequency, duration_ms, sample_rate, channels);
  return float_to_int16(float_buffer);
}
}  // namespace

SimpleAudioAsyncPlayer::SimpleAudioAsyncPlayer(
    base::TaskRunnerFactory* task_runner_factory,
    std::shared_ptr<media::AudioDevice> audio_device)
    : task_runner_(std::make_unique<base::TaskRunner>(
          task_runner_factory->CreateTaskRunner(
              "AudioPlayer",
              base::TaskRunnerFactory::Priority::HIGH))),
      audio_device_(std::move(audio_device)) {}

SimpleAudioAsyncPlayer::~SimpleAudioAsyncPlayer() {}

void SimpleAudioAsyncPlayer::Init(int32_t sample_rate,
                                  int channels,
                                  int32_t core_signal_freq,
                                  int32_t core_signal_duration_ms) {
  task_runner_->PostTask([this, sample_rate, channels, core_signal_freq,
                          core_signal_duration_ms]() {
    // create test buffer first
    loop_buffer_ = generate_test_signal(
        core_signal_freq, core_signal_duration_ms, sample_rate, channels);

    // Initialize audio player resources here
    media::AudioConfig audio_config;
    audio_config.sample_rate = sample_rate;
    audio_config.channel_layout = channels == 1 ? media::CHANNEL_LAYOUT_MONO
                                                : media::CHANNEL_LAYOUT_STEREO;
    audio_config.format = media::AUDIO_FORMAT_PCM_16_BIT;
    audio_config.frame_size = channels * 2;  // 16-bit = 2 bytes per sample

    audio_track_ = audio_device_->CreateAudioTrack();
    audio_track_->Open(audio_config);
  });
}

void SimpleAudioAsyncPlayer::Release() {
  task_runner_->PostTaskAndWait([this]() {
    // Release audio player resources here
    if (audio_track_) {
      audio_track_->Close();
      audio_track_.reset();
    }
  });
}

void SimpleAudioAsyncPlayer::Start() {
  task_runner_->PostTask([this]() {
    // Start audio playback here
    if (audio_track_) {
      audio_track_->Start();
      // Start the play loop
      task_runner_->PostTask([this]() { Play(); });
    }
  });
}

void SimpleAudioAsyncPlayer::Stop() {
  task_runner_->PostTaskAndWait([this]() {
    // Stop audio playback here
    if (audio_track_) {
      audio_track_->Stop();
    }
  });
}

void SimpleAudioAsyncPlayer::Play() {
  if (!audio_track_ || !audio_track_->ready()) {
    return;
  }

  // Calculate chunk size for 10ms of audio
  int32_t chunk_size_samples =
      audio_track_->sampleRate() * kWriteChunkSizeMs / 1000;
  int32_t chunk_size_bytes =
      chunk_size_samples * audio_track_->channelCount() * 2;  // 16-bit

  // Write a chunk of audio data
  if (current_position_ + chunk_size_bytes <=
      loop_buffer_.size() * sizeof(int16_t)) {
    // Write from current position
    const uint8_t* chunk_data =
        reinterpret_cast<const uint8_t*>(loop_buffer_.data()) +
        current_position_;
    ssize_t written = audio_track_->Write(chunk_data, chunk_size_bytes, false);
    if (written > 0) {
      current_position_ += written;
    }
  } else {
    // Reset to beginning and continue
    current_position_ = 0;
    const uint8_t* chunk_data =
        reinterpret_cast<const uint8_t*>(loop_buffer_.data());
    ssize_t written = audio_track_->Write(chunk_data, chunk_size_bytes, false);
    if (written > 0) {
      current_position_ = written;
    }
  }

  // Schedule next play cycle
  task_runner_->PostTask([this]() { Play(); });
}

void SimpleAudioAsyncPlayer::Write(const uint8_t* data, size_t size) {
  // Copy data to avoid issues with the original buffer being modified
  std::vector<uint8_t> buffer(data, data + size);
  task_runner_->PostTask([this, buffer = std::move(buffer)]() {
    // Write audio data to the player here
    if (audio_track_ && audio_track_->ready()) {
      audio_track_->Write(buffer.data(), buffer.size(), false);
    }
  });
}

}  // namespace ave
