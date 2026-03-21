# TUI Guide

The EFL TUI (Terminal User Interface) is a display-only monitor that shows real-time information about the EFL engine's state. It provides visual feedback during boot, validation diagnostics, and live activity monitoring.

## What It Does

The TUI connects to the EFL engine via a named pipe and displays structured information in three phases. It is purely passive — it does not send commands to the engine or modify game state.

## Installation

The TUI binary (`efl-tui` or `efl-tui.exe`) is included in EFL releases. Place it somewhere on your system PATH, or in the same directory as EFL.dll.

### Building from Source

```bash
cd tui
cargo build --release
```

The binary is output to `tui/target/release/efl-tui.exe`.

## Launching

### Auto-launch (Recommended)

The EFL engine automatically spawns the TUI as a child process during initialization if `efl-tui` is found on PATH. No manual setup required — just ensure the binary is accessible.

If the TUI fails to launch, the engine continues normally. The TUI is optional.

### Manual Launch

Connect to a running EFL instance by specifying the pipe name:

```bash
efl-tui --pipe \\.\pipe\efl-12345
```

Replace `12345` with the game's process ID.

### Demo Mode

Preview all three phases with mock data, no game required:

```bash
efl-tui --demo
```

## The Three Phases

### Boot

Displays an animated startup sequence as the engine initializes. Each subsystem reports its status:

- Version check
- Aurie/YYTK connection
- Manifest discovery and parsing
- Capability resolution
- Subsystem startup

Each step shows an in-progress indicator followed by a pass/fail/warning result.

### Diagnostics

After boot, a structured validation report appears showing any issues found during content loading. Diagnostics are displayed as cards organized by category, with coded identifiers and severity levels.

See [Diagnostic Codes](diagnostic-codes.md) for the code format and categories.

### Monitor

The live dashboard shows ongoing engine activity:

- **Active hooks**: Which game functions are currently hooked
- **Recent events**: Event bus activity (room transitions, NPC interactions, etc.)
- **Save operations**: Read/write operations to the save namespace
- **Loaded mods**: Status of each loaded EFL Pack (active, error, etc.)

## Terminal Requirements

The TUI requires a terminal that supports ANSI color codes and Unicode box-drawing characters. Recommended:

- **Windows Terminal** (included with Windows 11, available from Microsoft Store for Windows 10)
- Any terminal emulator with 256-color support

The classic Windows Command Prompt (`cmd.exe`) may not render correctly.

## Exiting

Press **Ctrl+C** to exit the TUI at any time. This does not affect the game or the EFL engine — it only closes the monitor display.
