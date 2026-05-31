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
| `PremiumizeModel` | `src/model/PremiumizeModel.hpp` | `QAbstractListModel` for the cloud pane (flat list, one folder at a time). Injects a virtual "↑ Up" row at position 0 when not at root; `showUpEntry()` (private) derives visibility from `currentFolderId_` / `parentFolderId_` — no separate stored flag. Use `isUpEntry(row)` and `itemAtViewRow(row)` (returns nullptr for the up row or OOB) at every call site. Emits `application/x-premiumize-items` MIME for drag-and-drop. |
| `UpEntryProxyModel` | `src/model/UpEntryProxyModel.hpp` | `QIdentityProxyModel` wrapping `QFileSystemModel` for the local pane. Inserts a virtual "↑ Up" row at position 0 when not at root. A heap-allocated `QObject* sentinel_` is used as `internalPointer` to identify the virtual row. `viewRoot_` is a plain `QModelIndex` (not `QPersistentModelIndex`) — `endResetModel()` invalidates persistent indices. Not used for the cloud pane. |
| `FilePane` | `src/ui/FilePane.hpp` | Reusable widget used for both panes. `PaneType::Local` wraps `QFileSystemModel` via `UpEntryProxyModel`; `PaneType::Cloud` drives `PremiumizeModel`. The cloud pane's download destination is kept in sync with the local pane via `setDownloadPath()`, called by `MainWindow` on every `localPathChanged`. Handles all drag/drop events and emits typed signals upward. |
| `TransferManager` | `src/transfer/TransferManager.hpp` | Job queue, max 2 concurrent. Job pointers are passed explicitly to `onJobFinished` — never use `QObject::sender()` here, it is unreliable when called from a regular member function. |
| `UploadJob` | `src/transfer/UploadJob.hpp` | Two-step upload: fetches token/URL via `PremiumizeApi::fetchUploadInfo()` (private reply), then POSTs `QHttpMultiPart`. Uses its own `QNetworkAccessManager`. |
| `DownloadJob` | `src/transfer/DownloadJob.hpp` | Streams `QNetworkReply` bytes directly to a `QFile`. Reply comes from `PremiumizeApi::startDownload`. |
| `MainWindow` | `src/ui/MainWindow.hpp` | Wires everything together. Owns all subsystem instances. Connects `PremiumizeApi` signals to pane updates and `FilePane` signals to API calls / transfer enqueuing. |

### Upload flow (two-step API requirement)

1. `TransferManager::enqueueUpload` → creates `UploadJob`
2. `UploadJob::start` → calls `api_->fetchUploadInfo(folderId)`, which returns a `QNetworkReply*` owned by this job only (not a shared signal)
3. `UploadJob` parses the reply, validates `token` and `url`, then calls `startUpload()`
4. `startUpload` constructs `QHttpMultiPart` with `token` field + file, POSTs to `url` via its own `QNetworkAccessManager`

**Do not use `requestUploadInfo` from UploadJob** — that emits a shared signal (`uploadInfoReady`) which would be received by all concurrently running upload jobs, giving them all the same one-time token. `fetchUploadInfo` returns a private reply instead.

### Startup sequence

`main.cpp` checks `AppConfig::isConfigured()`. If no API key, `ApiKeyDialog` runs modally before `MainWindow` is constructed. On close, `MainWindow` saves geometry and splitter state to `AppConfig`.

### Drag-and-drop

- Local → Cloud: `text/uri-list` dropped on cloud `FilePane` → `uploadRequested` signal
- Cloud → Local: `application/x-premiumize-items` (JSON array of `{id, name, isFolder, link, size}`) dropped on local `FilePane` → `downloadRequested` signal per non-folder item

### Model index safety

`PremiumizeModel::itemAt(row)` uses `Q_ASSERT` + `std::vector::at()` (throws `std::out_of_range` on bad access). All call sites in `FilePane` guard with an explicit row-range check before calling `itemAt`. When adding new call sites, always check `row >= 0 && row < cloudModel_->rowCount()` first — the model can be reset asynchronously by an in-flight `listFolder` reply.

### deleteItem signal contract

`PremiumizeApi::deleteItem` uses a **single** `QNetworkReply::finished` handler that emits either `deleteFinished(true)` on success or `deleteFinished(false, error)` on failure. It does **not** call `handleJsonReply` (which would also emit `networkError`, causing a double response). Do not add a second `connect` to a reply that already has one — both handlers fire on the same signal emission.

### "↑ Up" virtual row

Both panes show a "↑ Up" entry at row 0 whenever not at root. Each pane handles it differently:

- **Cloud**: `PremiumizeModel` inserts the row internally. `showUpEntry()` (private method) derives visibility from `currentFolderId_` and `parentFolderId_` so it is always in sync with the folder state — there is no separate flag that can drift.
- **Local**: `UpEntryProxyModel` inserts the row. `viewRoot_` stores the proxy root index as a plain `QModelIndex`, not `QPersistentModelIndex` — `endResetModel()` would immediately invalidate a persistent index, breaking the proxy.

Always call `isUpEntry` before accessing item data. Never store row offsets across model resets.

### cancelAll() iterator safety

`QNetworkReply::abort()` emits `finished` synchronously (direct connection, same thread), which chains through `on_finished()` → `onJobFinished()` → `active_.erase()`. `cancelAll()` therefore iterates a **snapshot copy** of `active_` so the original can be modified freely during cancellation. Preserve this pattern if new cancellation paths are added.

### Theme

Qlementine v1.4.2 is fetched via CMake `FetchContent` on first configure. Applied in `main.cpp` via `QApplication::setStyle(new oclero::qlementine::QlementineStyle(&app))`. Include path: `<oclero/qlementine/style/QlementineStyle.hpp>`.

Dark mode is toggled via **View → Dark Mode** and persisted in `AppConfig` (`ui/dark_mode`). `MainWindow::applyTheme(bool)` loads `:/themes/dark.json` (bundled in `src/resources/resources.qrc`) and calls `setDarkModeEnabled` only if the theme was applied successfully — not unconditionally.

## LSP / clangd

clangd will show Qt type errors because it does not know Qt's include paths by default. These are false positives — the CMake build is authoritative. Run `cmake --build` to check for real errors. A `compile_commands.json` is generated at `build/debug/compile_commands.json`; symlinking it to the root resolves clangd errors:

```bash
ln -sf build/debug/compile_commands.json compile_commands.json
```
