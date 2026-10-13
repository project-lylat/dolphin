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
