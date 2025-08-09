#!/bin/bash
set -e

my_top_dir=$PWD

echo "[I] Creating kernel build structure..."
mkdir -vp "$my_top_dir/kernel"
cd "$my_top_dir/kernel"
git clone https://android.googlesource.com/kernel/build

echo "[I] Setting up Clang toolchain..."
mkdir -vp "$my_top_dir/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b"
cd "$my_top_dir/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b"

if [ ! -d "$my_top_dir/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b/bin" ]; then
    echo "[I] Clang not found, downloading..."
    wget -O clang-r416183b.tar.gz \
      https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/b669748458572622ed716407611633c5415da25c/clang-r416183b.tar.gz
    tar -xf clang-r416183b.tar.gz --strip-components=1
else
    echo "[✔] Clang already exists."
fi

echo "[I] Downloading build-tools..."
mkdir -vp "$my_top_dir/kernel/prebuilts/"
cd "$my_top_dir/kernel/prebuilts/"
if [ ! -d build-tools ]; then
    git clone https://android.googlesource.com/kernel/prebuilts/build-tools
else
    echo "[✔] build-tools already exists."
fi

echo "[I] Creating GKI directory..."
mkdir -vp "$my_top_dir/vendor/aosp_gki/kernel/aarch64"
cd "$my_top_dir/vendor/aosp_gki/kernel/aarch64"

echo "[I] Downloading GKI Image.gz for android13-5.10-2025-07_r1..."
wget -O Image.gz \
  https://ci.android.com/builds/submitted/13792087/kernel_aarch64/latest/Image.gz

echo "[✔] All prebuilts and GKI downloaded successfully."
cd "$my_top_dir"

