#!/bin/bash
#
# install-deps.sh
# Modified for GitHub Actions
#
# Distributed under terms of the GPLv2 license.
#

set -e

# 更新 apt 包索引
sudo apt-get update

# 安装必要的包
package_list="
    curl \
    file \
    locales \
    lsb-release \
    python2 \
    python-dbus-dev \
    python-setuptools \
    python3-pip \
    sudo \
    wget \
    lsof \
    libfuse2 \
    software-properties-common \
    ninja-build"

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $package_list

# 设置默认的 Python 解释器为 Python3
sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 30

# 配置 sudoers（可选，通常 GitHub Actions 运行器已配置为无需密码的 sudo）
# 如果确实需要，可以保留以下配置，否则可以注释掉
# echo 'runner ALL=NOPASSWD: ALL' | sudo tee /etc/sudoers.d/50-runner
# echo 'Defaults    env_keep += "DEBIAN_FRONTEND"' | sudo tee /etc/sudoers.d/env_keep

# 创建 setup 目录
mkdir -p $HOME/setup

# 下载 Chromium 的依赖安装脚本
curl https://chromium.googlesource.com/chromium/src/+/HEAD/build/install-build-deps.sh?format=text | base64 --decode > $HOME/setup/install-build-deps.sh
curl https://chromium.googlesource.com/chromium/src/+/HEAD/build/install-build-deps.py?format=text | base64 --decode > $HOME/setup/install-build-deps.py

# 修改 install-build-deps.py 以跳过 snapcraft
sed -i 's/packages.append("snapcraft")/print("skipping snapcraft")/g' $HOME/setup/install-build-deps.py

# 赋予脚本执行权限
chmod +x $HOME/setup/install-build-deps.sh
chmod +x $HOME/setup/install-build-deps.py

# 运行 install-build-deps.sh 脚本
sudo DEBIAN_FRONTEND=noninteractive bash $HOME/setup/install-build-deps.sh --syms --no-prompt --no-chromeos-fonts --no-nacl

# 清理 apt 缓存
sudo rm -rf /var/lib/apt/lists/*

# 可选：移除 setup 目录
rm -rf $HOME/setup

# install depot tols
# https://www.chromium.org/developers/how-tos/install-depot-tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git $HOME/depot_tools