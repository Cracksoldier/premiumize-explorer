# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure (first time — downloads Qlementine via FetchContent, takes ~30s)
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/debug --parallel

# Run
./build/debug/src/premiumize-explorer

# Release build
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
```

AddressSanitizer is available but off by default: `-DPMEX_ENABLE_ASAN=ON`.

When adding a new `.cpp` file, add it to `src/CMakeLists.txt`'s `add_executable` list — there is no glob.

## Architecture

The app is a two-pane file browser (Total Commander style): local filesystem on the left, Premiumize.me cloud on the right. All subsystems communicate through Qt signals/slots.

### Data flow

```
PremiumizeApi  ──signals──▶  MainWindow  ──calls──▶  FilePane / PremiumizeModel
     ▲                           │
     │                           ▼
TransferManager  ◀──enqueue──  FilePane (drag-drop / context menu)
     │
     ▼
TransferProgressWindow  (non-modal Qt::Tool window)
```

### Key classes

| Class | File | Role |
|---|---|---|
| `api::PremiumizeApi` | `src/api/PremiumizeApi.hpp` | All HTTP calls; fire-and-emit (never blocking). Auth via `Authorization: Bearer` header. |
| `api::ApiTypes` | `src/api/ApiTypes.hpp` | Plain data structs: `FolderItem`, `FolderListing`, `UploadInfo`, `AccountInfo`. |
| `AppConfig` | `src/config/AppConfig.hpp` | Singleton QSettings facade. Writes to `~/.config/premiumize-explorer/premiumize-explorer.ini`. |
| `PremiumizeModel` | `src/model/PremiumizeModel.hpp` | `QAbstractListModel` for the cloud pane (flat list, one folder at a time). Emits `application/x-premiumize-items` MIME for drag-and-drop. |
| `FilePane` | `src/ui/FilePane.hpp` | Reusable widget used for both panes. `PaneType::Local` drives `QFileSystemModel`; `PaneType::Cloud` drives `PremiumizeModel`. Handles all drag/drop events and emits typed signals upward. |
| `TransferManager` | `src/transfer/TransferManager.hpp` | Job queue, max 2 concurrent. Holds `UploadJob` / `DownloadJob` instances and forwards their progress signals with a stable `jobId`. |
| `UploadJob` | `src/transfer/UploadJob.hpp` | Two-step upload: calls `requestUploadInfo` → receives token/URL → POSTs `QHttpMultiPart`. Uses its own `QNetworkAccessManager`. |
| `DownloadJob` | `src/transfer/DownloadJob.hpp` | Streams `QNetworkReply` bytes directly to a `QFile`. Reply comes from `PremiumizeApi::startDownload`. |
| `MainWindow` | `src/ui/MainWindow.hpp` | Wires everything together. Owns all subsystem instances. Connects `PremiumizeApi` signals to pane updates and `FilePane` signals to API calls / transfer enqueuing. |

### Upload flow (two-step API requirement)

1. `TransferManager::enqueueUpload` → creates `UploadJob`
2. `UploadJob::start` → calls `api_->requestUploadInfo(folderId)`
3. `PremiumizeApi` emits `uploadInfoReady(token, url)`
4. `UploadJob` constructs `QHttpMultiPart` with `token` field + file, POSTs to `url`

### Startup sequence

`main.cpp` checks `AppConfig::isConfigured()`. If no API key, `ApiKeyDialog` runs modally before `MainWindow` is constructed. On close, `MainWindow` saves geometry and splitter state to `AppConfig`.

### Drag-and-drop

- Local → Cloud: `text/uri-list` dropped on cloud `FilePane` → `uploadRequested` signal
- Cloud → Local: `application/x-premiumize-items` (JSON array of `{id, name, isFolder, link, size}`) dropped on local `FilePane` → `downloadRequested` signal per non-folder item

### Theme

Qlementine v1.4.2 is fetched via CMake `FetchContent` on first configure. Applied in `main.cpp` via `QApplication::setStyle(new oclero::qlementine::QlementineStyle(&app))`. Include path: `<oclero/qlementine/style/QlementineStyle.hpp>`.

## LSP / clangd

clangd will show Qt type errors because it does not know Qt's include paths by default. These are false positives — the CMake build is authoritative. Run `cmake --build` to check for real errors. A `compile_commands.json` is generated at `build/debug/compile_commands.json`; symlinking it to the root resolves clangd errors:

```bash
ln -sf build/debug/compile_commands.json compile_commands.json
```
