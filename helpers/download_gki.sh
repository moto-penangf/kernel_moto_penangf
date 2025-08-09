#!/bin/bash
my_top_dir=$PWD

mkdir -vp $my_top_dir/kernel
cd $my_top_dir/kernel
git clone https://android.googlesource.com/kernel/build

mkdir -vp  $my_top_dir/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b
cd $my_top_dir/kernel/prebuilts-master/clang/host/linux-x86/clang-r416183b

if [ ! -d "${my_top_dir}/prebuilts/clang" ]; then
    echo "[I] Clang is not downloaded, downloading..."
    wget https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/b669748458572622ed716407611633c5415da25c/clang-r416183b.tar.gz
    tar -xf $my_top_dir/prebuilts-master/clang/host/linux-x86/clang-r416183b/clang-r416183b.tar.gz
else
    echo "[✔] Clang already exist, copying..."
    cp -r $my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/* .
fi

mkdir -vp  $my_top_dir/kernel/prebuilts/
cd $my_top_dir/kernel/prebuilts/
git clone https://android.googlesource.com/kernel/prebuilts/build-tools
cd $my_top_dir
