# Linux System Monitor

A simple system monitoring tool written in C as part of my first-semester Introduction to Computer Science course at IIT Jodhpur.

The program displays CPU utilization, RAM usage, and disk space usage in the terminal and refreshes the values every second.

## Features
- CPU usage calculation using `/proc/stat`
- RAM usage using `/proc/meminfo`
- Disk space information using `statvfs()`
- Live terminal display

## How to Run

Make sure GCC is installed on your Linux system.

1.) Compile the program:

gcc main.c -o system_monitor

2.) Run it:

./system_monitor


Press `Ctrl+C` to stop the program.

## What I Learned

This project helped me understand basic C programming, structures, file handling, Linux system information, and calculating CPU usage from successive readings.

## Features

- **CPU utilization:** estimates overall CPU usage by comparing counters from two readings of `/proc/stat`.
- **RAM usage:** reads `MemTotal` and `MemAvailable` from `/proc/meminfo`.
- **Filesystem usage:** uses `statvfs()` to report capacity and allocated space for a selected path.
- **Live dashboard:** refreshes every second by default.
- **Graceful exit:** press `Ctrl+C` to stop the monitor.

## Requirements

- Linux
- GCC (or another C11-compatible compiler)
- `make` (optional, but recommended)

The program reads Linux-specific virtual files under `/proc` and uses the POSIX `statvfs()` interface, so it is not intended to compile unchanged on Windows.

## Build

Clone the repository, enter its directory, and run:

```bash
make
```

Or compile directly:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 src/main.c -o system-monitor
```

## Run

Monitor the filesystem containing `/`:

```bash
./system-monitor
```

Monitor the filesystem containing another path, such as `/home`:

```bash
./system-monitor /home
```

The program refreshes once per second. Press `Ctrl+C` to exit.

> The path selects the filesystem containing that path. For example, `/home` may be on the same filesystem as `/` or on a separate mounted filesystem.

## How it works

### CPU

Linux exposes aggregate CPU time counters in the first line of `/proc/stat`. The program takes an initial snapshot, waits for one interval, and takes another snapshot.

Let:

- \(\Delta T\) = change in the sum of the tracked CPU counters
- \(\Delta I\) = change in idle time plus I/O-wait time

Then the reported utilization is:

\[
\text{CPU usage (\%)} =
100 \times \frac{\Delta T - \Delta I}{\Delta T}
\]

The result is an interval-based estimate, not an instantaneous measurement. Linux CPU counters are measured in scheduler ticks, not seconds.

### Memory

`/proc/meminfo` reports memory values in KiB. The program uses `MemTotal` and `MemAvailable`:

\[
\text{Used RAM} \approx \text{MemTotal} - \text{MemAvailable}
\]

`MemAvailable` is an estimate of memory available for new applications without swapping, so this definition is more useful for general monitoring than simply counting memory that is not marked idle.

### Disk

`statvfs()` provides filesystem capacity information. The program reports total filesystem capacity and allocated space for the filesystem containing the chosen path. Values are converted to GiB (1024³ bytes).

## Project structure


linux-system-monitor/
├── src/
│   └── main.c
├── Makefile
├── .gitignore
└── README.md


## Limitations

- This is a lightweight learning project, not a replacement for tools such as `top`, `htop`, or `df`.
- It monitors aggregate CPU usage, not per-core usage or individual processes.
- Memory and filesystem values are rounded for display.
- Disk figures describe the filesystem containing the selected path, not necessarily every disk or partition in the machine.
- Terminal clearing uses ANSI escape sequences and may not behave as expected in every terminal.

## Possible future improvements

- Per-core CPU usage and a simple text-based usage bar.
- Network throughput and process-level monitoring.
- Configurable refresh interval.
- Automated tests for CPU percentage calculations.
- Better terminal resizing and non-interactive output mode.

## Learning outcomes

- Reading Linux system information from `/proc`
- Using C structures, functions, pointers, and file I/O
- Calculating CPU utilization from successive counter snapshots
- Inspecting filesystem statistics with `statvfs()`
- Compiling C programs with compiler warnings and a Makefile

