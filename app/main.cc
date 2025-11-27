/*
 * main.cc
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include <signal.h>
#include <chrono>
#include <iostream>
#include <thread>

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

  try {
    // Create and initialize the measurer
    ave::Measurer measurer(
        ave::kDefaultCoreSignalFreqHz,      // 4000 Hz test signal
        ave::kDefaultCoreSignalDurationMs,  // 10ms duration
        ave::kDefaultPeakThreshold          // 0.3 threshold
    );

    std::cout << "Initializing audio system..." << std::endl;
    measurer.Start();

    // Main loop - continuously check for latency updates
    int last_latency = -1;
    while (ave::g_running) {
      int current_latency = measurer.GetLatencyMs();
      // std::cout << "Current latency: " << current_latency << " ms"
      //           << std::endl;

      if (current_latency != last_latency && current_latency >= 0) {
        std::cout << "Measured latency: " << current_latency << " ms"
                  << std::endl;
        last_latency = current_latency;
      }

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
