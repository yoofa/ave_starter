/*
 * async_latency_measure.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "async_latency_measure.h"

#include <iostream>

namespace ave {

AsyncLatencyMeasure::AsyncLatencyMeasure(
    ave::base::TaskRunnerFactory* task_runner_factory,
    float core_signal_freq,
    float core_signal_duration_ms,
    int record_sample_rate,
    int record_channel_count,
    float initial_peak_threshold)
    : task_runner_(std::make_unique<ave::base::TaskRunner>(
          task_runner_factory->CreateTaskRunner(
              "AsyncLatencyMeasure",
              ave::base::TaskRunnerFactory::Priority::NORMAL))),
      running_(false),
      latency_measure_(
          std::make_unique<LatencyMeasure>(core_signal_freq,
                                           core_signal_duration_ms,
                                           record_sample_rate,
                                           record_channel_count,
                                           initial_peak_threshold)),
      buffer_size_(0) {
  // 预分配缓冲区
  audio_buffer_.reserve(record_sample_rate * 2);  // 2秒的缓冲区
}

AsyncLatencyMeasure::~AsyncLatencyMeasure() {
  Stop();
}

void AsyncLatencyMeasure::Start() {
  running_.store(true);
  std::cout << "[AsyncLatencyMeasure] Started asynchronous processing"
            << std::endl;
}

void AsyncLatencyMeasure::Stop() {
  running_.store(false);
  std::cout << "[AsyncLatencyMeasure] Stopped asynchronous processing"
            << std::endl;
}

void AsyncLatencyMeasure::SendFrame(const std::vector<int16_t>& frame) {
  if (!running_.load()) {
    return;
  }

  // 将数据添加到缓冲区
  {
    // 这里可以添加线程安全保护，如果需要的话
    audio_buffer_.insert(audio_buffer_.end(), frame.begin(), frame.end());
    buffer_size_.store(audio_buffer_.size());
  }

  // 异步触发处理，避免阻塞调用线程
  if (audio_buffer_.size() >
      static_cast<size_t>(latency_measure_->GetLatency() >= 0 ? 48000 : 4800)) {
    task_runner_->PostTask([this]() { ProcessBufferAsync(); });
  }
}

void AsyncLatencyMeasure::ProcessBufferAsync() {
  // std::cout << "[AsyncLatencyMeasure] Processing buffer asynchronously"
  //           << std::endl;
  if (!running_.load()) {
    return;
  }

  // 获取当前缓冲区大小
  size_t current_buffer_size = buffer_size_.load();

  if (current_buffer_size == 0) {
    return;
  }

  // 创建要处理的数据副本
  std::vector<int16_t> data_to_process;
  {
    // 这里可以添加线程安全保护，如果需要的话
    data_to_process.swap(audio_buffer_);
    buffer_size_.store(0);
  }

  // 将数据发送给LatencyMeasure进行处理
  if (latency_measure_) {
    latency_measure_->SendFrame(data_to_process);
  }

  // 如果还有足够的数据需要处理，继续异步处理
  if (buffer_size_.load() > 4800 && running_.load()) {
    task_runner_->PostTask([this]() { ProcessBufferAsync(); });
  }
}

float AsyncLatencyMeasure::GetLatency() const {
  if (latency_measure_) {
    return latency_measure_->GetLatency();
  }
  return -1.0f;
}

void AsyncLatencyMeasure::SetPeakThreshold(float new_threshold) {
  if (latency_measure_) {
    latency_measure_->SetPeakThreshold(new_threshold);
  }
}

}  // namespace ave
