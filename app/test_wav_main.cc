/*
 * test_wav_main.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

// main2.cpp
#include <cstdint>
#include <iostream>
#include <vector>

// 定义这个宏，dr_wav.h 就会包含它的实现代码
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "latency_measure.h"

void print_error_and_exit(const std::string& message) {
  std::cerr << "ERROR: " << message << std::endl;
  exit(1);
}

int main(int argc, char* argv[]) {
  // --- 配置 (必须与ffmpeg命令中的参数严格对应) ---
  const float CORE_FREQ = 4000.0f;
  const float CORE_DURATION_MS = 10.0f;
  const float INITIAL_THRESHOLD = 0.5f;
  const float EXPECTED_LATENCY_MS = 50.0f;
  const char* WAV_FILENAME = "test_latency.wav";

  // argv[1] is file path
  if (argc > 1) {
    WAV_FILENAME = argv[1];
  }

  std::cout << "WAV Latency Test Application" << std::endl;
  std::cout << "Using WAV file: " << WAV_FILENAME << std::endl;

  // --- 1. 使用 dr_wav 读取 WAV 文件 ---
  unsigned int channels;
  unsigned int sampleRate;
  drwav_uint64 totalFrameCount;
  int16_t* pSampleData = drwav_open_file_and_read_pcm_frames_s16(
      WAV_FILENAME, &channels, &sampleRate, &totalFrameCount, NULL);

  if (pSampleData == NULL) {
    print_error_and_exit("Failed to open and read WAV file: " +
                         std::string(WAV_FILENAME));
  }

  std::cout << "Successfully loaded WAV file." << std::endl;
  std::cout << "  - Channels: " << channels << std::endl;
  std::cout << "  - Sample Rate: " << sampleRate << std::endl;
  std::cout << "  - Total Frames: " << totalFrameCount << " ("
            << (float)totalFrameCount / sampleRate << " seconds)" << std::endl;

  // --- 2. 初始化延迟检测器 ---
  LatencyMeasure latency_detector(CORE_FREQ, CORE_DURATION_MS, sampleRate,
                                  channels, INITIAL_THRESHOLD);

  // --- 3. 模拟实时流，分块处理音频数据 ---
  std::cout << "\nProcessing audio stream..." << std::endl;

  const size_t totalSampleCount = totalFrameCount * channels;
  const int frame_size_interleaved = 480 * channels;  // 每次处理 10ms 的数据

  std::vector<int16_t> frame(frame_size_interleaved);
  size_t samples_processed = 0;

  for (size_t i = 0; i < totalSampleCount; i += frame_size_interleaved) {
    size_t samples_to_copy =
        std::min((size_t)frame_size_interleaved, totalSampleCount - i);
    frame.assign(pSampleData + i, pSampleData + i + samples_to_copy);

    latency_detector.SendFrame(frame);

    // 每处理5秒的数据，就打印一次当前检测到的延迟
    size_t current_processed_mono_samples = i / channels;
    if (current_processed_mono_samples > samples_processed + sampleRate * 5) {
      samples_processed = current_processed_mono_samples;
      std::cout << "Time: " << (float)samples_processed / sampleRate
                << "s, Current detected latency: "
                << latency_detector.GetLatency() << " ms" << std::endl;
    }
  }

  // 确保处理缓冲区中剩余的最后数据
  latency_detector.SendFrame({});

  // --- 4. 清理并报告最终结果 ---
  drwav_free(pSampleData, NULL);  // 释放 dr_wav 分配的内存

  std::cout << "\n--- Final Results ---" << std::endl;
  std::cout << "Expected latency was: " << EXPECTED_LATENCY_MS << " ms"
            << std::endl;
  float final_latency = latency_detector.GetLatency();
  if (final_latency > 0) {
    std::cout << "Last detected latency: " << final_latency << " ms"
              << std::endl;
  } else {
    std::cout << "Could not detect any valid latency in the WAV file."
              << std::endl;
  }

  return 0;
}
