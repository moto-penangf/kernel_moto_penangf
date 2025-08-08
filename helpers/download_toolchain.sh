#!/bin/bash
my_top_dir=$PWD

mkdir -vp $my_top_dir/kernel
cd $my_top_dir/kernel
git clone https://android.googlesource.com/kernel/build

mkdir -vp  $my_top_dir/kernel/prebuilts-master/clang/host
cd $my_top_dir/kernel/prebuilts-master/clang/host
wget https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/b669748458572622ed716407611633c5415da25c/clang-r416183b.tar.gz
tar -xf clang-r416183b.tar.gz

mkdir -vp  $my_top_dir/kernel/prebuilts/
cd $my_top_dir/kernel/prebuilts/
git clone https://android.googlesource.com/kernel/prebuilts/build-tools
cd $my_top_dir
