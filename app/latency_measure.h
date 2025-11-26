/*
 * latency_measure.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#pragma once
#ifndef AVE_LATENCY_MEASURE_LATENCY_MEASURE_H_H_
#define AVE_LATENCY_MEASURE_LATENCY_MEASURE_H_H_

#include <atomic>  // 用于线程安全的阈值
#include <cstddef>
#include <vector>

class LatencyMeasure {
 public:
  /**
   * @brief 构造函数 (已更新)
   * @param core_signal_freq 测试信号的核心频率 (Hz)，例如 4000.0f
   * @param core_signal_duration_ms 测试信号的持续时间 (ms)，例如 30.0f
   * @param record_sample_rate 录音设备的采样率 (Hz)，例如 48000
   * @param record_channel_count 录音设备的通道数，例如 1 (单声道) 或 2 (立体声)
   * @param initial_peak_threshold 初始的波峰检测阈值 (0.0 到 1.0)
   */
  LatencyMeasure(float core_signal_freq,
                 float core_signal_duration_ms,
                 int record_sample_rate,
                 int record_channel_count,
                 float initial_peak_threshold);

  /**
   * @brief 接收并处理一帧音频数据。支持多通道，数据格式为交错排列
   * (interleaved)。 例如，立体声数据格式为 [L0, R0, L1, R1, L2, R2, ...]
   * @param frame 音频数据帧
   */
  void SendFrame(const std::vector<int16_t>& frame);

  /**
   * @brief 获取当前计算出的最新延迟
   * @return 返回检测到的延迟（毫秒）。如果尚未检测到，则返回 -1.0。
   */
  float GetLatency() const;

  /**
   * @brief NEW: 在运行时动态设置波峰检测阈值 (线程安全)
   * @param new_threshold 新的阈值
   */
  void SetPeakThreshold(float new_threshold);

 private:
  // ---------- 配置参数 ----------
  float core_signal_freq_;
  int record_sample_rate_;
  int record_channel_count_;
  int min_peak_distance_;  // 根据信号时长自动计算

  // ---------- 内部数据缓冲 (始终为单声道) ----------
  std::vector<float> audio_buffer_;
  long long total_samples_processed_;

  // ---------- 滤波器和状态 ----------
  float bpf_b0, bpf_b1, bpf_b2, bpf_a1, bpf_a2;
  float bpf_x1, bpf_x2, bpf_y1, bpf_y2;
  float lpf_alpha_;
  float lpf_y1_;

  // ---------- 波峰检测 ----------
  std::atomic<float> peak_threshold_;  // NEW: 使用 atomic 以确保线程安全
  std::vector<long long> peak_positions_;

  // ---------- 延迟结果 ----------
  float last_latency_ms_;

  // ---------- 私有方法 ----------
  void DesignBandpassFilter();
  void ProcessBuffer();
  float ApplyBandpassFilter(float sample);
  float ApplyEnvelopeLPF(float sample);
  void DownmixAndAppend(const std::vector<int16_t>& frame);
};

#endif /* !AVE_LATENCY_MEASURE_LATENCY_MEASURE_H_H_ */
