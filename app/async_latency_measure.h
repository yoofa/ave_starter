/*
 * async_latency_measure.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#pragma once
#ifndef AVE_LATENCY_MEASURE_ASYNC_LATENCY_MEASURE_H_H_
#define AVE_LATENCY_MEASURE_ASYNC_LATENCY_MEASURE_H_H_

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "latency_measure.h"

namespace ave {

/**
 * @brief AsyncLatencyMeasure - LatencyMeasure的异步包装器
 *
 * 这个类将LatencyMeasure包装起来，使用task runner进行异步处理，
 * 避免数据读取和处理阻塞主线程。对外接口与LatencyMeasure完全一致。
 */
class AsyncLatencyMeasure {
 public:
  using OnDataCallback = std::function<void(const std::vector<float>&)>;
  using OnLatencyUpdateCallback = std::function<void(float)>;

  /**
   * @brief 构造函数
   * @param task_runner_factory 任务运行器工厂
   * @param core_signal_freq 测试信号的核心频率 (Hz)
   * @param core_signal_duration_ms 测试信号的持续时间 (ms)
   * @param record_sample_rate 录音设备的采样率 (Hz)
   * @param record_channel_count 录音设备的通道数
   * @param initial_peak_threshold 初始的波峰检测阈值
   * @param on_data_callback 音频数据回调
   * @param on_latency_update_callback 延迟更新回调
   */
  AsyncLatencyMeasure(
      ave::base::TaskRunnerFactory* task_runner_factory,
      float core_signal_freq,
      float core_signal_duration_ms,
      int record_sample_rate,
      int record_channel_count,
      float initial_peak_threshold,
      OnDataCallback on_data_callback = nullptr,
      OnLatencyUpdateCallback on_latency_update_callback = nullptr);

  /**
   * @brief 析构函数
   */
  ~AsyncLatencyMeasure();

  /**
   * @brief 启动异步处理
   */
  void Start();

  /**
   * @brief 停止异步处理
   */
  void Stop();

  /**
   * @brief 接收并处理一帧音频数据（异步）
   * @param frame 音频数据帧
   */
  void SendFrame(const std::vector<int16_t>& frame);

  /**
   * @brief 获取当前计算出的最新延迟
   * @return 返回检测到的延迟（毫秒）。如果尚未检测到，则返回 -1.0。
   */
  float GetLatency() const;

  /**
   * @brief 在运行时动态设置波峰检测阈值 (线程安全)
   * @param new_threshold 新的阈值
   */
  void SetPeakThreshold(float new_threshold);

 private:
  /**
   * @brief 异步处理缓冲区数据
   */
  void ProcessBufferAsync();

  // 任务运行器
  std::unique_ptr<ave::base::TaskRunner> task_runner_;
  std::atomic<bool> running_;

  // 被包装的LatencyMeasure实例
  std::unique_ptr<LatencyMeasure> latency_measure_;

  // 音频数据缓冲区（用于异步处理）
  std::vector<int16_t> audio_buffer_;
  std::atomic<size_t> buffer_size_;

  OnDataCallback on_data_callback_;
  OnLatencyUpdateCallback on_latency_update_callback_;
};

}  // namespace ave

#endif /* !AVE_LATENCY_MEASURE_ASYNC_LATENCY_MEASURE_H_H_ */
