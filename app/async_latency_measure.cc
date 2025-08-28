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
    base::TaskRunnerFactory* task_runner_factory,
    float core_signal_freq, float core_signal_duration_ms,
    int record_sample_rate, int record_channel_count,
    float initial_peak_threshold)
    : task_runner_(std::make_unique<base::TaskRunner>(
          task_runner_factory->CreateTaskRunner(
              "AsyncLatencyMeasure", base::TaskRunnerFactory::Priority::NORMAL))),
      latency_measure_(std::make_unique<LatencyMeasure>(
          core_signal_freq, core_signal_duration_ms,
          record_sample_rate, record_channel_count,
          initial_peak_threshold)) {
}

AsyncLatencyMeasure::~AsyncLatencyMeasure() {
  Stop();
}

void AsyncLatencyMeasure::Start() {
  running_.store(true);
  std::cout << "[AsyncLatencyMeasure] Started asynchronous processing" << std::endl;
}

void AsyncLatencyMeasure::Stop() {
  running_.store(false);
  std::cout << "[AsyncLatencyMeasure] Stopped asynchronous processing" << std::endl;
}

void AsyncLatencyMeasure::SendFrame(const std::vector<int16_t> &frame) {
  if (!running_.load()) {
    return;
  }

  // 将对 LatencyMeasure 的所有访问封装到 task_runner_ 的任务中
  task_runner_->PostTask([this, frame]() {
    if (!running_.load()) {
      return;
    }
    if (latency_measure_) {
      latency_measure_->SendFrame(frame);
      latest_latency_ms_.store(latency_measure_->GetLatency());
    }
  });
}

// 移除本地缓冲与分片处理，统一在 task_runner_ 线程直接驱动 LatencyMeasure

float AsyncLatencyMeasure::GetLatency() const {
  return latest_latency_ms_.load();
}

void AsyncLatencyMeasure::SetPeakThreshold(float new_threshold) {
  // 在专用线程更新阈值，避免跨线程访问
  task_runner_->PostTask([this, new_threshold]() {
    if (latency_measure_) {
      latency_measure_->SetPeakThreshold(new_threshold);
    }
  });
}

} // namespace ave
