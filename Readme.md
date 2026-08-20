# MeowMeowOS

MeowMeowOS is a small x86 hobby operating system built from scratch. It boots
with QEMU, uses a freestanding i686 toolchain, and includes a FAT16 filesystem,
kernel shell, user programs, virtual memory, task scheduling, and basic device
drivers.

## Requirements

- x86_64 host computer
- At least 2 GB of available memory
- About 1 GB of free disk space
- `make`, NASM, QEMU, and an `i686-elf` cross-compiler
- `mtools` for macOS image injection

## macOS Setup

Install Homebrew if it is not already installed:

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Install the required packages:

```sh
brew install nasm qemu mtools i686-elf-binutils i686-elf-gcc
```

Confirm that the cross-compiler is available:

```sh
which i686-elf-gcc
which i686-elf-objcopy
```

If Homebrew does not provide the cross-compiler packages, build an `i686-elf`
toolchain manually or use the toolchain already configured in your `PATH`.

## Linux Setup

On Ubuntu or Debian:

```sh
sudo apt update
sudo apt install -y build-essential nasm qemu-system-x86 mtools dosfstools
sudo apt install -y gcc-i686-linux-gnu binutils-i686-linux-gnu
```

If necessary, create the tool names expected by the Makefile:

```sh
sudo ln -sf /usr/bin/i686-linux-gnu-gcc /usr/local/bin/i686-elf-gcc
sudo ln -sf /usr/bin/i686-linux-gnu-objcopy /usr/local/bin/i686-elf-objcopy
```

## First Run

From the repository root, run:

```sh
make clean
make first-run
```

The first-run workflow:

1. Builds the kernel and user programs.
2. Starts QEMU and waits for the guest FAT16 filesystem to finish mounting.
3. Closes the initialization session automatically.
4. Injects the user programs into the disk image.
5. Reopens QEMU with the completed image.

The monitor timeout defaults to 30 seconds. Increase it when needed:

```sh
make FIRST_BOOT_TIMEOUT=60 first-run
```

`make clean` deletes the build output and disk image, so use it only when a
fresh image is intended.

## Running the OS

Build everything without launching QEMU:

```sh
make all
```

Start the OS normally:

```sh
make run
```

Start with serial kernel logs:

```sh
make debug
```

Start with QEMU interrupt tracing:

```sh
make debug-full
```

The default macOS QEMU window size is 1200x800. Customize it with:

```sh
make QEMU_WINDOW_WIDTH=1440 QEMU_WINDOW_HEIGHT=900 debug
```

Window resizing on macOS uses `osascript`. If the window does not resize,
allow Terminal under **System Settings > Privacy & Security > Accessibility**.

## Shell

The shell prompt looks like this:

```text
[root@shell:/]>
```

Built-in commands:

```text
help
clear
cd <path>
```

User programs include:

```text
ls       cat      echo     mkdir
rm       rmdir    touch    format
install  uptime   testmem  testdsk
taskst
```

Commands can be run from the current directory. The shell also searches the
installed system command directory and the FAT16 root directory.

## Troubleshooting

### `i686-elf-gcc` not found

Check the compiler path:

```sh
which i686-elf-gcc
```

Install the cross-compiler packages or add the toolchain's `bin` directory to
your `PATH`.

### QEMU says the image is locked

Only one QEMU process can use the disk image at a time. Close the existing
emulator before running `make run`, `make debug`, or `make first-run` again.

### User programs are missing

Run:

```sh
make inject
```

On macOS, injection uses `mtools` rather than Linux loopback mounting:

```sh
brew install mtools
```

### The screen is black or the kernel stops booting

Run debug mode and inspect the serial output:

```sh
make debug
```

For more detailed QEMU interrupt logs:

```sh
make debug-full
```

### Keyboard input is not working

Click inside the QEMU window to focus it. Use `Ctrl+Alt+G` to release captured
keyboard and mouse input.

## Project Layout

```text
MeowMeowOS/
├── Makefile
├── build.sh
├── bin/                  Generated kernel, user programs, and disk image
├── build/                Generated object files
└── src/
    ├── bootloader/       BIOS boot sector
    ├── kernel/           Kernel, drivers, filesystem, memory, and shell
    └── usr/              User programs and user-space headers
```

## Development

Rebuild from scratch:

```sh
make clean
make all
```

Kernel logs are written to `MeowMeowOS.log` during debug runs. The disk image is
preserved by normal builds; only `make clean` removes it.