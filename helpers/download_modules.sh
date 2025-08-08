#!/bin/bash
BRANCH="android-14-release-uhas34.29"
TAG="MMI-THAS33.31-40-3"

# MET performance driver v3
git clone --branch "$BRANCH" --single-branch \
  https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-met_drv_v3 \
  vendor/mediatek/kernel_modules/met_drv_v3  # :contentReference[oaicite:0]{index=0}

# GPU platform support for MT6768
git clone --branch "$BRANCH" --single-branch \
  https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-gpu \
  vendor/mediatek/kernel_modules/gpu/platform/mt6768  # :contentReference[oaicite:1]{index=1}

# Common connectivity helpers
git clone --branch "$BRANCH" --single-branch \
  https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-common \
  vendor/mediatek/kernel_modules/connectivity/common  # :contentReference[oaicite:2]{index=2}

# FM-radio driver
git clone --branch "$BRANCH" --single-branch \
  https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-fmradio \
  vendor/mediatek/kernel_modules/connectivity/fmradio  # :contentReference[oaicite:3]{index=3}

# GPS
git clone --branch "$BRANCH" --single-branch https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-gps \
  vendor/mediatek/kernel_modules/connectivity/gps

# FEM (Front-End Module)
git clone --branch "$BRANCH" --single-branch https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-connfem \
  vendor/mediatek/kernel_modules/connectivity/connfem

# Connectivity Infrastructure
git clone --branch "$TAG" --single-branch https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-conninfra.git \
  vendor/mediatek/kernel_modules/connectivity/conninfra
