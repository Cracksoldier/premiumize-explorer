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
| `api::ApiTypes` | `src/api/ApiTypes.hpp` | Plain data structs: `FolderItem`, `FolderListing`, `UploadInfo`, `AccountInfo`, `CloudTransferEntry`. |
| `AppConfig` | `src/config/AppConfig.hpp` | Singleton QSettings facade. Writes to `~/.config/premiumize-explorer/premiumize-explorer.ini`. |
| `PremiumizeModel` | `src/model/PremiumizeModel.hpp` | `QAbstractListModel` for the cloud pane (flat list, one folder at a time). Injects a virtual "↑ Up" row at position 0 when not at root; `showUpEntry()` (private) derives visibility from `currentFolderId_` / `parentFolderId_` — no separate stored flag. Use `isUpEntry(row)` and `itemAtViewRow(row)` (returns nullptr for the up row or OOB) at every call site. Emits `application/x-premiumize-items` MIME for drag-and-drop. |
| `UpEntryProxyModel` | `src/model/UpEntryProxyModel.hpp` | `QIdentityProxyModel` wrapping `QFileSystemModel` for the local pane. Inserts a virtual "↑ Up" row at position 0 when not at root. A heap-allocated `QObject* sentinel_` is used as `internalPointer` to identify the virtual row. `viewRoot_` is a plain `QModelIndex` (not `QPersistentModelIndex`) — `endResetModel()` invalidates persistent indices. Not used for the cloud pane. |
| `FilePane` | `src/ui/FilePane.hpp` | Reusable widget used for both panes. `PaneType::Local` wraps `QFileSystemModel` via `UpEntryProxyModel`; `PaneType::Cloud` drives `PremiumizeModel`. The cloud pane's download destination is kept in sync with the local pane via `setDownloadPath()`, called by `MainWindow` on every `localPathChanged`. Handles all drag/drop events and emits typed signals upward. |
| `TransferManager` | `src/transfer/TransferManager.hpp` | Job queue, max 2 concurrent. `queue_` and `active_` store `pair<int, …>` so job IDs are preserved through `dispatchNext()`. `cancelJob(int)` cancels a single job without disturbing unrelated transfers; `cancelAll()` cancels everything. Never use `QObject::sender()` in `onJobFinished` — pass job pointers explicitly. Emits `uploadFinished(folderId, success, error)` per upload so `MainWindow` can auto-refresh the cloud pane. |
| `UploadJob` | `src/transfer/UploadJob.hpp` | Two-step upload: fetches token/URL via `PremiumizeApi::fetchUploadInfo()` (private reply), then POSTs a manually constructed `QByteArray` multipart body. Uses its own `QNetworkAccessManager` with `Http2AllowedAttribute=false`. |
| `DownloadJob` | `src/transfer/DownloadJob.hpp` | Streams `QNetworkReply` bytes directly to a `QFile`. Reply comes from `PremiumizeApi::startDownload`. |
| `LogWindow` | `src/ui/LogWindow.hpp` | Non-modal `Qt::Tool` window showing a timestamped log of all API requests and responses. Opened via **View → API Log**. "Save to File…" and "Clear" buttons. Fed by `PremiumizeApi::requestLogged` signal. |
| `CloudTransfersWindow` | `src/ui/CloudTransfersWindow.hpp` | Non-modal `Qt::Tool` window showing Premiumize server-side transfers (`GET /transfer/list`). Displays name, status badge, progress bar, speed, and ETA. Polls every 5 s while visible; stops polling when hidden. Opened via **View → Cloud Transfers**. |
| `BatchDownloadWizard` | `src/ui/BatchDownloadWizard.hpp` | Modal `QWizard` with three pages: `SearchPage` (keyword search via `GET /api/folder/search` → checkbox list), `DestinationPage` (local path picker), `ProgressPage` (dual progress bars + timer + scrollable per-file status list). Downloads files sequentially via `TransferManager`. Opened via **File → Batch Download… (Ctrl+Shift+D)**. |
| `FormatHelpers` | `src/ui/FormatHelpers.hpp` | Shared inline helpers `ui::formatBytes(qint64)` and `ui::formatDuration(qint64 ms)` used by `TransferProgressWindow`, `CloudTransfersWindow`, and `BatchDownloadWizard`. |
| `MainWindow` | `src/ui/MainWindow.hpp` | Wires everything together. Owns all subsystem instances. Connects `PremiumizeApi` signals to pane updates and `FilePane` signals to API calls / transfer enqueuing. |

### Upload flow (two-step API requirement)

1. `TransferManager::enqueueUpload` → creates `UploadJob`
2. `UploadJob::start` → calls `api_->fetchUploadInfo(folderId)`, which returns a `QNetworkReply*` owned by this job only (not a shared signal)
3. `UploadJob` parses the reply, validates `token` and `url`, then calls `startUpload()`
4. `startUpload` builds a raw `QByteArray` multipart body, sets explicit `Content-Type` and `Content-Length` headers, forces `Http2AllowedAttribute=false`, and POSTs to `url` via its own `QNetworkAccessManager`. **Do not replace with `QHttpMultiPart`** — the energycdn.com CDN rejects Qt's serialized multipart with HTTP 500.
5. On completion, `TransferManager` emits `uploadFinished(folderId, success, error)`. `MainWindow` refreshes the cloud pane if `folderId` matches the currently displayed folder.

**Field order is significant**: `token` must precede `file` in the multipart body — the CDN returns HTTP 500 if `file` comes first.

**Do not use `requestUploadInfo` from UploadJob** — that emits a shared signal (`uploadInfoReady`) which would be received by all concurrently running upload jobs, giving them all the same one-time token. `fetchUploadInfo` returns a private reply instead.

### Startup sequence

`main.cpp` checks `AppConfig::isConfigured()`. If no API key, `ApiKeyDialog` runs modally before `MainWindow` is constructed. On close, `MainWindow` saves geometry and splitter state to `AppConfig`.

### Drag-and-drop

- Local → Cloud: `text/uri-list` dropped on cloud `FilePane` → `uploadRequested` signal
- Cloud → Local: `application/x-premiumize-items` (JSON array of `{id, name, isFolder, link, size}`) dropped on local `FilePane` → `downloadRequested` signal per non-folder item

The `QListView` in each pane uses `DragOnly` mode (`setDragDropMode(DragOnly)`), **not** `DragDrop`. This is critical: `DragDrop` mode makes the viewport call `setAcceptDrops(true)`, intercepting drops through the model's `canDropMimeData`/`dropMimeData` chain (which has no signal-emitting implementation). With `DragOnly`, the viewport has `setAcceptDrops(false)`, so Qt delivers drop events directly to the `FilePane` parent widget where `dragEnterEvent`/`dropEvent` are implemented.

### Model index safety

`PremiumizeModel::itemAt(row)` uses `Q_ASSERT` + `std::vector::at()` (throws `std::out_of_range` on bad access). All call sites in `FilePane` guard with an explicit row-range check before calling `itemAt`. When adding new call sites, always check `row >= 0 && row < cloudModel_->rowCount()` first — the model can be reset asynchronously by an in-flight `listFolder` reply.

### deleteItem signal contract

`PremiumizeApi::deleteItem` uses a **single** `QNetworkReply::finished` handler that emits either `deleteFinished(true)` on success or `deleteFinished(false, error)` on failure. It does **not** call `handleJsonReply` (which would also emit `networkError`, causing a double response). Do not add a second `connect` to a reply that already has one — both handlers fire on the same signal emission.

### "↑ Up" virtual row

Both panes show a "↑ Up" entry at row 0 whenever not at root. Each pane handles it differently:

- **Cloud**: `PremiumizeModel` inserts the row internally. `showUpEntry()` (private method) derives visibility from `currentFolderId_` and `parentFolderId_` so it is always in sync with the folder state — there is no separate flag that can drift.
- **Local**: `UpEntryProxyModel` inserts the row. `viewRoot_` stores the proxy root index as a plain `QModelIndex`, not `QPersistentModelIndex` — `endResetModel()` would immediately invalidate a persistent index, breaking the proxy.

Always call `isUpEntry` before accessing item data. Never store row offsets across model resets.

### Batch download wizard

`BatchDownloadWizard` (`src/ui/BatchDownloadWizard.hpp`) is a modal `QWizard` that downloads multiple cloud files sequentially:

1. **SearchPage** — calls `api_->searchItems(query)` (`GET /api/folder/search?q=`), shows results as a checkbox list filtered to files with a valid `link`. Items without a link are silently skipped.
2. **DestinationPage** — picks a writable local directory. `initializePage()` reads the file list from `SearchPage` for the summary label.
3. **ProgressPage** — `initializePage()` calls `startNextFile()` which enqueues one download at a time via `TransferManager::enqueueDownload`. The wizard tracks the active job ID in `currentJobId_` so progress signals from unrelated jobs are ignored. A scrollable `QListWidget` below the progress bars shows every file with a live status icon: pending (`SP_FileIcon`), active (`SP_BrowserReload`), success (`SP_DialogOkButton`), error/skipped (`SP_MessageBoxCritical`), or cancelled (`SP_DialogCancelButton`). Both successfully-downloaded and link-less skipped items call `scrollToItem` so the active row is always visible.

**Cancel safety**: `cancelBatch()` sets `cancelled_ = true`, calls `manager_->cancelJob(currentJobId_)`, and marks `allDone_`. `on_jobFinished` guards `if (cancelled_) return` to skip the synchronous re-entrant call that fires when `cancelJob` aborts the in-flight reply. `BatchDownloadWizard::reject()` calls `cancelBatch()` before delegating to `QWizard::reject()`, so Escape and the window X button are also safe.

### cancelAll() and cancelJob() iterator safety

`QNetworkReply::abort()` emits `finished` synchronously (direct connection, same thread), which chains through `on_finished()` → `onJobFinished()` → `active_.erase()`. `cancelAll()` therefore iterates a **snapshot copy** of `active_` so the original can be modified freely during cancellation. `cancelJob(int)` exits its search loop and saves a pointer before calling `cancel()`, for the same reason. Preserve this pattern if new cancellation paths are added.

### Theme

Qlementine v1.4.2 is fetched via CMake `FetchContent` on first configure. Applied in `main.cpp` via `QApplication::setStyle(new oclero::qlementine::QlementineStyle(&app))`. Include path: `<oclero/qlementine/style/QlementineStyle.hpp>`.

Dark mode is toggled via **View → Dark Mode** and persisted in `AppConfig` (`ui/dark_mode`). `MainWindow::applyTheme(bool)` loads `:/themes/dark.json` (bundled in `src/resources/resources.qrc`) and calls `setDarkModeEnabled` only if the theme was applied successfully — not unconditionally.

## LSP / clangd

clangd will show Qt type errors because it does not know Qt's include paths by default. These are false positives — the CMake build is authoritative. Run `cmake --build` to check for real errors. A `compile_commands.json` is generated at `build/debug/compile_commands.json`; symlinking it to the root resolves clangd errors:

```bash
ln -sf build/debug/compile_commands.json compile_commands.json
```
