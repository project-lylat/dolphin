# Upstream merge notes

Branch `lylat/upstream-sync`, based on `origin/lylat/dolphin-matchmaking`.
Upstream is `upstream/master` of dolphin-emu/dolphin. The fork's merge base
was `664663e8de` (5.0-16607, June 2022), 9185 upstream commits behind.

The merge is done in steps. Each step merges one upstream commit (a first-parent
merge commit or a release tag), builds Release x64, and is then committed.

Build used for every step (CMake + Ninja, VS 2022 17.6, MSVC 19.36):

```
tmp-dolphin\build.bat [configure]
  cmake -S <worktree> -B tmp-dolphin\build -G Ninja -DCMAKE_BUILD_TYPE=Release
        -DENABLE_TESTS=OFF -DENABLE_NOGUI=OFF -DENABLE_LLVM=OFF -DENABLE_AUTOUPDATE=OFF
  cmake --build tmp-dolphin\build -j 2
```

## What the fork carries (kept through every step)

- Lylat matchmaking: `Core/Lylat/*`, the Lylat connection type and tab in
  `NetPlaySetupDialog`, the Lylat toolbar button and game list action, the `-M`
  command line flag, `MainWindow` matchmaking glue.
- `NetPlayServer` traversal-connect callback and `GetServerID()`.
- Netplay: chat on/off, auto start, the "Recommended Buffer" column, the
  "Load Preloaded Saves" option (copies `Sys/Wii/title` into the session NAND)
  and its flag in the start-game packet.
- Default setting changes (D3D11 first, graphics hacks, volume 50, and so on).
- `WiiSave::SaveExists` uses `&&` (fork fix).

## Step 1: `052c7395fb` (PR #9624, 929 commits)

Conflicts:

- `Core/Config/NetplaySettings.cpp`: upstream replaced the save settings with
  `NETPLAY_SAVEDATA_LOAD/WRITE/SYNC_ALL_WII` and dropped `LoadWiiSave`. Took the
  new names and kept the fork's defaults (write off, all Wii saves on, golf
  overlay off). Kept `NETPLAY_PRELOADED_SAVES`, `NETPLAY_ENABLE_CHAT`,
  `NETPLAY_ENABLE_AUTO_START_GAME`.
- `Core/NetPlayProto.h`: upstream renamed every `NetSettings` field to
  snake_case. Took upstream and re-added the fork fields as
  `load_preloaded_save_files` and `is_chat_enabled`.
- `Core/NetPlayClient.cpp`, `Core/NetPlayServer.cpp`: took upstream's renamed
  packet code and re-added `load_preloaded_save_files` right before
  `strict_settings_sync` on both the send and receive side, so host and client
  stay in step. Kept `GetServerID()` next to upstream's new helpers.
- `DolphinQt/NetPlay/NetPlayDialog.*`: upstream replaced the three save
  checkboxes with a radio group (none / load / load and write) plus "Use All
  Wii Save Data". Took that and added "Load Preloaded Saves" after it, with its
  load/save/enable lines. The tooltip now names the new menu items.
- `DolphinQt/MainWindow.h`: kept both include sets.
- `VideoCommon/VideoBackendBase.cpp`: kept the fork's D3D11/D3D12-first order
  and added upstream's new Metal backend line.

Build fixes:

- `Core/Lylat/LylatMatchmakingClient.cpp`: `Common::Timer::GetTimeMs()` was
  removed upstream; use `Common::Timer::NowMs()`.

Build status: reached [1275/1810] with only the error above, then got as far
as [193/689] on the rebuild before C: ran out of space. Not yet fully built.

## Step 2: `ed9915308b` (PR #11386, 1548 commits)

Conflicts (all default values next to new upstream settings):

- `Core/Config/GraphicsSettings.cpp`, `VideoCommon/VideoConfig.h`, Android
  `BooleanSetting.java`: upstream added the new performance overlay settings
  (`ShowFTimes`, `ShowVPS`, `ShowGraphs`, ...). Took upstream and kept the
  fork's defaults: `ShowFPS` on and `ShowNetPlayPing` on.

Build status: not built yet (C: is full; builds are on hold).

## Step 3: `db40348c86` (PR #11695, 2488 commits)

Conflicts:

- `VideoCommon/VideoBackendBase.cpp`: upstream moved Metal to the front of the
  list (`emplace` at the start) and kept D3D after OpenGL. Kept the fork's
  D3D11/D3D12-first block and dropped the old Metal `push_back` (upstream's
  `emplace` covers it).
- Android `BooleanSetting.java` and `IntSetting.java` were converted to Kotlin
  upstream. Removed the Java files and put the fork's defaults into the `.kt`
  files: `ShowFPS` on, `UsePanicHandlers` off, volume 50.

Build status: not built yet (C: is full).

## Step 4: `5512d19d4b` (PR #12061, 3265 commits)

Conflicts:

- `Core/NetPlayClient.cpp` (`ThreadFunc`): kept upstream's start log line and
  the fork's "If your opponent does not connect..." chat line.
- `DolphinQt/MainWindow.cpp` (destructor): kept the fork's
  `delete m_lylat_matchmaking_client` and upstream's RetroAchievements shutdown.
- `DolphinQt/ToolBar.cpp`: upstream replaced `GetScaledThemeIcon` with
  `GetThemeIcon`. Took upstream and moved the Lylat "Find Match" icon to the
  new call.

Build status: not built yet (C: is full).

## Step 5: `3cb5d3455f` (PR #12462, 4235 commits)

Conflicts:

- `Core/Config/GraphicsSettings.cpp`: upstream added custom aspect ratio and
  widescreen heuristic settings. Took upstream and kept the fork's defaults
  (widescreen hack on, aspect ratio Analog Wide).
- `DolphinQt/Main.cpp`: upstream replaced `SetCurrentUserStyle` with
  `InitDefaultPalette/UpdateSystemDark/ApplyStyle`, called before the main
  window is made. Took that and kept the fork's `MainWindow` constructor with
  the `-M` matchmaking game path.
- `DolphinQt/MainWindow.cpp`: took upstream's RetroAchievements init. In
  `NetPlayHostWithCallback` kept the fork's server with the traversal-connect
  callback and passed upstream's new `traversal_port_alt`.
- `DolphinQt/NetPlay/NetPlaySetupDialog.cpp`: upstream rebuilt the ALERT label
  inside the connect tab. Took upstream and removed the fork's older copy of
  the label higher up.

Build status: not built yet (C: is full).

## Step 6: tag `2407` (5205 commits)

Conflicts:

- `VideoCommon/VideoBackendBase.cpp`: upstream now lists D3D11/D3D12 before
  OpenGL on Windows, which is what the fork did. The file now matches upstream.
- `DolphinQt/GameList/GameList.cpp`, `DolphinQt/MainWindow.cpp`:
  `Core::IsRunning()` now takes a `Core::System&`. Kept the fork's
  `NetPlayHost` -> `NetPlayHostWithCallback` split and passed
  `Core::System::GetInstance()` in every fork call (GameList "Find Match with
  Lylat", `NetPlaySearch`, `NetPlayHostWithCallback`, and the two auto-start
  checks in `NetPlayDialog.cpp`).

Build status: not built yet (C: is full).

## Step 7: tag `2503` (6151 commits)

Conflicts:

- `DolphinQt/MainWindow.h/.cpp`, `DolphinQt/Main.cpp`: upstream's
  `MainWindow` now takes a `Core::System&` and `MainWindow::Show()` is gone
  (the constructor shows the window and starts a pending boot). The constructor
  now takes the system, the boot parameters, the movie path and the fork's
  `-M` matchmaking game path. The fork's `-M` handling moved from `Show()` to
  the end of the constructor, right after upstream's pending-boot start.
  `NetPlaySearch`/`NetPlayHostWithCallback` use `!Core::IsUninitialized(m_system)`
  like upstream's `NetPlayJoin`. `Common::Timer::GetTimeMs()` -> `NowMs()`.
- `DolphinQt/GameList/GameListModel.cpp`: kept the fork's
  `AddOrGetFromCache` and upstream's `OnEmulationStateChanged` plus includes.

Build status: not built yet (C: is full).

## Step 8: tag `2509` (7202 commits)

Conflicts:

- `.gitmodules`: upstream moved the bundled Windows Qt forward. The fork
  pointed Qt at `project-lylat/ext-win-qt`, which does not have the new Qt
  commit, so Qt now comes from `dolphin-emu/ext-win-qt` again.
- Android `IntSetting.kt`: took upstream's new `AudioBufferSize` and kept the
  fork's volume 50.
- `Core/NetPlayServer.h`, `DolphinQt/MainWindow.h`: upstream marked the
  destructors `override`. Kept the fork's extra constructor arguments (the
  traversal-connect callback, the `-M` game path).
- `VideoCommon/VideoConfig.h`: upstream dropped `bShowNetPlayPing` from
  `VideoConfig`; the ping overlay now reads `GFX_SHOW_NETPLAY_PING`, whose
  default is still the fork's `true`. Took upstream.
- `DolphinQt/NetPlay/NetPlaySetupDialog.cpp`: upstream simplified
  `OnConnectionTypeChanged` for its two connection types. Kept the fork's
  three-way switch (Lylat, direct, traversal); the fork's two-list
  `PopulateGameList` stays.
- `DolphinQt/NetPlay/NetPlayDialog.cpp`: upstream made `is_hosting` const;
  kept the fork's `auto_start` next to it.
- `DolphinQt/MainWindow.cpp`: kept the fork's "Find Match" toolbar and
  double-click-to-search connections, with upstream's lambda style.

Build status: not built yet (C: is full).

## Step 9: tag `2603` (8254 commits)

Conflicts:

- `Core/Config/GraphicsSettings.cpp`: took upstream's new `CapImmediateXFB`
  and kept the fork's `ImmediateXFBEnable` default (on).
- `Core/WiiRoot.cpp`: upstream dropped the `SessionSettings.h` include. Kept
  `NetplaySettings.h`, which the fork's preloaded-saves copy needs.
- `DolphinQt/MainWindow.cpp`: kept the Lylat include and dropped
  `WiimoteDevice.h` with upstream. Kept the fork's `SearchAndOpen` and
  `OpenLylatJSON` next to upstream's new `OpenConfigFolder`/`OpenCacheFolder`.
- `DolphinQt/NetPlay/NetPlaySetupDialog.cpp`: upstream wrapped the UPnP
  connect in a Qt version check. Took that and kept the Lylat connections after
  it. Also fixed a fork bug in the same block: the Lylat game list saved the
  name from `m_host_games` instead of `m_lylat_games`.

Build status: not built yet (C: is full).

## Step 10: `upstream/master` (`8d2ad1a662`, all 9185 commits)

Conflicts:

- `Source/Core/DolphinLib.props`: upstream removed the Visual Studio projects
  and builds Windows with CMake only. Removed the file; the Lylat sources are
  already in `Source/Core/Core/CMakeLists.txt`.
- `DolphinQt/GameList/GameList.h`: upstream changed the `SelectionChanged`
  signal to take a const reference. Kept the fork's `NetPlaySearch` signal.
- Android `BooleanSetting.kt`: took upstream's crop settings and kept
  `ShowFPS` on.

Follow-up from the solution removal:

- `build-windows.bat` and the Windows job in `.github/workflows/main.yml` now
  run CMake + Ninja instead of `msbuild dolphin-emu.sln` (the output is in
  `Build\x64\Binaries`; CI copies it to `Binary\x64` for the old packaging
  step). The CI change is not tested.

Build status: not built yet (C: is full).

## After the merge

- `Core/NetPlayServer.cpp`: the fork's traversal callback and `GetServerID()`
  now use `Common::g_TraversalClient` (upstream moved it into `Common`), and the
  callback is skipped when empty.

## QR sign-in (not part of the merge)

- `Core/Lylat/LylatDeviceLogin.*`: the device-code client. It follows the
  Lylat server contract (`docs/device-login.md` on lylat `facelift`):
  `POST <LylatURL>/api/device/code`, then `POST /api/device/token` every
  `interval` seconds on its own thread. `authorization_pending` keeps polling,
  `slow_down` adds 5 s, `expired_token`/`invalid_grant` -> Expired,
  `access_denied` -> Denied, six failed polls in a row -> Error, 404 -> the old
  browser flow. On 200 the body is checked with `LylatUser::GetUserFromDisk`
  and written to `User/lylat.json` (temp file, then rename), the file the
  manual login writes. The device code never leaves the worker thread.
- `DolphinQt/NetPlay/LylatQRLoginDialog.*`: the Qt dialog (QR, short code,
  "Pick NN on your phone", address, countdown; Retry, Manual login, Cancel).
- `NetPlaySetupDialog.cpp`: Sign In opens the dialog. Manual login and a 404
  open `<LylatURL>/users/enable` as before.
- New setting `[NetPlay] LylatURL` (default `https://lylat.gg`) so a local or
  mock server can be used.
- `Externals/qrcodegen`: Nayuki's QR Code generator (C, MIT, v1.8.0, copied
  from doldecomp `native/source/vendor/qrcodegen`), listed in
  `Externals/licenses.md`.
