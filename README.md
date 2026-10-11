# Shogun-Sync

> A lightweight, automatic multiplayer save synchronization tool for **Total War: Shogun 2**.

[![Language](https://img.shields.io/badge/Language-C-00599C.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/Platform-Windows_10_%2F_11-blue.svg)](https://www.microsoft.com/windows)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-green.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Release](https://img.shields.io/badge/Release-v0.0.7-brightgreen.svg)]()

---

## Overview

**Shogun-Sync** is an independent, single-developer hobby project created to solve one of the most frustrating issues in *Total War: Shogun 2* multiplayer co-op and head-to-head campaigns: **save file desynchronization (desyncs)**.

During long campaigns, minor simulation divergence causes the host and client save files to drift apart. When this happens, turns fail to load or the campaign breaks entirely, forcing players to manually copy large `.save_multiplayer` files over Discord, cloud storage, or USB drives.

**Shogun-Sync fixes this automatically:**
1. It runs in a background terminal alongside your game.
2. It monitors your game save directory for changes every turn.
3. It computes SHA-1 checksums of the active save file.
4. It compares hashes with your campaign partner via a secure relay lobby server (`wss://shogun2-sync.duckdns.org`).
5. **Auto-Resync**: If a desync is detected, the host automatically compresses the authoritative save and sends it to the client over an encrypted WebSocket stream, immediately restoring the save file on the client's machine without any manual intervention.

---

## System Requirements & Save Location

* **Operating System:** Windows 10 / Windows 11 (64-bit)
* **Game:** *Total War: Shogun 2* (Steam edition)
* **Save Directory:**  
  The tool automatically resolves your game directory via `%APPDATA%`:
  ```text
  %APPDATA%\The Creative Assembly\Shogun2\save_games_multiplayer
  ```
  *(You do not need to configure any paths manually — the program detects them automatically).*

---

## Installation

### Option 1: Quick Install (Recommended for Players)

1. Go to the [Releases](https://github.com/KolajW/shogun2-sync/releases) page and download the latest release archive (`shogun-sync.zip`).
2. Unzip the downloaded folder. You will find:
   * `shogun-sync.exe`
   * `install.bat`
3. Double-click **`install.bat`** to run the automatic installer:
   * It creates a dedicated directory: `%LOCALAPPDATA%\ShogunSync`
   * It copies `shogun-sync.exe` there.
   * It automatically adds `%LOCALAPPDATA%\ShogunSync` to your user `PATH` environment variable.
4. **Close and reopen** your terminal (Command Prompt or PowerShell).
5. Verify the installation by typing:
   ```cmd
   shogun-sync --version
   ```

---

## Quick Start Guide

### Step 0: Initialize (One-time setup)
Before your first session, initialize the local repository structure:
```cmd
shogun-sync init
```
*This creates the hidden `.sync` directory structure used to version and store compressed save snapshots.*

---

### Step 1: Host Creates a Room
The host player starts the lobby session with a chosen password:
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
*Give the 6-character **Room Code** and **Password** to your multiplayer partner.*

---

### Step 2: Client Joins the Room
The second player connects using the provided room code and password:
```cmd
shogun-sync join K7P9X2 MySecretPass123
```
Output:
```text
Connected with lobby
Waiting for turn changes... (Press CTRL + C to exit)
```

---

### Step 3: Play Your Campaign
1. Launch *Total War: Shogun 2* and play your multiplayer campaign normally.
2. At the end of every turn, Shogun-Sync detects the new autosave and verifies hashes over the network:
   * **In sync:** `Saves are sync` — Campaign continues smoothly.
   * **Desync detected:** `Saves are desync` — The client automatically receives the host's save, decompresses it via `zlib`, and overwrites the local autosave.
3. To stop the synchronization tool at any time, press `CTRL + C`.

---

## Command Line Reference

```text
Usage: shogun-sync [command] [options]

Commands:
  init                       Initialize local .sync repository directories
  create <password>          Create a multiplayer lobby room with an auto-generated 6-character code
  join <room_code> <pass>    Join an existing multiplayer room using room code and password
  commit                     Manually create a local snapshot commit of current saves
  --watch, -w                Watch save directory for changes (standalone offline mode)
  --scan, -s                 Scan and count save files in the multiplayer directory
  --tscan, -ts               Scan and list names of all detected multiplayer saves
  --test [name], -t          Test SHA-1 hashing and zlib compression on a save file
  --version, -v              Display version and author information
  --help, -h                 Display command help and usage instructions
```

---

## Diagnostics & Error Codes

If any operation fails, Shogun-Sync outputs a standardized error code:

| Code | Identifier | Explanation |
| :---: | :--- | :--- |
| **001** | `SAVE_NOT_FOUND` | Multiplayer save file does not exist in the folder yet. |
| **002** | `BLOB_WRITE_FAILED` | Failed to write compressed object to disk. |
| **003** | `DEFLATE_STREAM_FAILED` | Zlib compression stream encountered an error. |
| **004** | `DEFLATE_INIT_FAILED` | Zlib deflate stream initialization failed. |
| **005** | `APPDATA_NOT_FOUND` | Windows `%APPDATA%` environment variable is missing. |
| **006** | `LOCALAPPDATA_NOT_FOUND` | Windows `%LOCALAPPDATA%` environment variable is missing. |
| **007** | `DIR_CREATE_FAILED` | Could not create hidden `.sync` directories. |
| **008** | `DIR_OPEN_FAILED` | Could not open the save games folder. |
| **009** | `CTRL_HANDLER_FAILED` | Win32 console control handler failed to attach. |
| **010** | `INFLATE_INIT_FAILED` | Zlib inflate stream initialization failed. |
| **011** | `INFLATE_STREAM_FAILED` | Zlib decompression stream error. |
| **012** | `INFLATE_WRITE_FAILED` | Failed to write restored save file to game directory. |
| **013** | `DECOMPRESS_OPEN_FAILED`| Could not open compressed blob for inflation. |
| **014** | `HOSTNAME_FAILED` | Network socket hostname lookup failed. |
| **015** | `CONN_FAILED` | WebSocket connection to relay server failed. |
| **016** | `INVALID_PAYLOAD` | Malformed network packet received. |
| **017** | `COMMIT_FAILED` | Commit creation failed. |
| **018** | `MANIFEST_WRITE_FAILED` | Failed to write tree manifest file. |
| **019** | `RESYNC_DECOMPRESS_FAILED` | Failed to decompress save file during Auto-Resync. |
| **020** | `INVALID_ARGUMENTS` | Missing or invalid command line arguments. |

*Detailed diagnostics and recovery manual are available in [`docs/errors.tex`](docs/errors.tex).*

---

## Architecture & Built With

* **Language:** Pure C (C99), native Win32 API (`FindFirstChangeNotificationA`, `FindFirstFileA`, Winsock2).
* **Storage Engine:** Git-inspired Content-Addressable Storage (CAS) storing immutable objects (Blobs, Trees, Commits, Refs).
* **Cryptographic Hash:** Custom standalone implementation of **SHA-1** (FIPS PUB 180-1) for fast content hashing.
* **Compression:** [zlib](https://zlib.net/) (v1.3.2) by Jean-loup Gailly and Mark Adler for in-memory deflation and inflation.
* **Networking:** [Mongoose](https://cesanta.com/) by Cesanta Software — embedded event-driven networking engine providing secure WebSocket connections with TLS.
* **Relay Infrastructure:** Lightweight FastAPI WebSocket relay server deployed on Hetzner with Let's Encrypt TLS termination.

---

## Building from Source

To compile the binary yourself using GCC (MinGW-w64 on Windows):

```cmd
gcc -DMG_TLS=MG_TLS_BUILTIN *.c mongoose\mongoose.c zlib\*.c -o shogun-sync.exe -lws2_32
```

---

## License

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)**.

* **Mongoose Library** is dual-licensed by Cesanta Software under GPLv2 (for open-source/non-commercial use) and commercial terms.
* **zlib** is licensed under the permissive zlib license (Copyright © 1995–2026 Jean-loup Gailly and Mark Adler).

In compliance with the GNU GPLv2 license, all source code for this project is publicly available. See the [LICENSE](LICENSE) file for complete details.

---

## Disclaimer

*Total War: Shogun 2* is a registered trademark of The Creative Assembly and SEGA. **Shogun-Sync** is an unofficial, open-source utility created by a fan and is not affiliated with, endorsed by, or supported by The Creative Assembly or SEGA.
