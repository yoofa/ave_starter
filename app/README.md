# Audio Latency Measurement System

这是一个用于测量音频回声延迟的系统，通过播放测试信号并录制环境声音来检测延迟。

## 系统架构

系统包含以下主要组件：

1. **LatencyMeasure**: 核心延迟测量算法，使用带通滤波器和波峰检测
2. **SimpleAudioAsyncPlayer**: 异步音频播放器，循环播放5秒测试信号
3. **SimpleAudioAsyncRecorder**: 异步音频录制器，持续录制环境声音
4. **Measurer**: 协调播放和录制的管理器

## 测试信号

- 测试信号频率: 4000 Hz (默认)
- 信号持续时间: 30ms (默认)
- 总缓冲区长度: 5秒
- 信号位置: 从1秒开始播放

## 编译

```bash
# 生成构建文件
gn gen out/Default

# 编译
ninja -C out/Default app
```

## 运行

```bash
# 运行延迟测量系统
./out/Default/latency_measure_main
```

## 使用方法

1. 运行程序后，系统会自动初始化音频设备
2. 开始播放测试信号（5秒循环）
3. 同时开始录制环境声音
4. 系统会持续分析录制的音频数据，检测回声延迟
5. 当检测到延迟时，会在控制台输出结果
6. 按 Ctrl+C 停止程序

## 配置参数

可以通过修改 `measurer.h` 中的常量来调整系统参数：

- `kDefaultCoreSignalFreqHz`: 测试信号频率 (默认: 4000 Hz)
- `kDefaultCoreSignalDurationMs`: 信号持续时间 (默认: 30ms)
- `kDefaultPeakThreshold`: 波峰检测阈值 (默认: 0.05)
- `kRecordPlaybackSampleRate`: 采样率 (默认: 48000 Hz)
- `kRecordPlaybackChannelCount`: 通道数 (默认: 2)

## 音频设备要求

系统需要支持以下音频配置：
- 采样率: 48000 Hz
- 格式: PCM 16-bit
- 通道: 立体声 (2通道)

## 注意事项

1. 确保系统有可用的音频输入和输出设备
2. 测试环境需要有一定的回声（如房间、会议室等）
3. 避免在完全消声的环境中测试
4. 系统会自动循环播放测试信号，无需手动干预

## 故障排除

如果遇到音频设备问题：
1. 检查音频设备是否被其他程序占用
2. 确认系统音频权限设置
3. 检查音频设备驱动是否正常
4. 在Linux系统上，确保ALSA或PulseAudio正常工作
