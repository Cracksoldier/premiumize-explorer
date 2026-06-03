# premiumize-explorer

A desktop file browser for [Premiumize.me](https://www.premiumize.me) cloud storage, built with C++20 and Qt6.

![Two-pane layout: local filesystem on the left, Premiumize.me cloud on the right](.github/screenshot.png)

## Features

- **Two-pane layout** inspired by Total Commander — local filesystem on the left, Premiumize.me cloud on the right
- **Drag & drop** to upload (local → cloud) or download (cloud → local); also via right-click context menu
- **Auto-refresh** — cloud pane updates automatically when an upload into the displayed folder completes
- **Non-blocking transfer progress window** showing speed, ETA, elapsed time, and bytes transferred per file
- **Folder navigation** — breadcrumb path label; **↑ Up** always at the top of each list, hidden only at root
- **Cloud operations**: create folder, delete file/folder, context menu
- **Cloud transfers monitor** — live view of server-side Premiumize transfers (torrents, URL downloads) with status, progress, speed and ETA; open via **View → Cloud Transfers**, auto-refreshes every 5 s
- **API log window** — timestamped log of every request and response; open via **View → API Log**, saveable to file
- **API key stored** in `~/.config/premiumize-explorer/premiumize-explorer.ini` — entered once on first launch
- **Light / dark theme** — toggle via **View → Dark Mode**, preference persisted across restarts

## Requirements

| Dependency | Version |
|---|---|
| CMake | 3.21+ |
| Qt | 6.5+ (Core, Widgets, Network, Svg) |
| C++ compiler | GCC or Clang with C++20 support |
| [Qlementine](https://github.com/oclero/qlementine) | fetched automatically by CMake |

## Build

```bash
# First-time configure (downloads Qlementine, ~30s on a fresh clone)
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/debug --parallel

# Run
./build/debug/src/premiumize-explorer
```

Release build:

```bash
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
```

Optional: enable AddressSanitizer with `-DPMEX_ENABLE_ASAN=ON`.

## First launch

On first run you will be prompted for your Premiumize.me API key. You can find it at **[premiumize.me/account](https://www.premiumize.me/account)** under the **API** section. The key is saved to disk immediately so you won't be asked again.

To change the key later: **File → Change API Key…**

## Usage

| Action | How |
|---|---|
| Navigate into a folder | Double-click |
| Go up | Click **↑ Up** at the top of the list |
| Upload files | Drag files from the local pane onto the cloud pane, or right-click a local file → **Upload to Cloud** |
| Download a file | Drag a file from the cloud pane onto the local pane, or right-click → **Download** |
| Create a cloud folder | Click **+ Folder** in the cloud pane toolbar or right-click → **New Folder…** |
| Delete a cloud item | Select it and click **Delete**, or right-click → **Delete** |
| View transfer progress | **View → Transfers** |
| View cloud transfers (server-side) | **View → Cloud Transfers** — shows torrents/URL downloads Premiumize is processing for you |
| Refresh the cloud view | Click **⟳** in the cloud pane toolbar (auto-refreshes after completed uploads) |
| View API log | **View → API Log** — shows all requests/responses with timing; use **Save to File…** to export |
| Toggle dark mode | **View → Dark Mode** |

## Configuration file

Located at `~/.config/premiumize-explorer/premiumize-explorer.ini` on Linux and `%APPDATA%\premiumize-explorer\premiumize-explorer.ini` on Windows.

```ini
[auth]
api_key=your_key_here

[ui]
last_local_path=/home/user/Downloads
window_geometry=...
splitter_sizes=...
```

## Project structure

```
src/
├── api/          PremiumizeApi — all HTTP calls, fire-and-emit
├── config/       AppConfig — QSettings singleton
├── model/        PremiumizeModel — QAbstractListModel for the cloud pane
├── transfer/     TransferManager, UploadJob, DownloadJob
└── ui/           MainWindow, FilePane, TransferProgressWindow, LogWindow, CloudTransfersWindow, dialogs
```

## API

Uses the [Premiumize.me REST API](https://www.premiumize.me/api) with Bearer token authentication. All requests are async via `QNetworkAccessManager`; results are delivered through Qt signals.

## License

MIT
