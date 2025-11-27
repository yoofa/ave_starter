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

LatencyMeasure::LatencyMeasure(
    float core_signal_freq,
    float core_signal_duration_ms,
    int record_sample_rate,
    int record_channel_count,
    float initial_peak_threshold,
    OnDataCallback on_data_callback,
    OnLatencyUpdateCallback on_latency_update_callback)
    : core_signal_freq_(core_signal_freq),
      record_sample_rate_(record_sample_rate),
      record_channel_count_(record_channel_count),
      min_peak_distance_(0),
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
      last_latency_ms_(-1.0f),
      on_data_callback_(on_data_callback),
      on_latency_update_callback_(on_latency_update_callback) {
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
  std::vector<float> new_samples;
  if (record_channel_count_ == 1) {
    // 单声道，直接添加并归一化
    new_samples.reserve(frame.size());
    for (int16_t sample : frame) {
      float val = sample / 32768.0f;
      audio_buffer_.push_back(val);
      new_samples.push_back(val);
    }
  } else {
    // 多通道，进行缩混 (取平均值) 并归一化
    size_t num_samples = frame.size() / record_channel_count_;
    audio_buffer_.reserve(audio_buffer_.size() + num_samples);
    new_samples.reserve(num_samples);
    for (size_t i = 0; i < frame.size(); i += record_channel_count_) {
      float mono_sample = 0.0f;
      for (int ch = 0; ch < record_channel_count_; ++ch) {
        mono_sample += frame[i + ch];
      }
      float val = (mono_sample / record_channel_count_) / 32768.0f;
      audio_buffer_.push_back(val);
      new_samples.push_back(val);
    }
  }

  if (on_data_callback_) {
    on_data_callback_(new_samples);
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
  // Fast Attack, Slow Release
  if (sample > lpf_y1_) {
    // Attack phase: much faster response (e.g. alpha = 0.8 or 1.0)
    lpf_y1_ = 0.8f * sample + 0.2f * lpf_y1_;
  } else {
    // Release phase: use the calculated LPF alpha (slow decay)
    lpf_y1_ = lpf_y1_ + lpf_alpha_ * (sample - lpf_y1_);
  }
  return lpf_y1_;
}

float LatencyMeasure::GetLatency() const {
  return last_latency_ms_;
}

// 完整的 processBuffer
void LatencyMeasure::ProcessBuffer() {
  if (audio_buffer_.empty())
    return;

  float max_raw = 0.0f;
  float max_filtered = 0.0f;
  std::vector<float> envelope(audio_buffer_.size());
  for (size_t i = 0; i < audio_buffer_.size(); ++i) {
    max_raw = std::max(max_raw, std::abs(audio_buffer_[i]));
    float filtered = ApplyBandpassFilter(audio_buffer_[i]);
    max_filtered = std::max(max_filtered, std::abs(filtered));
    envelope[i] = ApplyEnvelopeLPF(std::abs(filtered));
  }
  // std::cout << "[DEBUG] Max Raw: " << max_raw << ", Max Filtered: " <<
  // max_filtered << std::endl;

  float current_threshold = peak_threshold_.load();
  float reset_threshold = current_threshold * 0.6f;

  float max_env = 0.0f;
  for (float v : envelope)
    max_env = std::max(max_env, v);
  // std::cout << "[DEBUG] Max envelope: " << max_env << ", Threshold: " <<
  // current_threshold << std::endl;

  std::vector<PeakInfo> new_peaks_info;

  bool in_pulse = false;
  float current_max_val = 0.0f;
  size_t current_max_pos = 0;
  size_t last_peak_pos = 0;
  bool has_last_peak = false;

  // 如果 peak_positions_ 不为空，我们需要考虑最后一个已检测到的波峰位置
  // 以便正确应用 min_peak_distance_
  if (!peak_positions_.empty()) {
    long long last_abs_pos = peak_positions_.back().pos;
    if (last_abs_pos >= total_samples_processed_) {
      last_peak_pos = last_abs_pos - total_samples_processed_;
      has_last_peak = true;
    }
  }

  for (size_t i = 0; i < envelope.size(); ++i) {
    float val = envelope[i];
    if (!in_pulse) {
      if (val > current_threshold) {
        // 检查与上一个波峰的距离 (Cooldown)
        // 如果在 cooldown 内，但当前值比旧波峰大，我们也应该进入 in_pulse
        // 以便有机会替换它
        if (!has_last_peak ||
            (i > last_peak_pos &&
             (i - last_peak_pos) > static_cast<size_t>(min_peak_distance_))) {
          in_pulse = true;
          current_max_val = val;
          current_max_pos = i;
        } else {
          // Cooldown 内
          // 检查是否比旧波峰大
          // 我们需要知道旧波峰的值。现在 peak_positions_ 存储了值。
          // 如果比旧波峰大，我们开启新脉冲，并标记需要检查替换
          if (!peak_positions_.empty()) {
            float last_val = peak_positions_.back().val;
            if (val > last_val) {
              in_pulse = true;
              current_max_val = val;
              current_max_pos = i;
            }
          }
        }
      }
    } else {
      // In pulse: 寻找脉冲内的最大值
      if (val > current_max_val) {
        current_max_val = val;
        current_max_pos = i;
      }

      // 迟滞比较: 只有当信号下降到 reset_threshold 以下时，才确认波峰结束
      if (val < reset_threshold) {
        in_pulse = false;
        long long abs_pos = total_samples_processed_ + current_max_pos;

        bool added = false;
        // 1. 检查与 new_peaks_info 中最后一个波峰的冲突
        if (!new_peaks_info.empty()) {
          PeakInfo& last_peak = new_peaks_info.back();
          if (abs_pos - last_peak.pos < min_peak_distance_) {
            // 冲突：保留较大的那个
            if (current_max_val > last_peak.val) {
              last_peak.pos = abs_pos;
              last_peak.val = current_max_val;
            }
            added = true;
          }
        }

        // 2. 检查与 peak_positions_ (旧波峰) 的冲突
        if (!added && !peak_positions_.empty()) {
          PeakInfo& last_old_peak = peak_positions_.back();
          if (abs_pos - last_old_peak.pos < min_peak_distance_) {
            if (current_max_val > last_old_peak.val) {
              // 替换旧波峰
              last_old_peak.pos = abs_pos;
              last_old_peak.val = current_max_val;
              // 更新 last_peak_pos 以便后续逻辑正确
              if (abs_pos >= total_samples_processed_) {
                last_peak_pos = abs_pos - total_samples_processed_;
                has_last_peak = true;
              }
            }
            added = true;
          }
        }

        // 3. 如果没有合并，则添加为新波峰
        if (!added) {
          if (peak_positions_.empty() || abs_pos > peak_positions_.back().pos) {
            new_peaks_info.push_back({abs_pos, current_max_val});
          }
        }

        last_peak_pos = current_max_pos;
      }
    }
  }

  // 将新检测到的波峰加入列表
  for (const auto& p : new_peaks_info) {
    peak_positions_.push_back(p);
  }

  while (peak_positions_.size() >= 2) {
    // Heuristic: The Source signal is usually stronger than the Echo.
    // If Peak[0] < Peak[1], it's likely that Peak[0] is noise/pre-echo and
    // Peak[1] is the true Source.
    if (peak_positions_[0].val < peak_positions_[1].val) {
      peak_positions_.erase(peak_positions_.begin());
      continue;
    }

    long long latency_samples = peak_positions_[1].pos - peak_positions_[0].pos;
    if (latency_samples > 0 && latency_samples < 4.0 * record_sample_rate_) {
      last_latency_ms_ =
          static_cast<float>(latency_samples * 1000.0 / record_sample_rate_);
      std::cout << "[SUCCESS] Latency detected: " << last_latency_ms_
                << " ms. (Peaks at " << peak_positions_[0].pos << " and "
                << peak_positions_[1].pos << ")" << std::endl;
      if (on_latency_update_callback_) {
        on_latency_update_callback_(last_latency_ms_);
      }
    }
    peak_positions_.erase(peak_positions_.begin(), peak_positions_.begin() + 2);
  }

  long long samples_to_remove = 0;
  if (!peak_positions_.empty()) {
    if (peak_positions_[0].pos > total_samples_processed_) {
      samples_to_remove = peak_positions_[0].pos - total_samples_processed_;
    }
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
