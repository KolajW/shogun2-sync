<div align="center">

<img src="docs/shogun-sync.png" width="160" alt="Shogun-Sync Logo" />

# Shogun-Sync

> An unofficial multiplayer save synchronization and desynchronization recovery tool for **Total War: Shogun 2**.  
> **Website & Downloads:** [https://shogun2-sync.com](https://shogun2-sync.com)

[![Language](https://img.shields.io/badge/Language-C99%20%2F%20C%2B%2B17-00599C.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/Platform-Windows_10_%2F_11-blue.svg)](https://www.microsoft.com/windows)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-green.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Release](https://img.shields.io/badge/Release-v0.1.1--beta-brightgreen.svg)]()
[![Website](https://img.shields.io/badge/Website-shogun2--sync.com-orange.svg)](https://shogun2-sync.com)

</div>

> [!NOTE]
> **Beta Notice**: Shogun-Sync is currently in beta. If you encounter any bugs, unexpected behavior, or desync issues during your campaigns, please report them on [GitHub Issues](https://github.com/KolajW/shogun2-sync/issues).  
> For installer downloads, quickstart guide, and documentation, visit [**shogun2-sync.com**](https://shogun2-sync.com).

---

## Overview

**Shogun-Sync** is an independent, community-developed tool created to resolve multiplayer campaign desynchronization (desync) issues in *Total War: Shogun 2*.

During long co-op or head-to-head campaigns, simulation divergence can cause host and client save files to drift out of sync. When this occurs, turns fail to load or the match breaks entirely, traditionally forcing players to exit the session, find files on disk, and manually copy `.save_multiplayer` archives.

**Shogun-Sync automates the entire recovery process:**
1. **Real-time Filesystem Watcher**: Monitors the multiplayer campaign save directory via native Win32 kernel notifications (`FindFirstChangeNotificationA`).
2. **Cryptographic Consistency Check**: Generates RFC-compliant SHA-1 digests of the active campaign save file at the end of each turn.
3. **Encrypted Relay Communication**: Compares turn checksums with peer players through an external, standalone WebSocket relay broker.
4. **Auto-Resync**: When a desynchronization is detected, the host automatically streams the authoritative, compressed save file to the desynced client over the WebSocket channel.
5. **In-Game Save Reload Injection**: The client restores the save file on disk and dispatches an input signal to the game process, prompting the engine to execute an immediate quickload without restarting the match.
6. **DirectX HUD Overlay**: An in-game overlay displays live campaign synchronization metrics, room code, and turn count directly on screen.

---

## Project Components & Binaries

The application is structured into four native Windows binaries:

| Binary | Description |
| :--- | :--- |
| **`ShogunSyncSetup.exe`** | Dedicated Win32 graphical installation wizard with embedded binary resources (`RCDATA`). |
| **`ShogunSync.exe`** | Windowless graphical launcher executing the GUI without flashing a console terminal. |
| **`shogun-sync.exe`** | Core C99 CLI daemon, filesystem watcher, CAS storage engine, and embedded Dear ImGui GUI. |
| **`shogun_overlay32.dll`** | 32-bit (x86) in-game HUD overlay detouring Direct3D 9 and Direct3D 11 presentation calls. |

---

## Graphical User Interface (GUI)

The application provides a hardware-accelerated desktop interface (DirectX 11 + Dear ImGui) offering live synchronization telemetry, session management, and one-click in-game overlay injection:

![Shogun-Sync Desktop Interface](docs/gui_preview.png)

---

## In-Game Overlay & Hotkeys

When the overlay DLL (`shogun_overlay32.dll`) is injected into the running game (`Shogun2.exe`), it hooks Direct3D presentation routines and intercepts window messages via a dedicated `WndProc` detour:

* **`F10`**: Toggles the interactive Shogun-Sync HUD menu on/off. When opened, it enables mouse cursor capture to allow clicking in-menu controls.
* **`F9`**: Triggers a manual quick save reload in game.
* **HUD Telemetry Banner**: Displays current campaign turn, room code, role (Host/Client), and real-time synchronization state:
  * `[ SYNC OK ]` – Both campaign saves match.
  * `[ DESYNC DETECTED ]` – Hash divergence detected between players.
  * `[ RESYNCING ]` – Save stream is being received and restored.
  * `[ SAVE INJECTED ]` – Save has been written to disk and reload dispatched (displays for 6 seconds).

---

## Installation

### Option 1: Dedicated Installer (Recommended)

1. Download `ShogunSyncSetup.exe` from [shogun2-sync.com/#downloads](https://shogun2-sync.com/#downloads) or the GitHub Releases page.
2. Run the installer:
   * Extracts `shogun-sync.exe`, `ShogunSync.exe`, and `shogun_overlay32.dll` into `%LOCALAPPDATA%\ShogunSync`.
   * Automatically adds `%LOCALAPPDATA%\ShogunSync` to your user `PATH` environment variable.
   * Generates desktop and Start Menu shortcuts pointing to `ShogunSync.exe`.
   * Registers uninstallation keys in the Windows registry.

### Option 2: Script-Based Install (`install.bat`)

1. Extract the release `.zip` archive containing `shogun-sync.exe`, `ShogunSync.exe`, `shogun_overlay32.dll`, and `install.bat`.
2. Double-click `install.bat` to copy the files to `%LOCALAPPDATA%\ShogunSync` and register the `PATH` variable.

---

## Quick Start Guide

### Mode A: Graphical User Interface (GUI)

1. Launch **`ShogunSync.exe`** (or use the desktop shortcut).
2. **Multiplayer Lobby Tab**:
   * Select **Host (Create Room)** or **Client (Join Room)**.
   * Enter your chosen password (and Room Code if joining as Client).
   * Click **Start Session**.
3. **Tools Tab**:
   * Click **Inject In-Game Overlay** once *Total War: Shogun 2* is running.
   * If manual reload is ever needed, click **Trigger In-Game Save Reload (F9)**.

---

### Mode B: Command-Line Interface (CLI)

#### 1. Initial Setup (One-time)
```cmd
shogun-sync init
```
*Creates the local `.sync` repository structure in `%APPDATA%\The Creative Assembly\Shogun2\`.*

#### 2. Host Creates a Room
```cmd
shogun-sync create MySecretPass123
```
Output:
```text
Room code: K7P9X2
Password: MySecretPass123
Connected with lobby
Waiting for turn changes... (Press CTRL + C to exit)
```
*Share the 6-character room code and password with your partner.*

#### 3. Client Joins the Room
```cmd
shogun-sync join K7P9X2 MySecretPass123
```

#### 4. Inject Overlay (Optional but Recommended)
In a separate terminal (or via the GUI):
```cmd
shogun-sync inject
```

---

## Command Line Reference

```text
Usage: shogun-sync [command] [options]

Commands:
  --gui, -g                  Launch graphical user interface
  init                       Initialize synchronization directories and local CAS repository
  create <password>          Create a multiplayer room with an auto-generated 6-character code
  join <room_code> <pass>    Join an existing multiplayer room
  commit                     Manually create a local save snapshot commit
  --watch, -w                Watch save directory for changes (offline standalone mode)
  --scan, -s                 Scan and count save files in multiplayer directory
  --tscan, -ts               Scan and list names of all detected multiplayer saves
  inject [dll_path]          Inject DirectX HUD overlay into running Shogun2.exe process
  reload [save_name]         Trigger in-game save reload via overlay IPC
  update, -u                 Check for and install software updates
  uninstall                  Cleanly uninstall Shogun-Sync from system
  --version, -v              Display version information
  --help, -h                 Display help and usage instructions
```

---

## Diagnostics & Error Codes Matrix

When an issue occurs, Shogun-Sync outputs a standardized 3-digit error code:

| Code | Identifier | Description & Resolution |
| :---: | :--- | :--- |
| **001** | `SAVE_NOT_FOUND` | Save file does not exist in directory. Play one turn to create an autosave. |
| **002** | `FILE_WRITE_FAILED` | Failed to write file stream to disk. Verify folder permissions and disk space. |
| **003** | `COMPRESSION_FAILED` | Zlib compression stream error during deflate operation. |
| **004** | `COMPRESSION_INIT_FAILED` | Zlib `deflateInit` initialization failure. |
| **005** | `APPDATA_NOT_FOUND` | `%APPDATA%` environment variable missing or variable initialization failed. |
| **006** | `LOCALAPPDATA_NOT_FOUND` | `%LOCALAPPDATA%` environment variable missing in operating system. |
| **007** | `INIT_FAILED` | Failed to create hidden `.sync` metadata storage directory. |
| **008** | `SCAN_FAILED` | Failed to open or read the save games folder via Win32 API. |
| **009** | `CTRL_HANDLER_FAILED` | Win32 console control handler failed to attach via `SetConsoleCtrlHandler`. |
| **010** | `DECOMPRESS_INIT_FAILED` | Zlib `inflateInit` initialization failure. |
| **011** | `DECOMPRESS_STREAM_FAILED`| Zlib decompression stream error or incomplete stream. |
| **012** | `DECOMPRESS_WRITE_FAILED` | Failed to write restored uncompressed save data to game directory. |
| **013** | `DECOMPRESS_OPEN_FAILED` | Could not open compressed blob file for inflation. |
| **014** | `HOSTNAME_FAILED` | Windows Sockets failed to resolve local machine hostname. |
| **015** | `CONN_FAILED` | WebSocket connection to relay broker failed or timed out. Check internet connection. |
| **016** | `INVALID_PACKET` | Malformed packet received or path traversal sequence detected. |
| **017** | `MEMORY_ALLOC_FAILED` | Heap memory allocation (`malloc`) failed due to memory exhaustion. |
| **018** | `COMMIT_FAILED` | Failed to write commit manifest or update reference pointer (`refs/latest`). |
| **019** | `RESYNC_RESTORE_FAILED` | Failed to decompress save file during Auto-Resync. |
| **020** | `INVALID_ARGUMENTS` | Missing or invalid command line parameters (e.g. room code or password). |
| **021** | `SHARED_MEMORY_FAILED` | Failed to create named shared memory mapping (`Local\ShogunSyncIPC`). |
| **022** | `MEMORY_VIEW_FAILED` | Failed to map view of IPC shared memory buffer (`MapViewOfFile`). |
| **023** | `PROCESS_NOT_FOUND` | Target process `Shogun2.exe` is not currently running. |
| **024** | `PROCESS_OPEN_FAILED` | Failed to obtain process handle with `PROCESS_ALL_ACCESS` rights. |
| **025** | `REMOTE_ALLOC_FAILED` | Virtual memory allocation in target process failed (`VirtualAllocEx`). |
| **026** | `REMOTE_WRITE_FAILED` | Writing DLL path into target process memory failed (`WriteProcessMemory`). |
| **027** | `REMOTE_THREAD_FAILED` | Spawning remote thread in game process failed (`CreateRemoteThread`). |
| **028** | `OVERLAY_NOT_FOUND` | Target overlay DLL (`shogun_overlay32.dll`) not found on disk. |
| **029** | `REMOTE_LOAD_FAILED` | Injected overlay DLL failed to load in remote process space. |
| **030** | `UPDATE_CHECK_FAILED` | Failed to query remote endpoint for latest software version. |
| **031** | `UPDATE_DOWNLOAD_FAILED` | WinINet failed to download update payload from remote URL. |
| **032** | `UNINSTALL_FAILED` | Uninstallation routine failed to clean user environment variables or directories. |
| **033** | `SELF_REPLACE_FAILED` | Atomic file replacement failed during in-place upgrade. |
| **034** | `INSTALLATION_FAILED` | Setup wizard failed to extract embedded resources or register shortcuts. |

*Detailed diagnostics and troubleshooting matrices are documented in [`docs/errors.pdf`](docs/errors.pdf).*

---

## Technical Stack & Libraries

* **Core Language**: C99 & C++17 (compiled for 32-bit x86 Windows).
* **GUI Engine**: [Dear ImGui](https://github.com/ocornut/imgui) (v1.91.x) with Direct3D 11 and Direct3D 9 backends.
* **API Hooking**: [MinHook](https://github.com/TsudaKageyu/minhook) (v1.3.3) for Direct3D Virtual Method Table (VMT) detours.
* **Storage Engine**: Content-Addressable Storage (CAS) storing immutable Blobs, Trees, Commits, and Refs.
* **Cryptographic Hash**: Standalone implementation of **SHA-1** (FIPS PUB 180-1).
* **Compression**: [zlib](https://zlib.net/) (v1.3.2) streaming deflation and inflation.
* **Networking**: [Cesanta Mongoose](https://cesanta.com/) (v7.14) event-driven asynchronous WebSocket client with embedded TLS.
* **Platform Integration**: Native Win32 APIs (`FindFirstChangeNotificationA`, `OpenFileMappingA`, `VirtualAllocEx`, `CreateRemoteThread`, `PostMessageA`, `SetWindowLongPtrA`).

---

## License

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)**.
* **Mongoose Library** is dual-licensed by Cesanta Software under GPLv2 and commercial terms.
* **zlib** is licensed under the zlib License (Copyright © 1995–2026 Jean-loup Gailly and Mark Adler).
* **Dear ImGui** is licensed under the MIT License (Copyright © 2014–2026 Omar Cornut).
* **MinHook** is licensed under the BSD 2-Clause License (Copyright © 2009–2017 Tsuda Kageyu).

See the [LICENSE](LICENSE) file for full text.

---

## Disclaimer & Legal Notice

*Total War: Shogun 2* is a registered trademark of The Creative Assembly and SEGA. **Shogun-Sync** is an unofficial, community-developed open-source modification and synchronization utility. It is **not** created by, affiliated with, endorsed by, sponsored by, or connected in any way to The Creative Assembly, SEGA, or any of their parent companies, subsidiaries, or affiliates. All game titles, assets, and trademarks belong to their respective copyright holders.
