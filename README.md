# NBT-AA-Installer

> **Version 0.1.0.** It shows in the window title and at the top of every log.
>
> **Status (2026-11-20): built, not yet run against a car.** Tested on a PC without a car: the GitHub list, downloading
> and checking a release, reading a folder, the settings tabs and the File tab, and the 45 checks in
> `tests\`. Connect, Send to car, Write .env only, Roll back and Pull car logs have not talked to a car yet. Try
> Connect first: it only reads.

A Windows tool that installs or updates [NBT-AAD](https://github.com/Sq00ky/Supra-iDrive6-Android-Auto) (wireless
Android Auto for BMW / Toyota NBT Evo, iDrive 6 head units) over SSH, with no USB stick, and edits its settings file.

- **Send NBT-AAD to the car** from either of two places:
  - **From GitHub**: pick one of the last 20 releases on NBT-AAD's releases page. Stable releases are marked
    `[stable]`.
  - **From this PC**: a folder, for example your own build.
- **Edit `nbt-aad.env`** (NBT-AAD's settings) with checkboxes, lists and fields instead of a text editor, or edit the
  raw file in a File tab. The tool keeps to the car's limits: at most 15 settings and 159 characters per line.
- **Find the car** on the ENET cable, so you don't need to know its address.
- **Log every step** to the window and to a file, so a failed install can be understood and undone.

What it doesn't do: unlock SSH on the head unit (use Bavarian SSH or the TurboCode tools first), build NBT-AAD, or
include NBT-AAD itself. Releases are downloaded from NBT-AAD's official releases page only when you ask.

## Using it

1. **Unlock SSH on the head unit first** (see NBT-AAD's
   [install guide](https://github.com/Sq00ky/Supra-iDrive6-Android-Auto/blob/main/install.md)).
2. Connect the PC to the car with an **OBD-to-ENET cable**, ignition on.
3. Run `NBT-AA-Installer.exe` and press **Connect**. With **Host** empty (or nothing answering there) it first finds
   the car on the ENET cable and fills in Host; then it reads what's on the car: install type, versions, the current
   `nbt-aad.env` and the iDrive Android Auto options. Nothing is changed.
4. Choose what to send:
   - **From GitHub**: select a release in the list. `[stable]` marks stable releases, `[on car]` the one already
     installed. The newest stable one is selected for you.
   - **From this PC**: press **Folder…** and pick the folder. It must hold `nbt-aad` and the 5 plugin files
     (`libNbtAad.so`, `NbtAad.manifest`, `NbtAad.xml`, `NbtAad_ALL_Texts.zip`, `NbtAad_ALL_Images.zip`), loose or in
     subfolders up to 3 levels deep; a `Releases.zip` with the plugin files inside the folder is fine too.
5. Set the options you want in the tabs (Common, Features, Advanced, iDrive overrides, File). Anything left on
   **Default** isn't written to the file.
6. Press **Send to car** in the section you chose. The tool asks for confirmation, then uploads the new files next to
   the current ones and checks them, stops Android Auto, swaps them in (the current files become `.old`), writes the
   settings, sets up the firewall rule (`nbt-aad --pf-install`) and, if **Restart iDrive** is ticked, restarts iDrive.
   **If anything fails, the car is never left half updated**: before the swap nothing has changed, and after it the
   previous version and settings are put back from the `.old` files automatically.

There's no port or login to enter: the car's SSH port is always 22 (the TurboCode proxy on `127.0.0.1` uses 6969), and
every NBT Evo has the same `root` login from NBT-AAD's install guide, built into the tool.

Other buttons: **Write .env only** (settings without new files), **Roll back** (puts the `.old` files back), **Pull car
logs** (copies NBT-AAD's logs from the car into the `logs` folder).

### Rules the settings tabs follow

- **15 settings at most.** The car passes only the first 15 settings lines to Android Auto and ignores the rest. The
  tabs show a counter and won't add a 16th. If the file already has more, **Send to car** warns and asks before sending.
- **159 characters per line at most.** A longer line is cut in two by the car. Sending is blocked until it's fixed.
- **Default means no line.** Choosing Default removes the setting from the file.
- **iDrive overrides** are settings that also exist as checkboxes in iDrive's Android Auto menu. A line in the file wins
  over the checkbox, so that tab is locked until you tick its master checkbox and accept a warning. Unticking it again
  turns those lines into `#~off ` comments, so they can come back later.
- **The File tab is the truth.** You can type anything there. Lines the tool doesn't know are kept as they are. A wrong
  value gets a ⚠ on its control but is only changed when you use that control.

### Files next to the .exe

| File | What |
|---|---|
| `data\settings.json` | The list of settings shown in the tabs, and where the GitHub releases come from. Edit it to add new NBT-AAD settings; no rebuild needed. |
| `logs\send-….log`, `connect-…` (with the car search when it was needed), `releases-…` | One file per run, newest 50 of each kept. The password is masked as `***`. |
| `downloads\<tag>\` | Releases downloaded from GitHub, reused next time (newest 5 kept). |
| `NBT-AA-Installer.ini` | Remembers the host and the last folder. |
| `LICENSE`, `THIRD-PARTY-NOTICES.md` | Licences. |

Run the tool from a folder you can write to.

## Building it yourself

The result is **one `.exe`** with the libraries and the C++ runtime linked in statically; it only needs Windows' own
DLLs. Three libraries are downloaded; everything else comes with Windows (WinHTTP for downloads, CNG for SHA-256 and
libssh2's crypto, IP Helper for Find car).

### What you need

- **Visual Studio 2026** (Community is fine) with the **Desktop development with C++** workload. That brings MSVC
  (toolset **v145**), MSBuild and a Windows SDK.
  - Visual Studio 2022 should also work, but isn't tested: change **Platform Toolset** to **v143** in
    `src\NBT-AA-Installer.vcxproj` and `deps\libssh2-build\libssh2.vcxproj` (wxWidgets' own projects pick the right
    toolset themselves).
- **PowerShell** (built into Windows) for the commands below.
- Optional: **gpg** to check signatures. Git for Windows includes it as `C:\Program Files\Git\usr\bin\gpg.exe`.
- About **2 GB** of free disk space (wxWidgets is large once built).

### What is in this repository, and what you download

| Path | Where it comes from |
|---|---|
| `src\`, `data\`, `NBT-AA-Installer.sln` | This project |
| `deps\build-deps.ps1` | This project: builds wxWidgets and libssh2 |
| `deps\wx_local.props` | This project: the only addition to wxWidgets (static C++ runtime) |
| `deps\libssh2-build\libssh2.vcxproj` | This project: a Visual Studio project for libssh2, which ships only CMake and autotools files |
| `deps\downloads\` | **You download** the archives here (git-ignored) |
| `deps\wxWidgets\`, `deps\libssh2\`, `deps\json\` | **You unpack** or copy the downloads here |
| `deps\lib\`, `deps\wxWidgets\lib\`, `bin\`, `obj\` | Build output (git-ignored) |

**No source file of any library is changed.** wxWidgets gets one extra file (`wx_local.props`, copied in by the build
script); libssh2 is compiled by our own project file with a few defines; nlohmann/json is a single header used as is.

### The libraries

| Library | Version | Download | Size | Check | Licence |
|---|---|---|---|---|---|
| wxWidgets | **3.3.4** | [wxWidgets-3.3.4.zip](https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.4/wxWidgets-3.3.4.zip) | 47,309,846 bytes | SHA-256 `a604fe40a59e6515eac3efe58deca38b54b7ef7f3c1df0906559bde6de1e2f58` (published in the [release notes](https://github.com/wxWidgets/wxWidgets/releases/tag/v3.3.4)) | wxWindows Library Licence 3.1 |
| libssh2 | **1.11.1** | [libssh2-1.11.1.zip](https://github.com/libssh2/libssh2/releases/download/libssh2-1.11.1/libssh2-1.11.1.zip) | 1,354,061 bytes | GPG signature [libssh2-1.11.1.zip.asc](https://github.com/libssh2/libssh2/releases/download/libssh2-1.11.1/libssh2-1.11.1.zip.asc) by Daniel Stenberg, key `27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2`. SHA-256 of the file we used: `1452bed856eb7f82d6e18db481f34dc18c435dd77d56f884725f72fc3757f30a` | BSD 3-Clause |
| nlohmann/json | **3.12.0** | [json.hpp](https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp) and [LICENSE.MIT](https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT) | 953,436 bytes (`json.hpp`) | GPG signature [json.hpp.asc](https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp.asc) by Niels Lohmann (key listed on the [release page](https://github.com/nlohmann/json/releases/tag/v3.12.0)) | MIT |

Release pages: [wxWidgets v3.3.4](https://github.com/wxWidgets/wxWidgets/releases/tag/v3.3.4),
[libssh2 1.11.1](https://github.com/libssh2/libssh2/releases/tag/libssh2-1.11.1),
[nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0). Use exactly these versions: the project
files point at their folder layout and library names.

### Step 1: download

From the repository folder, in PowerShell:

```powershell
New-Item -ItemType Directory -Force deps\downloads | Out-Null
Invoke-WebRequest https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.4/wxWidgets-3.3.4.zip -OutFile deps\downloads\wxWidgets-3.3.4.zip
Invoke-WebRequest https://github.com/libssh2/libssh2/releases/download/libssh2-1.11.1/libssh2-1.11.1.zip -OutFile deps\downloads\libssh2-1.11.1.zip
Invoke-WebRequest https://github.com/libssh2/libssh2/releases/download/libssh2-1.11.1/libssh2-1.11.1.zip.asc -OutFile deps\downloads\libssh2-1.11.1.zip.asc
Invoke-WebRequest https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp -OutFile deps\downloads\json.hpp
Invoke-WebRequest https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp.asc -OutFile deps\downloads\json.hpp.asc
Invoke-WebRequest https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT -OutFile deps\downloads\LICENSE.MIT
```

Or download the same files with a browser into `deps\downloads\`.

### Step 2: check the downloads

```powershell
Get-FileHash deps\downloads\*.zip -Algorithm SHA256
```

Compare with the SHA-256 values in the table above. `json.hpp` 3.12.0 has SHA-256
`aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`, and its signature is made with key
`797167AE41C0A6D9232E48457F3CEA63AE251B69`. To check the signatures as well, fetch the signers' keys once, then verify
(with Git for Windows' gpg):

```powershell
& "C:\Program Files\Git\usr\bin\gpg.exe" --keyserver hkps://keyserver.ubuntu.com --recv-keys 27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2
& "C:\Program Files\Git\usr\bin\gpg.exe" --verify deps\downloads\libssh2-1.11.1.zip.asc deps\downloads\libssh2-1.11.1.zip
& "C:\Program Files\Git\usr\bin\gpg.exe" --verify deps\downloads\json.hpp.asc deps\downloads\json.hpp
```

Each must say `Good signature`. For `json.hpp`, fetch key `797167AE41C0A6D9232E48457F3CEA63AE251B69` the same way with
`--recv-keys`. A warning that a key isn't certified with a trusted signature is normal for a key you just fetched. If
gpg complains about `keyboxd`, it has no keyring yet: run `gpg --list-keys` once to create one.

### Step 3: unpack

The wxWidgets zip has **no top folder**, so it goes straight into `deps\wxWidgets`. The libssh2 zip has a
`libssh2-1.11.1` top folder, which is renamed to `deps\libssh2`. The two nlohmann/json files are copied into
`deps\json`:

```powershell
Expand-Archive deps\downloads\wxWidgets-3.3.4.zip -DestinationPath deps\wxWidgets
Expand-Archive deps\downloads\libssh2-1.11.1.zip -DestinationPath deps
Rename-Item deps\libssh2-1.11.1 libssh2
New-Item -ItemType Directory -Force deps\json\nlohmann | Out-Null
Copy-Item deps\downloads\json.hpp deps\json\nlohmann\json.hpp
Copy-Item deps\downloads\LICENSE.MIT deps\json\LICENSE.MIT
```

Afterwards these files must exist: `deps\wxWidgets\build\msw\wx_vc17.sln`, `deps\libssh2\src\session.c` and
`deps\json\nlohmann\json.hpp`.

### Step 4: build the libraries (once)

```powershell
.\deps\build-deps.ps1
```

It finds MSBuild with `vswhere`, checks the sources are in place, copies `deps\wx_local.props` into
`deps\wxWidgets\build\msw\`, then builds libssh2 and wxWidgets for x64 **Debug** and **Release**. The first wxWidgets
build takes several minutes. Pass `-Configurations Release` to build only one configuration. If PowerShell refuses to
run scripts, use `powershell -ExecutionPolicy Bypass -File .\deps\build-deps.ps1`. nlohmann/json is header-only and
needs no build.

What it builds, and why each setting is there:

- **libssh2** (`deps\libssh2-build\libssh2.vcxproj`, output `deps\lib\x64\<Configuration>\libssh2.lib`): a static
  library from libssh2's 26 `src\*.c` files, with the static C++ runtime (`/MT`, `/MTd`). Defines:
  - `LIBSSH2_WINCNG`: use Windows' own crypto (CNG), so no OpenSSL is needed.
  - `LIBSSH2_DSA_ENABLE`: libssh2 1.11 turns `ssh-dss` host keys off by default; the car's old OpenSSH 5.2 server may
    need them.
  - `LIBSSH2_LIBRARY`: building the library itself, not using it.
- **wxWidgets** (its own `build\msw\wx_vc17.sln`, output `deps\wxWidgets\lib\vc_x64_lib\`), two targets:
  - **`core`**: the windows and controls. It also builds `base` (which includes the zip reader used for
    `Releases.zip`) and the bundled third-party libraries: zlib, libpng and PCRE2, which the app links, and libjpeg,
    libtiff, libwebp and expat, which it doesn't.
  - **`stc`**: the text editor control used by the File tab, with the Scintilla and Lexilla libraries it is built on.

  `wx_local.props` sets `wxRuntimeLibs=static`, so wxWidgets uses the static C++ runtime like the rest of the app.
  wxWidgets' projects import that file automatically when it exists.

To build them by hand in Visual Studio instead: copy `deps\wx_local.props` to `deps\wxWidgets\build\msw\` first, then
build projects **core** and **stc** of `deps\wxWidgets\build\msw\wx_vc17.sln` and `deps\libssh2-build\libssh2.vcxproj`,
each for x64 Debug and Release.

### Step 5: build the app

Open `NBT-AA-Installer.sln` in Visual Studio, pick **Release** / **x64**, and build. Or from a
**Developer PowerShell for VS**:

```powershell
msbuild NBT-AA-Installer.sln /m /p:Configuration=Release /p:Platform=x64
```

The result is `bin\x64\Release\NBT-AA-Installer.exe` (about 8 MB), with `data\settings.json`, `LICENSE` and
`THIRD-PARTY-NOTICES.md` copied next to it. Copy the `.exe`, `data\` and the two licence files anywhere to use it.

The solution also builds `NBT-AA-Installer.Tests.exe`, a console program that checks the `nbt-aad.env` rules (15
settings, 159 characters, `#~off`, duplicates, where new lines go), the POSIX `cksum` and SHA-256 code, and that
`data\settings.json` loads. Run it from the repository folder; it prints `N checks, 0 failed`.

Project settings worth knowing, all in `src\NBT-AA-Installer.vcxproj`:

- C++20, warning level 4, SDL checks. Library headers are included with `<…>` and treated as external, so their
  warnings are hidden; `_CRT_SECURE_NO_WARNINGS` is defined for wxWidgets' headers.
- **wxWidgets links itself**: its `setup.h` adds `#pragma comment(lib, …)` for its libraries. The app defines
  `wxNO_*_LIB` for every part it doesn't use (net, XML, JPEG, TIFF, WebP, HTML, …), so only base, core, stc, Scintilla,
  Lexilla, libpng, zlib and the regex library end up in the `.exe`.
- Other libraries linked: `libssh2.lib`, and from Windows `ws2_32.lib` (sockets), `bcrypt.lib` and `crypt32.lib`
  (crypto and SHA-256), `iphlpapi.lib` (Find car), `winhttp.lib` (GitHub downloads).
- Static C++ runtime (`/MT`, `/MTd`), matching the libraries.
- `App.rc` includes wxWidgets' `wx/msw/wx.rc` with `wxUSE_RC_MANIFEST=1` and `wxUSE_DPI_AWARE_MANIFEST=2`: modern
  common controls and per-monitor DPI awareness. The linker's own manifest is turned off so there's only one.
- Debug builds put the debug info inside each `.obj` (`/Z7`), which avoids a shared `.pdb` being locked by a stuck
  compiler process.

### If something goes wrong

| Problem | Cause and fix |
|---|---|
| `cannot open include file 'wx/wx.h'`, `'libssh2.h'` or `'nlohmann/json.hpp'` | The sources aren't in `deps\wxWidgets`, `deps\libssh2` or `deps\json`. Redo step 3. |
| `cannot open file 'wxmsw33u_core.lib'` (or `_stc`) or `'libssh2.lib'` | The libraries aren't built for that configuration. Redo step 4. |
| `mismatch detected for 'RuntimeLibrary'` (LNK2038) | wxWidgets was built without `wx_local.props`. Delete `deps\wxWidgets\lib` and `deps\wxWidgets\build\msw\vc_*`, then run step 4 again. |
| `LNK1104: cannot open file '…NBT-AA-Installer.exe'` | The app is still running. Close it and build again. |
| `MSB8020: The build tools for v145 cannot be found` | You have an older Visual Studio. See "What you need" for the toolset change. |
| Connect's car search sends the broadcast but nothing answers | Windows Firewall blocked the reply: allow the app on private networks. |
| The GitHub list stays empty | No internet, or GitHub's limit of 60 requests per hour without login was reached. Wait and press Refresh. The `releases-…` log says which. |

## Source layout

```
src\App               wxApp entry point: starts winsock and libssh2, loads data\settings.json
src\MainFrame         the window: connection row, the two install sections, the tabs, the action buttons, the log
src\SourcePanels      GitHubPanel (the last 20 releases) and LocalPanel (folder)
src\SettingsBook      the settings tabs, the iDrive overrides lock and the File tab, all views of one EnvFile
src\SettingControl    one setting's row: a list or a field, the ⚠ marker, the description
src\SettingsCatalog   reads data\settings.json; checks a value the way the car would
src\EnvFile           nbt-aad.env as the car reads it: 15 settings, 159 characters, #~off, findings, diff
src\PackageSource     where the 6 files come from: LocalSource (folder / zip) and GitHubSource (download, check, cache)
src\PackageFiles      the 6 files in memory, SHA-256, POSIX cksum, the NBT-AAD-VERSION tag
src\GitHubReleases    the release list and the [stable] rule
src\HttpClient        HTTPS GET through WinHTTP
src\ZipReader         reading zips with wxWidgets' zip stream
src\Deployer          Connect, Send to car, Write .env only, Roll back, Pull car logs: every step logged
src\CarShell          runs scripts on hu-omap (through `on -f` when SSH lands on hu-jacinto), uploads with cat >
src\CarState.h        what Connect found on the car
src\SshSession        libssh2: connect with timeout, old algorithms, password / keyboard-interactive login, exec with stdin
src\CarFinder         the car search Connect falls back to: proxy, link-local adapters, HSFZ broadcast, neighbour table, SSH probe
src\LogPanel, RunLog  the coloured log at the bottom, and one log file per run
tests\TestMain.cpp    the checks described above
```

Code style: Allman braces, one statement per line however long, fixed-width integer types; `.clang-format` and
`.editorconfig` hold the details.

## Licence

PolyForm Noncommercial 1.0.0, see [LICENSE](LICENSE). Third-party licences are in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

Setting descriptions, paths, file names and iDrive option labels come from NBT-AAD:
Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto).
NBT-AAD itself is not included; the From GitHub section downloads it from NBT-AAD's official releases page when you
ask. NBT-AAD is free: selling it, or bundling it with anything sold, is not permitted.

## Disclaimer

No warranty. Changing a head unit is at your own risk; keep the `.old` files until the new version works. BMW, MINI,
Toyota and Android Auto are trademarks of their owners; this project is not affiliated with them.
