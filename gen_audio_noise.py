#! /usr/bin/env python3
# vim:fenc=utf-8
#
# Copyright © 2025 youfa <vsyfar@gmail.com>
#
# Distributed under terms of the GPLv2 license.

""" """

import numpy as np
from scipy.io.wavfile import write
import argparse
import sys


def generate_audio(
    total_len_s, freq_hz, sig_dur_ms, delay_ms, noise_level, output_file
):
    """
    核心生成逻辑
    """
    SAMPLE_RATE = 44100
    BLOCK_DURATION_S = 5
    ECHO_DECAY = 0.5  # 回音衰减系数

    # --- 1. 生成 5秒 的基准块 (Clean Signal) ---
    block_samples = int(BLOCK_DURATION_S * SAMPLE_RATE)
    block_data = np.zeros(block_samples, dtype=np.float32)

    # 计算关键时间点的采样索引
    # T0: 原始信号开始时间 (固定为第 1000 ms)
    start_idx_origin = int(1.0 * SAMPLE_RATE)

    # 信号长度采样数
    sig_samples = int((sig_dur_ms / 1000.0) * SAMPLE_RATE)

    # T1: 原始信号结束时间
    end_idx_origin = start_idx_origin + sig_samples

    # T2: 回声信号开始时间 (原始信号开始 + delay_ms) [Start-to-Start]
    start_idx_echo = start_idx_origin + int((delay_ms / 1000.0) * SAMPLE_RATE)

    # T3: 回声信号结束时间
    end_idx_echo = start_idx_echo + sig_samples

    # 校验时间是否超出 5秒 周期
    if end_idx_echo > block_samples:
        print(f"Error: 信号总长度超出 5秒 周期!")
        print(f"计算回声结束点: {end_idx_echo/SAMPLE_RATE:.2f}s, 周期上限: 5.00s")
        print("请减小 延迟时间 或 缩短信号时长。")
        sys.exit(1)

    # --- 2. 制造原始波形 (正弦波 + 淡入淡出) ---
    t = np.linspace(0, sig_dur_ms / 1000.0, sig_samples, endpoint=False)
    raw_signal = 0.8 * np.sin(2 * np.pi * freq_hz * t)

    # 淡入淡出处理
    fade_len = int(sig_samples * 0.1)
    if fade_len > 0:
        raw_signal[:fade_len] *= np.linspace(0, 1, fade_len)
        raw_signal[-fade_len:] *= np.linspace(1, 0, fade_len)

    # --- 3. 填充基准块 ---
    # 使用 += 叠加，以支持 delay < duration 时的信号重叠
    block_data[start_idx_origin:end_idx_origin] += raw_signal
    block_data[start_idx_echo:end_idx_echo] += raw_signal * ECHO_DECAY

    # --- 4. 循环拼接至总时长 ---
    num_repeats = int(np.ceil(total_len_s / BLOCK_DURATION_S))
    final_data = np.tile(block_data, num_repeats)

    # 裁剪到精确时长
    total_samples = int(total_len_s * SAMPLE_RATE)
    final_data = final_data[:total_samples]

    # --- 5. [新增] 叠加全时长的随机噪声 ---
    if noise_level > 0:
        # 使用高斯分布生成白噪声 (Mean=0, Std=noise_level)
        noise = np.random.normal(0, noise_level, total_samples)
        final_data += noise

    # --- 6. 归一化与保存 ---
    # 检查最大振幅，防止爆音 (Clipping)
    max_val = np.max(np.abs(final_data))
    if max_val > 1.0:
        final_data = final_data / max_val
        print(f"注意: 信号+噪声幅度超过上限，已自动归一化 (原Max: {max_val:.2f})")
    elif max_val == 0:
        print("警告: 生成了全静音音频")

    final_int16 = np.int16(final_data * 32767)
    write(output_file, SAMPLE_RATE, final_int16)

    print("-" * 30)
    print(f"生成成功: {output_file}")
    print(f"参数详情:")
    print(f"  - 总时长: {total_len_s} s")
    print(f"  - 频率: {freq_hz} Hz")
    print(f"  - 信号时长: {sig_dur_ms} ms")
    print(f"  - 延迟(Start-to-Start): {delay_ms} ms")
    print(f"  - 噪声等级: {noise_level} (叠加高斯白噪)")
    print("-" * 30)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="生成带回音和噪声的测试音频 (每5s循环)"
    )

    parser.add_argument("--total", type=float, required=True, help="音频总时长 (秒)")
    parser.add_argument(
        "--freq", type=float, required=True, help="原始信号核心频率 (Hz)"
    )
    parser.add_argument(
        "--duration", type=float, required=True, help="原始信号持续时间 x (毫秒)"
    )
    parser.add_argument(
        "--delay",
        type=float,
        required=True,
        help="延迟时间：原始信号开始 到 回声信号开始 的间隔 (毫秒)",
    )
    parser.add_argument(
        "--noise",
        type=float,
        default=0.0,
        help="环境噪声幅度系数 (建议 0.01 - 0.1, 默认 0)",
    )
    parser.add_argument(
        "--out", type=str, default="output.wav", help="输出文件名 (默认 output.wav)"
    )

    args = parser.parse_args()

    generate_audio(
        total_len_s=args.total,
        freq_hz=args.freq,
        sig_dur_ms=args.duration,
        delay_ms=args.delay,
        noise_level=args.noise,
        output_file=args.out,
    )
