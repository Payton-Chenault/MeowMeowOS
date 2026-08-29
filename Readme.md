# MeowMeowOS

MeowMeowOS is a 32-bit x86 hobby operating system built from scratch. It boots via a custom BIOS bootloader, runs in QEMU with a VBE linear framebuffer, utilizes a freestanding `i686-elf` toolchain, and features a FAT16 filesystem, preemptive priority-based multitasking, virtual memory management, user-space libc, interactive kernel shell, and a suite of Unix-style user utilities.

---

## System Requirements

* **RAM:** At least 2 GB of available host memory.
* **Disk Space:** Approximately 1 GB of free disk space.
* **Toolchains & Emulators:** GNU Make, NASM assembler, QEMU (`qemu-system-i386` or `qemu-system-x86_64`), and an `i686-elf` cross-compiler.
* **Filesystem Tools:** `mtools` and `dosfstools` for injecting user-space binaries into the FAT16 disk image.

---

## Environment Setup

### 1. Windows (WSL2) Setup

Windows users can run MeowMeowOS seamlessly using Windows Subsystem for Linux 2 (WSL2) with Ubuntu.

1. Open PowerShell as Administrator and run:
   ```powershell
   wsl --install
   ```

2. Launch WSL: 
   ```powershell 
   wsl
2. Restart your computer if prompted, launch Ubuntu, and complete the initial account setup.
3. Update package repositories and install the build toolchain, GIT QEMU, and python-pil:

   ```terminal
   sudo apt update && sudo apt install git -y
   sudo apt upgrade -y
   ```
   ```terminal
   sudo apt install -y build-essential nasm qemu-system-x86 mtools dosfstools python3-pil
   sudo apt install -y gcc-i686-linux-gnu binutils-i686-linux-gnu
   ```
4. GUI Support: Windows 11 and recent Windows 10 builds include WSLg, allowing QEMU's graphical window to display directly on your Windows desktop without third-party X servers.

### 2. Linux Setup (Ubuntu / Debian)

On native Linux systems, install dependencies using apt:

   ```terminal
   sudo apt update && sudo apt install git -y
   sudo apt upgrade -y
   sudo apt install -y build-essential nasm qemu-system-x86 mtools dosfstools python3-pil
   sudo apt install -y gcc-i686-linux-gnu binutils-i686-linux-gnu
   ```

Set up toolchain symlinks if you are using the distro-provided cross-compilers:

   ```terminal
sudo ln -sf /usr/bin/i686-linux-gnu-gcc /usr/local/bin/i686-elf-gcc
sudo ln -sf /usr/bin/i686-linux-gnu-objcopy /usr/local/bin/i686-elf-objcopy
```

### macOS Setup

On macOS, install dependencies using Homebrew:

   ```terminal
brew install nasm qemu mtools i686-elf-binutils i686-elf-gcc python-pillow
```

Verify that the cross-compiler is available in your $PATH:

   ```terminal
which i686-elf-gcc
which i686-elf-objcopy
```


## Building and Initializing (First Run)

The OS requires an initial FAT16 disk formatting and binary injection run. Execute:


   ```terminal 
make clean
make first-run
```

or 

   ```terminal
make clean && make first run
```

#### The First-Run Workflow:
1. Compiles the bootloader, kernel, standard libraries, and all user programs.  

2. Boots QEMU headlessly to initialize and format the virtual FAT16 disk layout.  

3. Injects user-space executables (.elf) and assets onto the virtual disk using mtools.  

4. Launches the fully populated operating system in QEMU.  

Note: If disk initialization requires more time on slower machines, adjust the boot timeout:  

```terminal
make FIRST_BOOT_TIMEOUT=60 first-run
```

## Running the OS

#### Normal Run

Compile changes and boot into QEMU:
   ```terminal
make run
```

#### Build Only

Compile without launching the emulator:
   ```terminal
make all
```

#### Debug Mode

Launch with serial kernel logging to MeowMeowOS.log:
   ```terminal
make debug
```

#### Full CPU/Interrupt Tracing

Launch with QEMU interrupt and hardware trace logs:
   ```terminal
make debug-full
```

#### Custom Window Dimensions (macOS)

Launch QEMU and run the script within the repo to enlarge the window automatically to a more viewable size:

   ```terminal
make QEMU_WINDOW_WIDTH=1440 QEMU_WINDOW_HEIGHT=900 (run mode here [run, debug, etc.])
```

## Interactive Shell Conveniences
The interactive shell prompt displays the active directory:

   ```terminal
[root@shell: /]>
```

* **Command History Ring Buffer:** The shell maintains an in-memory history of the last 50 executed commands. Use the Up and Down arrow keys (or Ctrl+P / Ctrl+N) to cycle through previous commands.

* **Tab Autocompletion:** Pressing Tab (or Ctrl+T) parses partial input strings and scans both ```/system/bin/usr/commands/``` and the active working directory to complete executable names and file paths.

* **Advanced Redirection & Chaining:**
   * **Piping ( | ):** Connects the standard output of one process directly to the standard input of another (e.g., ```ps | grep shell``` or ```cat testing.txt | grep root```).

   * **Input Redirection ( < ):** Reads standard input from a file (e.g., ```cat < file.txt```)

   * **Overwrite Redirection ( > ):** Truncates or creates a file and writes command output to it (e.g., ```ls > listing.txt```).

   * **Append Redirection ( >> ):** Appends command output to the end of a file (e.g., ```echo extra line >> listing.txt```).

## Command Reference

### Built-in Commands
* ```help``` - Lists installed commands, built-in commands, and program descriptions.

* ```clear``` - Clears the graphical console buffer.

* ```cd <path>``` - Changes the current working directory.

### User-Space Programs (```/system/bin/usr/commands``` once ```install``` is ran)

| Command | Category | Description |
| :--- | :--- | :--- |
| `cat [file ...]` | File Management | Concatenates and streams file contents or standard input directly to stdout[cite: 1, 3]. |
| `echo [text ...]` | System Operations | Prints arguments separated by spaces followed by a newline. |
| `ls [path]` | File Management | Lists directory contents with file sizes and directory markers. |
| `mkdir <dir>` | File Management | Creates a new directory. |
| `rm <file>` | File Management | Unlinks and removes a file from the disk. |
| `rmdir <dir>` | File Management | Removes an empty directory. |
| `touch <file>` | File Management | Creates a new empty file. |
| `pwd` | File Management | Prints the active working directory. |
| `head [-n lines] [file]` | File Management | Outputs the first *N* lines of a file or stream (default: 10). |
| `tail [-n lines] [file]` | File Management | Outputs the last *N* lines of a file or stream using a ring buffer (default: 10). |
| `grep <pattern> [file]` | File Management | Filters and prints lines matching a pattern from a file or standard input. |
| `stat [file]` | System Operations | Displays file metadata, size, type, UID, GID, and octal permissions. |
| `ps` | System Operations | Reports a snapshot of active processes, priorities, states, and CPU ticks. |
| `kill [-s sig \| -sig] <pid>` | System Operations | Sends POSIX signals (e.g., `SIGINT`, `SIGTERM`, `SIGKILL`) to processes. |
| `free` | System Operations | Displays total, used, and free physical memory in kilobytes. |
| `uptime` | System Operations | Prints total system timer ticks elapsed since bootstrap. |
| `date` | System Operations | Displays current system date and time from the CMOS Real-Time Clock. |
| `dmesg` | System Operations | Dumps the kernel syslog ring buffer output. |
| `lspci` | System Operations | Lists detected PCI devices, class types, IRQs, and base address registers. |
| `install` | System Operations | Installs command binaries and system assets into `/system/`. |
| `format` | System Operations | Formats the drive with a fresh FAT16 filesystem. |
| `reboot` | System Operations | Reboots the system via ACPI, PS/2 controller, or triple fault reset. |
| `poweroff` | System Operations | Powers off the system via ACPI S5 sleep state. |
| `benchio` | Diagnostics | Benchmarks buffered I/O, streaming throughput, and raw 4KB block reads. |
| `testmem` | Diagnostics | Dynamic heap stress test verifying `sbrk`, splitting, and coalescing. |
| `testdsk` | Diagnostics | Validates disk I/O integrity using pattern write/read verification. |
| `taskst` | Diagnostics | Multitasking scheduler and memory subsystem stress test. |

## Troubleshooting

### ```i686-elf-gcc: command not found```
Ensure your cross-compiler is installed and included in your `$PATH`. On Debian/Ubuntu/WSL2, verify that the `/usr/local/bin/i686-elf-gcc symlinks` were created properly

### QEMU Error: "Disk Image is Locked"
Only one QEMU instance can hold a write lock on the disk image at a time. Close existing emulator instances before running `make run`, `make debug`, `make first-run`, etc.

### Executable Not Found or Missing Commands
Run `make inject` from your host terminal to re-inject compiled ELF binaries into the FAT16 disk image without re-formatting

### Keyboard / Mouse Capture in QEMU
Click inside the QEMU window to route keyboard input to MeowMeowOS. Press `Ctrl+Alt+G` (`Ctrl+Option+G` for macOS users or `Ctrl+Alt` depending on the QEMU build) to release the cursor back to the host system