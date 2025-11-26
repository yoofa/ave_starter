/*
 * simple_audio_async_player.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_PLAYER_H_H_
#define AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_PLAYER_H_H_

#include "base/task_util/task_runner.h"
#include "base/task_util/task_runner_factory.h"

#include "media/audio/audio_device.h"

namespace ave {

class SimpleAudioAsyncPlayer {
 public:
  SimpleAudioAsyncPlayer(base::TaskRunnerFactory* task_runner_factory,
                         std::shared_ptr<media::AudioDevice> audio_device);
  ~SimpleAudioAsyncPlayer();

  void Init(int32_t sample_rate,
            int channels,
            int32_t core_signal_freq,
            int32_t core_signal_duration_ms);
  void Release();

  void Start();
  void Stop();

 private:
  void Play();
  void Write(const uint8_t* data, size_t size);
  std::unique_ptr<base::TaskRunner> task_runner_;
  std::shared_ptr<media::AudioDevice> audio_device_;
  std::shared_ptr<media::AudioTrack> audio_track_;
  std::vector<int16_t> loop_buffer_;
  size_t current_position_;
};

}  // namespace ave

#endif /* !AVE_LATENCY_MEASURE_SIMPLE_AUDIO_ASYNC_PLAYER_H_H_ */
