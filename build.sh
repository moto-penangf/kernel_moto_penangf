#!/bin/bash
set -e

# MMI-UHAS34.29-3 For moto penang 4G 4G+
# Kernel Modules:
# ---------------
# Download prebuilts folder from Android distribution. Move it to $my_top_dir
# For example:
# my_top_dir=$PWD
# mkdir -vp $my_top_dir/kernel
# cd $my_top_dir/kernel
# git clone https://android.googlesource.com/kernel/build
# mkdir -vp  $my_top_dir/kernel/prebuilts-master/clang/host
# cd $my_top_dir/kernel/prebuilts-master/clang/host
# git clone https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86
# mkdir -vp  $my_top_dir/kernel/prebuilts/
# cd $my_top_dir/kernel/prebuilts/
# git clone https://android.googlesource.com/kernel/prebuilts/build-tools
# mkdir -vp $my_top_dir/vendor/aosp_gki/kernel/aarch64
# Download gki from: // no you ain't
# https://source.android.com/docs/core/architecture/kernel/gki-android13-5_10-release-builds
# cd -

# ========== CONFIGURATION ==========
my_top_dir="$PWD"
KERNEL_DIR="$my_top_dir/kernel-5.10"
OUT_BASE="$my_top_dir/out/target/product/penangf/obj/KERNEL_OBJ"
kernel_out_dir="$OUT_BASE/kernel-5.10"
MODULES_STAGING_DIR="$OUT_BASE/staging"
REL_KERNEL_OUT="$OUT_BASE"
TARGET_KERNEL_CONFIG="$kernel_out_dir/.config"

export PATH="$my_top_dir/prebuilts/build-tools/linux-x86/bin:$my_top_dir/prebuilts/clang/host/linux-x86/clang-r416183b/bin:$my_top_dir/kernel/prebuilts/build-tools/linux-x86/bin:$PATH"

export CLANG_TRIPLE= CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabi- \
CROSS_COMPILE_ARM32= ARCH=arm64 SUBARCH= MAKE_GOALS=all \
HOSTCC=clang HOSTCXX=clang++ CC=clang LD=ld.lld \
NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump \
OBJSIZE=llvm-size STRIP=llvm-strip

# ========== PREBUILTS SETUP ==========
echo "[*] Cloning toolchains..."
mkdir -p $my_top_dir/prebuilts/clang/host

#git clone https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86 -b aml_tz3_314012010 $my_top_dir/prebuilts/clang/host/linux-x86
# this is commented out because we dont need the whole thing, just download some one toolchain
# (or copy clang from AOSP source in prebuilts/clang/host/linux-x86)

if [ ! -d prebuilts/build-tools ]; then
    echo "[I] Toolchain is not cloneed, check build script line 50 for more info"
    echo "    Script will fail in a moment"
    git clone https://android.googlesource.com/kernel/prebuilts/build-tools $my_top_dir/prebuilts/build-tools
else
    echo "[✔] Build tools already exist, skipping"
fi

# ========== CONFIGURE KERNEL ==========
echo "[*] Generating build config..."
mkdir -p "$kernel_out_dir"
python2 kernel-5.10/scripts/gen_build_config.py --kernel-defconfig penangf_defconfig --kernel-defconfig-overlays "" --kernel-build-config-overlays "" -m user -o $REL_KERNEL_OUT/build.config
cp -p kernel-5.10/arch/arm64/configs/penangf_defconfig "$REL_KERNEL_OUT/penangf.config"

echo "[*] Building kernel..."
cd $KERNEL_DIR

# Step 1: Start with GKI base defconfig
make LLVM=1 LLVM_IAS=1 O=$kernel_out_dir gki_defconfig
echo "[✔] Built gki_defconfig"

# Step 2: Merge penangf.config into the existing .config
$KERNEL_DIR/scripts/kconfig/merge_config.sh -O $kernel_out_dir $kernel_out_dir/.config $REL_KERNEL_OUT/penangf.config
echo "[✔] Merged defconfigs successfully"

echo "[*] Avoiding interactive prompts"
make LLVM=1 LLVM_IAS=1 O=$kernel_out_dir olddefconfig

echo "[*] Building kernel"

# Step 3: Build kernel
make -j$(nproc) O=$kernel_out_dir LLVM=1 LLVM_IAS=1 all
echo "[✔] Built kernel (step#1)"

make -j$(nproc) O=$kernel_out_dir LLVM=1 LLVM_IAS=1 INSTALL_MOD_PATH=$MODULES_STAGING_DIR modules_install
echo "[✔] Built kernel (step#2)"

echo "[*] Building modules"
# ========== MODULE BUILD FUNCTION ==========
build_module() {
    local mod_path=$1
    local install_path=$2

    echo "[*] Building $mod_path"
    mkdir -p "$install_path"
    make -C $mod_path M=$mod_path KERNEL_SRC=$KERNEL_DIR O=$kernel_out_dir LLVM=1 LLVM_IAS=1 DEPMOD=depmod DTC=dtc
    make -C $mod_path M=$mod_path KERNEL_SRC=$KERNEL_DIR O=$kernel_out_dir LLVM=1 LLVM_IAS=1 DEPMOD=depmod DTC=dtc INSTALL_MOD_PATH=$install_path modules_install
}

echo "[✔] Built modules"

echo "[*] Building standard modules now"
# ========== STANDARD MODULES ==========
export GPS_CHIP_ID=common

build_module ../vendor/mediatek/kernel_modules/met_drv_v3 "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/met_drv_v3"
build_module ../vendor/mediatek/kernel_modules/gpu/platform/mt6768 "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/gpu/platform/mt6768"
build_module ../vendor/mediatek/kernel_modules/connectivity/common "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/common"
build_module ../vendor/mediatek/kernel_modules/connectivity/fmradio "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/fmradio"

build_module ../vendor/mediatek/kernel_modules/connectivity/connfem "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/connfem"
build_module ../vendor/mediatek/kernel_modules/connectivity/conninfra "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/conninfra"

build_module ../vendor/mediatek/kernel_modules/connectivity/gps/gps_stp "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/gps_stp"
build_module ../vendor/mediatek/kernel_modules/connectivity/gps/gps_pwr "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/gps_pwr"
# build_module ../vendor/mediatek/kernel_modules/connectivity/gps/gps_scp "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/gps_scp"
# build_module ../vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v010 "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v010"
# build_module ../vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v030 "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v030"
# build_module ../vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v050 "$REL_KERNEL_OUT/vendor/mediatek/kernel_modules/connectivity/gps/data_link/plat/v050"

# ========== ETC SPECIAL MODULES ==========
cd "$kernel_out_dir"

declare -A etc_modules=(
#    [udc]="../../ETC/udc_lib.ko_intermediates/LINKED"
#    [connfem]="../../ETC/connfem.ko_intermediates/LINKED"
#    [wmt_drv]="../../ETC/wmt_drv.ko_intermediates/LINKED"
#    [bt_drv]="../../ETC/bt_drv_connac1x.ko_intermediates/LINKED"
#    [wmt_chrdev_wifi]="../../ETC/wmt_chrdev_wifi.ko_intermediates/LINKED"
#    [wlan_drv_gen4m]="../../ETC/wlan_drv_gen4m.ko_intermediates/LINKED"
)

for mod in "${!etc_modules[@]}"; do
    path="${etc_modules[$mod]}"
    echo "[*] Building ETC module $mod"
    make O=$kernel_out_dir LLVM=1 LLVM_IAS=1 DEPMOD=depmod DTC=dtc M=$path AUTOCONF_H=$kernel_out_dir/include/generated/autoconf.h \
         KBUILD_EXTRA_SYMBOLS="$kernel_out_dir/Module.symvers" \
         src="$my_top_dir/vendor/mediatek/kernel_modules/connectivity/${mod//_/\/}"
done

echo "[✔] Kernel and modules built successfully."

# ========== ASK USER ABOUT ANYKERNEL3 ==========
read -p "Do you want to create a TWRP-flashable AnyKernel3 ZIP? (y/n): " choice
case "$choice" in
  [Yy])
    echo "[*] Creating AnyKernel3 flashable zip..."
    # Paths
    ANYKERNEL_DIR="$my_top_dir/AnyKernel3"
    ANYKERNEL_OUT="$my_top_dir/AnyKernel3-penang-kernel.zip"

    # Clone AnyKernel3 if not exists
    if [ ! -d "$ANYKERNEL_DIR" ]; then
      git clone https://github.com/r0ddty/AnyKernel3 "$ANYKERNEL_DIR"
    fi

    # Clean old files
    rm -f "$ANYKERNEL_DIR/Image.gz-dtb" "$ANYKERNEL_DIR/Image.gz"
    rm -rf "$ANYKERNEL_DIR/modules"
    mkdir -p "$ANYKERNEL_DIR/modules"

    # Copy kernel Image.gz-dtb (adjust if Image.gz or zImage is used)
    if [ -f "$kernel_out_dir/arch/arm64/boot/Image.gz" ]; then
      cp "$kernel_out_dir/arch/arm64/boot/Image.gz" "$ANYKERNEL_DIR/"
    else
      echo "[!] Kernel image not found! Check build output."
      exit 1
    fi

    # Copy modules
    find "$MODULES_STAGING_DIR" -name '*.ko' -exec cp {} "$ANYKERNEL_DIR/modules/" \;

    # Optional: strip modules to reduce size
    if command -v llvm-strip &> /dev/null; then
      find "$ANYKERNEL_DIR/modules" -name '*.ko' -exec llvm-strip --strip-unneeded {} \;
    fi

    # Customize anykernel.sh for your device
    sed -i 's|^block=.*|block=/dev/block/bootdevice/by-name/boot|' "$ANYKERNEL_DIR/anykernel.sh"
    sed -i 's|^is_slot_device=.*|is_slot_device=1|' "$ANYKERNEL_DIR/anykernel.sh"

    # Create ZIP
    (cd "$ANYKERNEL_DIR" && zip -r9 "$ANYKERNEL_OUT" ./* > /dev/null)

    echo "[✔] Flashable zip created at: $ANYKERNEL_OUT"
    ;;
  [Nn])
    echo "[*] Skipping AnyKernel3 zip creation."
    ;;
  *)
    echo "Invalid choice: '$choice'. Please answer 'y' or 'n'."
    ;;
esac
