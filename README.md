# GEMU

一种科研或教学用途的多平台的 RISCV 全系统指令级模拟器。基于南京大学计算机系
2023PA 作业 nemu 分支，延展出了这个项目。额外实现了 RISCV32 IMA 拓展、Zifence 拓展、Zicsr 拓展，支
持 M、MU、MSU 模式，也实现了对 Clint、Uart、VGA 外部设备模拟，适配支持清华大学的 rCore 项目，主要
是适配 Berkeley Boot Loader (BBL) ，以及对 rCore 项目进行 RV32IMA 的适配支持，成功启动 rCore 教程的
例程。这个项目可在 x64、Arm64 上运行。

此外还有一个衍生项目，gemu_linux一个针对 gemu 的 RISCV32 NOMMU Linux 移植项目（含移植教程）。对主
线 Linux6.9 内核进行裁剪、适配，构造适应 gemu 的设备树，成功启动 Linux6.9，且在模拟 VGA 正常显示，并
正确运行自编译的 Busybox
