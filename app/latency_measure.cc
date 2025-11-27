/*
 * latency_measure.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "latency_measure.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LatencyMeasure::LatencyMeasure(float core_signal_freq,
                               float core_signal_duration_ms,
                               int record_sample_rate,
                               int record_channel_count,
                               float initial_peak_threshold)
    : core_signal_freq_(core_signal_freq),
      record_sample_rate_(record_sample_rate),
      record_channel_count_(record_channel_count),
      total_samples_processed_(0),
      // 滤波器状态初始化
      bpf_b0(0.0f),
      bpf_b1(0.0f),
      bpf_b2(0.0f),
      bpf_a1(0.0f),
      bpf_x1(0.0f),
      bpf_x2(0.0f),
      lpf_alpha_(0.0f),
      lpf_y1_(0.0f),
      peak_threshold_(initial_peak_threshold),  // 初始化 atomic 变量
      last_latency_ms_(-1.0f) {
  if (record_channel_count_ <= 0) {
    // 防止除以零或其他无效通道数
    record_channel_count_ = 1;
  }

  DesignBandpassFilter();

  float lpf_cutoff = 50.0f;
  float rc = 1.0f / (2.0f * M_PI * lpf_cutoff);
  lpf_alpha_ =
      (1.0f / record_sample_rate_) / (rc + (1.0f / record_sample_rate_));

  // CHANGED: 自动计算最小波峰距离
  // 在信号时长的基础上增加 5ms 的安全间隔
  float min_dist_ms = core_signal_duration_ms + 5.0f;
  min_peak_distance_ =
      static_cast<int>((min_dist_ms / 1000.0f) * record_sample_rate_);

  audio_buffer_.reserve(record_sample_rate_ * 10);

  std::cout << "[Config] Min peak distance set to " << min_dist_ms << " ms ("
            << min_peak_distance_ << " samples)." << std::endl;
}

void LatencyMeasure::SetPeakThreshold(float new_threshold) {
  peak_threshold_.store(new_threshold);  // 使用 store() 来写入 atomic 变量
}

void LatencyMeasure::DownmixAndAppend(const std::vector<int16_t>& frame) {
  if (record_channel_count_ == 1) {
    // 单声道，直接添加
    audio_buffer_.insert(audio_buffer_.end(), frame.begin(), frame.end());
  } else {
    // 多通道，进行缩混 (取平均值)
    size_t num_samples = frame.size() / record_channel_count_;
    audio_buffer_.reserve(audio_buffer_.size() + num_samples);
    for (size_t i = 0; i < frame.size(); i += record_channel_count_) {
      float mono_sample = 0.0f;
      for (int ch = 0; ch < record_channel_count_; ++ch) {
        mono_sample += frame[i + ch];
      }
      audio_buffer_.push_back(mono_sample / record_channel_count_);
    }
  }
}

// CHANGED: sendFrame 现在调用 downmixAndAppend
void LatencyMeasure::SendFrame(const std::vector<int16_t>& frame) {
  // dump frame to file

  DownmixAndAppend(frame);
  // 触发处理的逻辑保持不变
  if (audio_buffer_.size() > static_cast<size_t>(record_sample_rate_)) {
    ProcessBuffer();
  }
}

void LatencyMeasure::DesignBandpassFilter() {
  float Q = 5.0f;  // 对于更高频率的信号，更高的Q值有助于滤除噪声
  float w0 = 2.0f * M_PI * core_signal_freq_ / record_sample_rate_;
  float cos_w0 = std::cos(w0);
  float sin_w0 = std::sin(w0);
  float alpha = sin_w0 / (2.0f * Q);
  float a0 = 1.0f + alpha;
  bpf_b0 = alpha / a0;
  bpf_b1 = 0.0f;
  bpf_b2 = -alpha / a0;
  bpf_a1 = -2.0f * cos_w0 / a0;
  bpf_a2 = (1.0f - alpha) / a0;
}

float LatencyMeasure::ApplyBandpassFilter(float sample) {
  float result = bpf_b0 * sample + bpf_x1;
  bpf_x1 = bpf_b1 * sample - bpf_a1 * result + bpf_x2;
  bpf_x2 = bpf_b2 * sample - bpf_a2 * result;
  return result;
}

float LatencyMeasure::ApplyEnvelopeLPF(float sample) {
  lpf_y1_ = lpf_y1_ + lpf_alpha_ * (sample - lpf_y1_);
  return lpf_y1_;
}

float LatencyMeasure::GetLatency() const {
  return last_latency_ms_;
}

// 完整的 processBuffer
void LatencyMeasure::ProcessBuffer() {
  if (audio_buffer_.empty())
    return;

  std::vector<float> envelope(audio_buffer_.size());
  for (size_t i = 0; i < audio_buffer_.size(); ++i) {
    float filtered = ApplyBandpassFilter(audio_buffer_[i]);
    envelope[i] = ApplyEnvelopeLPF(std::abs(filtered));
  }

  float current_threshold =
      peak_threshold_.load();  // 使用 load() 来读取 atomic 变量

  std::vector<long long> candidates;
  for (size_t i = 1; i < envelope.size() - 1; ++i) {
    if (envelope[i] > current_threshold && envelope[i] > envelope[i - 1] &&
        envelope[i] > envelope[i + 1]) {
      candidates.push_back(total_samples_processed_ + i);
    }
  }

  for (long long cand_pos : candidates) {
    if (peak_positions_.empty() ||
        (cand_pos - peak_positions_.back()) > min_peak_distance_) {
      peak_positions_.push_back(cand_pos);
    }
  }

  while (peak_positions_.size() >= 2) {
    long long latency_samples = peak_positions_[1] - peak_positions_[0];
    if (latency_samples > 0 && latency_samples < 4.0 * record_sample_rate_) {
      last_latency_ms_ =
          static_cast<float>(latency_samples * 1000.0 / record_sample_rate_);
      std::cout << "[SUCCESS] Latency detected: " << last_latency_ms_
                << " ms. (Peaks at " << peak_positions_[0] << " and "
                << peak_positions_[1] << ")" << std::endl;
    }
    peak_positions_.erase(peak_positions_.begin(), peak_positions_.begin() + 2);
  }

  long long samples_to_remove = 0;
  if (!peak_positions_.empty()) {
    samples_to_remove = peak_positions_[0] - total_samples_processed_;
  } else {
    if (audio_buffer_.size() > (0.1 * record_sample_rate_)) {
      samples_to_remove = audio_buffer_.size() - (0.1 * record_sample_rate_);
    }
  }
  if (samples_to_remove > 0 &&
      samples_to_remove <= (long long)audio_buffer_.size()) {
    audio_buffer_.erase(audio_buffer_.begin(),
                        audio_buffer_.begin() + samples_to_remove);
    total_samples_processed_ += samples_to_remove;
  }
}
