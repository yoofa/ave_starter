/*
 * measurer.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_LATENCY_MEASURE_MEASURE_H_H_
#define AVE_LATENCY_MEASURE_MEASURE_H_H_
#include <memory>

#include "base/task_util/default_task_runner_factory.h"
#include "base/task_util/task_runner_factory.h"
#include "media/audio/audio_device.h"

#include "async_latency_measure.h"
#include "simple_audio_async_player.h"
#include "simple_audio_async_recorder.h"

namespace ave {

constexpr int32_t kDefaultCoreSignalFreqHz = 4000;
constexpr int32_t kDefaultCoreSignalDurationMs = 30;
constexpr float kDefaultPeakThreshold = 0.05f;
constexpr int32_t kRecordPlaybackSampleRate = 48000;
constexpr int32_t kRecordPlaybackChannelCount = 2;

class Measurer {
public:
  class Callback {
    void OnData(const void *data, size_t size) {}
    void OnLatencyUpdate(int32_t latency_ms) {}
  };

  Measurer(int32_t core_signal_freq_hz = kDefaultCoreSignalFreqHz,
           int32_t core_signal_duration_ms = kDefaultCoreSignalDurationMs,
           float peak_threshold = kDefaultPeakThreshold);
  ~Measurer();

  void Init(int32_t core_signal_freq_hz = kDefaultCoreSignalFreqHz,
            int32_t core_signal_duration_ms = kDefaultCoreSignalDurationMs,
            float peak_threshold = kDefaultPeakThreshold);
  void Release();
  void Start();
  void Stop();

  // set peak threadshold on the fly
  void SetPeakThreshold(float new_threshold);

  int32_t GetLatencyMs();

private:
  void ReadDataAndMeasure(); // run in task runner thread

  std::unique_ptr<base::TaskRunnerFactory> task_runner_factory_;
  std::shared_ptr<base::TaskRunner> task_runner_;
  std::shared_ptr<media::AudioDevice> audio_device_;

  std::unique_ptr<SimpleAudioAsyncPlayer> player_;
  std::unique_ptr<SimpleAudioAsyncRecorder> recorder_;

  int32_t core_signal_freq_hz_;
  int32_t core_signal_duration_ms_;
  int32_t peak_threshold_;
  std::unique_ptr<AsyncLatencyMeasure> latency_measure_;
};

} // namespace ave

#endif /* !AVE_LATENCY_MEASURE_MEASURE_H_H_ */
