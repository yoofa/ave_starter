/*
 * main.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <signal.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

#include "measurer.h"

namespace ave {

volatile bool g_running = true;

void SignalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    std::cout << "\nReceived signal " << signal << ", shutting down..."
              << std::endl;
    g_running = false;
  }
}

}  // namespace ave

int main(int argc, char* argv[]) {
  // Set up signal handlers
  signal(SIGINT, ave::SignalHandler);
  signal(SIGTERM, ave::SignalHandler);

  std::cout << "=== Audio Latency Measurement System ===" << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;

  class MainCallback : public ave::Measurer::Callback {
   public:
    void OnData(const std::vector<float>& data) override {
      if (data.empty())
        return;
      float sum_sq = 0.0f;
      for (float s : data)
        sum_sq += s * s;
      float rms = std::sqrt(sum_sq / data.size());
      static int32_t print_count = 0;
      print_count++;
      if (print_count % 40 == 0)  // Print every 50 calls
      {
        std::cout << "[Callback] OnData RMS: " << rms << std::endl;
      }
    }

    void OnLatencyUpdate(const float latency) override {
      std::cout << "[Callback] OnLatencyUpdate: " << latency << " ms"
                << std::endl;
    }
  };

  try {
    MainCallback callback;
    // Create and initialize the measurer
    ave::Measurer measurer(
        &callback,
        ave::kDefaultCoreSignalFreqHz,      // 4000 Hz test signal
        ave::kDefaultCoreSignalDurationMs,  // 10ms duration
        ave::kDefaultPeakThreshold);        // 0.3 threshold

    std::cout << "Initializing audio system..." << std::endl;
    measurer.Start();

    // Main loop - continuously check for latency updates
    while (ave::g_running) {
      // Check every 100ms
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Stopping measurement..." << std::endl;
    measurer.Stop();

    std::cout << "Final latency: " << measurer.GetLatencyMs() << " ms"
              << std::endl;

    std::cout << "Cleaning up..." << std::endl;
    measurer.Release();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "System shutdown complete." << std::endl;
  return 0;
}
