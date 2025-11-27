/*
 * test_independent_control.cc
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

  std::cout << "=== Independent Control Test ===" << std::endl;

  try {
    ave::Measurer measurer;

    std::cout << "Initializing..." << std::endl;
    // Initialization is now synchronous in the constructor.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 1. Start Player Only
    std::cout << "[Step 1] Starting Player Only (Signal should be audible, but "
                 "no latency measured)..."
              << std::endl;
    measurer.StartPlayer();

    for (int i = 0; i < 3; ++i) {
      if (!ave::g_running)
        break;
      std::cout << "  Waiting... (" << i + 1 << "/3)" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      int latency = measurer.GetLatencyMs();
      if (latency != -1) {
        std::cout << "  WARNING: Latency measured but recorder should be off: "
                  << latency << " ms" << std::endl;
      }
    }

    if (!ave::g_running)
      return 0;

    // 2. Start Recorder (Now we should see latency)
    std::cout
        << "\n[Step 2] Starting Recorder (Latency measurement should start)..."
        << std::endl;
    measurer.StartRecorder();

    for (int i = 0; i < 5; ++i) {
      if (!ave::g_running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      int latency = measurer.GetLatencyMs();
      std::cout << "  Latency: " << latency << " ms" << std::endl;
    }

    if (!ave::g_running)
      return 0;

    // 3. Stop Player (Latency measurement might stop updating or become
    // invalid)
    std::cout << "\n[Step 3] Stopping Player (Signal stops)..." << std::endl;
    measurer.StopPlayer();

    for (int i = 0; i < 3; ++i) {
      if (!ave::g_running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      int latency = measurer.GetLatencyMs();
      std::cout << "  Latency (should be stable or -1): " << latency << " ms"
                << std::endl;
    }

    if (!ave::g_running)
      return 0;

    // 4. Stop Recorder
    std::cout << "\n[Step 4] Stopping Recorder..." << std::endl;
    measurer.StopRecorder();

    std::cout << "Cleaning up..." << std::endl;
    measurer.Release();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Test complete." << std::endl;
  return 0;
}
