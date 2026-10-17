# Security & Privacy

A quick note on how Shogun-Sync handles your files and network:

## Supported Versions

| Version | Supported |
| :--- | :---: |
| **0.1.x** | :white_check_mark: |
| **< 0.1.0** | :x: |

---

## How It Works

* **No tracking or telemetry**: We don't collect analytics, personal data, or usage stats. All source code is here in the repo so you can inspect it yourself.
* **Saves**: The tool only touches the Shogun 2 multiplayer saves folder (`save_games_multiplayer`). It doesn't read or modify your singleplayer saves, Steam files, or personal documents.
* **Network**: Save files and hash checks are sent only between you and the other player in your room over an encrypted connection.
* **Overlay**: The in-game overlay just draws the sync status on screen and listens for `F10` (menu) and `F9` (quickload). It doesn't change game files or how the game runs.
* **Signed files**: Official releases are signed with an Authenticode certificate to ensure the files haven't been tampered with.

---

## Reporting Issues

If you find a security bug or any unexpected behavior, please report it privately via [GitHub Security Advisories](https://github.com/KolajW/shogun2-sync/security/advisories/new). We'll look into it and fix it.
