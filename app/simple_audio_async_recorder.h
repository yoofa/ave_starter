/*
 * simple_audio_async_recorder.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_RECORDER_H_H_
#define AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_RECORDER_H_H_

#include <memory>

#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"
#include "media/audio/audio_device.h"

namespace ave {

class SimpleAudioAsyncRecorder {
public:
  SimpleAudioAsyncRecorder(base::TaskRunnerFactory *task_runner_factory,
                           std::shared_ptr<media::AudioDevice> audio_device);
  ~SimpleAudioAsyncRecorder();

  void Init(int32_t sample_rate, int32_t channels);
  void Release();

  bool Start();
  void Stop();

  size_t Read(const void *data, size_t size);

private:
  std::unique_ptr<base::TaskRunner> task_runner_;
  std::shared_ptr<media::AudioDevice> audio_device_;
  std::shared_ptr<media::AudioRecord> audio_record_;
};
} // namespace ave

#endif /* !AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_RECORDER_H_H_ */
