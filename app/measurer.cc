/*
 * measurer.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "measurer.h"

#include <iostream>

namespace ave {

Measurer::Measurer(int32_t core_signal_freq_hz, int32_t core_signal_duration_ms,
                   float peak_threshold)
    : task_runner_factory_(base::CreateDefaultTaskRunnerFactory()),
      task_runner_(std::make_unique<base::TaskRunner>(
          task_runner_factory_->CreateTaskRunner(
              "Measurer", base::TaskRunnerFactory::Priority::NORMAL))),
      audio_device_(media::AudioDevice::CreateAudioDevice()),
      player_(std::make_unique<SimpleAudioAsyncPlayer>(
          task_runner_factory_.get(), audio_device_)),
      recorder_(std::make_unique<SimpleAudioAsyncRecorder>(
          task_runner_factory_.get(), audio_device_)),
      core_signal_freq_hz_(core_signal_freq_hz),
      core_signal_duration_ms_(core_signal_duration_ms),
      peak_threshold_(peak_threshold),
      latency_measure_(std::make_unique<AsyncLatencyMeasure>(
          task_runner_factory_.get(),
          core_signal_freq_hz_, core_signal_duration_ms,
          kRecordPlaybackSampleRate, kRecordPlaybackChannelCount,
          peak_threshold_)) {
  audio_device_->Init();
}

Measurer::~Measurer() {}

void Measurer::Init(int32_t core_signal_freq_hz,
                    int32_t core_signal_duration_ms, float peak_threshold) {
  task_runner_->PostTask(
      [this, core_signal_freq_hz, core_signal_duration_ms, peak_threshold]() {
        core_signal_freq_hz_ = core_signal_freq_hz;
        core_signal_duration_ms_ = core_signal_duration_ms;
        peak_threshold_ = peak_threshold;

        latency_measure_ = std::make_unique<AsyncLatencyMeasure>(
            task_runner_factory_.get(),
            core_signal_freq_hz_, core_signal_duration_ms,
            kRecordPlaybackSampleRate, kRecordPlaybackChannelCount,
            peak_threshold_);

        player_->Init(kRecordPlaybackSampleRate, kRecordPlaybackChannelCount,
                      core_signal_freq_hz_, core_signal_duration_ms_);
        recorder_->Init(kRecordPlaybackSampleRate, kRecordPlaybackChannelCount);
      });
}

void Measurer::Release() {
  task_runner_->PostTask([this]() {
    player_->Release();
    recorder_->Release();
    if (latency_measure_) {
      latency_measure_->Stop();
      latency_measure_.reset();
    }
  });
}

void Measurer::Start() {
  task_runner_->PostTask([this]() {
    player_->Start();
    recorder_->Start();
    
    // 启动延迟测量模块
    if (latency_measure_) {
      latency_measure_->Start();
    }
    
    // now player_ is playing test signal
    // recorder_ is recording audio data

    // Start the measurement loop
    task_runner_->PostTask([this]() { ReadDataAndMeasure(); });
  });
}

void Measurer::Stop() {
  task_runner_->PostTask([this]() {
    player_->Stop();
    recorder_->Stop();
    
    // 停止延迟测量模块
    if (latency_measure_) {
      latency_measure_->Stop();
    }
    
    // stop the loop task
  });
}

// read 10ms data each time, and send it to latency_measure_
void Measurer::ReadDataAndMeasure() {
  if (!latency_measure_) {
    return;
  }

  // Calculate 10ms buffer size
  constexpr int32_t kReadChunkMs = 10;
  int32_t chunk_size_samples = kRecordPlaybackSampleRate * kReadChunkMs / 1000;
  int32_t chunk_size_bytes =
      chunk_size_samples * kRecordPlaybackChannelCount * 2; // 16-bit

  // Create buffer for reading audio data
  std::vector<int16_t> audio_buffer(chunk_size_samples *
                                    kRecordPlaybackChannelCount);

  // Read audio data from recorder
  size_t bytes_read = recorder_->Read(audio_buffer.data(), chunk_size_bytes);

  if (bytes_read > 0) {
    // Convert bytes to samples
    size_t samples_read = bytes_read / (kRecordPlaybackChannelCount * 2);
    if (samples_read > 0) {
      // Resize buffer to actual samples read
      audio_buffer.resize(samples_read * kRecordPlaybackChannelCount);

      // Send data to latency measure (异步处理)
      latency_measure_->SendFrame(audio_buffer);
    }
  }

  // Schedule next measurement cycle (every 10ms)
  task_runner_->PostTask([this]() { ReadDataAndMeasure(); });
}

void Measurer::SetPeakThreshold(float new_threshold) {
  task_runner_->PostTask([this, new_threshold]() {
    peak_threshold_ = new_threshold;
    if (latency_measure_) {
      latency_measure_->SetPeakThreshold(new_threshold);
    }
  });
}

int32_t Measurer::GetLatencyMs() {
  // this function should be called in the main thread
  if (latency_measure_) {
    return static_cast<int32_t>(latency_measure_->GetLatency());
  }
  return -1;
}

} // namespace ave
