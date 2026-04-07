#!/usr/bin/env bash
set -e

echo "=================================================="
echo " PicoCalc Pico38 Dev Environment Setup (Ubuntu) "
echo "=================================================="

# -------------------------------------------------
# 0. Check OS
# -------------------------------------------------
if ! grep -qi ubuntu /etc/os-release; then
  echo "ERROR: This script is intended for Ubuntu only."
  exit 1
fi

# -------------------------------------------------
# 1. Install system dependencies
# -------------------------------------------------
echo "== Installing system packages =="

corepack enable
corepack prepare yarn@stable --activate
sudo rm -f /etc/apt/sources.list.d/yarn.list

sudo apt update
sudo apt install -y \
  git \
  cmake \
  build-essential \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libstdc++-arm-none-eabi-newlib \
  python3 \
  python3-pip \
  openocd \
  minicom \
  picocom \
  pasmo \
  ripgrep \
  usbutils

# -------------------------------------------------
# 2. Install Pico SDK
# -------------------------------------------------
echo "== Installing Raspberry Pi Pico SDK =="

PICO_SDK_DIR="$HOME/pico-sdk"

if [ ! -d "$PICO_SDK_DIR" ]; then
  git clone https://github.com/raspberrypi/pico-sdk.git "$PICO_SDK_DIR"
  cd "$PICO_SDK_DIR"
  git submodule update --init
  cd -
else
  echo "Pico SDK already exists at $PICO_SDK_DIR"
fi

# -------------------------------------------------
# 3. Export environment variables
# -------------------------------------------------
echo "== Configuring environment variables =="

PROFILE_FILE="$HOME/.bashrc"

if ! grep -q "PICO_SDK_PATH" "$PROFILE_FILE"; then
  echo "export PICO_SDK_PATH=$PICO_SDK_DIR" >> "$PROFILE_FILE"
  echo "PICO_SDK_PATH added to ~/.bashrc"
else
  echo "PICO_SDK_PATH already present in ~/.bashrc"
fi

export PICO_SDK_PATH="$PICO_SDK_DIR"

# -------------------------------------------------
# 4. Sanity checks
# -------------------------------------------------
echo "== Toolchain sanity check =="

arm-none-eabi-gcc --version | head -n 1
cmake --version | head -n 1
echo "PICO_SDK_PATH=$PICO_SDK_PATH"

echo
echo "============================================"
echo " Pico38 PicoCalc environment ready"
echo "============================================"
