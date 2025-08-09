Linux kernel
============

There are several guides for kernel developers and users. These guides can
be rendered in a number of formats, like HTML and PDF. Please read
Documentation/admin-guide/README.rst first.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.  The formatted documentation can also be read online at:

    https://www.kernel.org/doc/html/latest/

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.

## How to build this shit

1. Make a new directory (top directory)
```shell
mkdir fuckmoto && cd fuckmoto
```
2. Clone our repository with the kernel and scripts into a directory kernel-5.10
```shell
git clone https://github.com/moto-penangf/kernel_moto_penangf kernel-5.10 --depth 1
```
3. Move the helper scripts to the top directory
```shell
mv kernel-5.10/helpers/* ./
```

*It should look something like this:*
```
|- fuckmoto
|  |-- kernel-5.10
|  |
|  |-- build.sh
|  |-- download_toolchain.sh
|  |-- download_modules.sh
```

4. Use the script `download_toolchain.sh` to download the correct toolchain and create the proper directory structure for its storage.

> **ATTETION!** Helper scripts must only be run in the top directory!

```shell
./download_toolchain.sh
```

5. Use the script `download_gki.sh` to download the GKI
```shell
./download_gki.sh
```

6. Use the script `download_modules.sh` to download the vendor kernel modules required for the build

```shell
./download_modules.sh
```
7. Check that the project structure is correct.
*A small example of what the structure should look like:*
```
|- fuckmoto
|  |-- kernel-5.10
|  |-- vendor/mediatek/kernel_modules
|  |-- prebuilts
|  |   |-- build-tools
|  |   |-- clang/host/linux-x86/clang-r416183b
|  |
|  |-- build.sh
|  |-- download_toolchain.sh
|  |-- download_modules.sh
```
8. Build this shitcode from Motorola and MTK using the `build.sh` script
```shell
./build.sh
```
