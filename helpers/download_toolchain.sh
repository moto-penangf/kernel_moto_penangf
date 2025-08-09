#!/bin/bash

echo "[*] Cloning toolchains..."

mkdir -vp "$my_top_dir/prebuilts/clang/host/clang-r416183b"
wget https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/b669748458572622ed716407611633c5415da25c/clang-r416183b.tar.gz \
     -O "$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/clang-r416183b.tar.gz"
tar -xf "$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/clang-r416183b.tar.gz" \
    -C "$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b"

if [ ! -d prebuilts/build-tools ]; then
    echo "[I] Toolchain is not clonned, clonning..."
    git clone https://android.googlesource.com/kernel/prebuilts/build-tools $my_top_dir/prebuilts/build-tools
else
    echo "[✔] Build tools already exist, skipping"
fi

