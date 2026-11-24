# NBT-AA-Installer: design and implementation notes

Everything decided so far for NBT-AA-Installer, collected from the planning discussion (2026-11-19).
**Built 2026-11-20 on the owner's go-ahead; not yet run against a car.** Section 16 lists how the build differs from the
plan below.

A separate read-only test app, **NBT-AA-Installer-Check** (its own folder next to this one), was built
first (2026-11-19). It uses the same rules, libraries and log widget, and its job is to settle the open questions in
[section 14](#14-open-questions-to-settle-with-nbt-aa-installer-check) before the installer writes anything to a car.
It was first run against a car on 2026-11-19 ([section 15](#15-nbt-aa-installer-check-built-2026-11-19-first-car-run-2026-11-19)).

### Decisions added after the first version of this document

- **The libraries are built on their own**, outside the app's solution: the solution has only the app project and links
  the `.lib` files ([section 3](#3-tech-stack)).
- **wxWidgets' JPEG, TIFF and WebP are not turned off in its `setup.h`**: the app just never links them.
- **The SSH user and password are hardcoded**: no login fields in the UI ([section 5](#5-facts-about-the-car-from-the-supra-idrive6-android-auto-repo)).
- **No port field**: the car's SSH port is always 22; the only other port is the TurboCode proxy's 6969, used when
  the host is `127.0.0.1` (`CarConnection::Port()` in `CarLogin.h`).
- **Find car**: finds the head unit and fills in the host ([section 6](#6-finding-the-car)). In the installer it is
  **merged into Connect** (owner's decision, 2026-11-20): no separate button.
- **Two install sections** (2026-11-20), each with its own **Send to car** button, sharing the settings tabs and the
  log panel: **From GitHub** lists the last 20 NBT-AAD releases, stable ones marked `[stable]`, and sends the one the
  user selects; **From this PC**
  sends a folder chosen on this PC ([section 7](#7-send-to-car)). This replaces the first version's "never
  downloads releases".
- **Code style**: every call and statement on one line, plus a rule for constructor initializer lists
  ([section 4](#4-code-style-applies-to-every-file)).
- Facts confirmed while building the Check app: libssh2's DSA switch, iDrive's flag file names, winsock connect errors.

---

## 1. Purpose

A Windows desktop tool that:

1. **Sends NBT-AAD to the car over SSH**, from one of two sources, with no USB stick:
   - **From GitHub**: a release picked from the last 20 on the NBT-AAD releases page (stable ones marked `[stable]`).
   - **From this PC**: a folder, typically your own build from the QNX VM.
2. **Edits `nbt-aad.env`** with tabs of checkboxes, dropdowns and fields generated from a settings JSON, plus a raw File
   tab.
3. **Finds the car** on the ENET cable, so the address doesn't have to be typed.

Out of scope:
- **Unlocking SSH.** That is done beforehand with Bavarian SSH or the TurboCode tools.
- **Building NBT-AAD.** "From GitHub" means the prebuilt release files, not a git clone: building needs the QNX
  toolchain in the VM.
- **Shipping NBT-AAD binaries.** The repo and the `.exe` never contain NBT-AAD. The From GitHub section downloads it
  from the official releases page only when the user asks, the same as downloading it by hand. The NBT-AAD README names
  TurboCode as the only authorised bundler and forbids bundling it with anything sold, so the installer never bundles it.

## 2. Project

| Item | Value |
|---|---|
| Name | NBT-AA-Installer |
| Folder | `NBT-AA-Installer`, its own git repo, **never mixed into Supra-iDrive6-Android-Auto** |
| Sharing | Public git repo with source, noncommercial |
| Author name in copyright lines and git | **supermegamau** (not the real name). Set `user.name` / `user.email` in this repo before the first commit. |
| Licence | **PolyForm Noncommercial 1.0.0** (same as NBT-AAD) |
| Version | **0.1.1** (2026-11-24; 0.1.0 on 2026-11-20), in `src/Version.h` only; changed only when the owner asks. Shown in the window title, the first line of every log, the `.exe`'s file properties and the GitHub User-Agent. |

## 3. Tech stack

- **C++**, a classic Visual Studio solution (`.sln` + `.vcxproj`), opened and built from Visual Studio.
  **No CMake unless strictly necessary; no vcpkg.**
- **Visual Studio 2026 Community** (`C:\Program Files\Microsoft Visual Studio\18\Community`, MSVC 14.51, toolset
  v145). VS 2022 (MSVC 14.44) is also installed. Use `.sln` rather than `.slnx` so the solution opens in both.
- **x64 only.** Debug and Release configurations.
- **One self-contained `.exe`**: static libraries plus the static C++ runtime (`/MT`, `/MTd`). The Check app proves it
  works: its Release `.exe` is 5.3 MB and needs only Windows' own DLLs. Dynamic linking would also be acceptable to the
  owner.

### Dependencies

In `deps\`, downloaded from the official GitHub releases and verified. **Each library is built on its own as a static
library; the app's solution only links the `.lib` files** (owner's choice).

| Library | Version | File, size, check | How it builds |
|---|---|---|---|
| wxWidgets | 3.3.4 (stable, 2026-10-10) | `wxWidgets-3.3.4.zip`, 47,309,846 bytes, SHA-256 `a604fe40a59e6515eac3efe58deca38b54b7ef7f3c1df0906559bde6de1e2f58` (from the release notes) | Its own `build\msw\wx_vc17.sln`. Build target **`core`** (pulls in base, zlib, libpng, PCRE2 and the image libs); the installer also needs **`stc`** for the File tab, which also builds scintilla and lexilla. wx 3.3.4 already maps VS 2026 to v145. |
| libssh2 | 1.11.1 (latest) | `libssh2-1.11.1.zip`, 1,354,061 bytes, `.asc` signature by Daniel Stenberg, key `27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2` (checked with Git for Windows' gpg) | Our own `deps\libssh2-build\libssh2.vcxproj` (libssh2 ships only CMake/autotools) listing its 26 `src\*.c` files. Defines `LIBSSH2_WINCNG` (Windows' own crypto, no OpenSSL) and **`LIBSSH2_DSA_ENABLE`** (1.11 turns DSA host keys off by default, see `src/crypto_config.h`). Output `deps\lib\x64\<Configuration>\libssh2.lib`. |
| nlohmann/json | 3.12.0 | `json.hpp` (953,436 bytes, `.asc` signature) + `LICENSE.MIT` from the tag | Header only. Used inside **one** loader class only. Not downloaded yet (the Check app doesn't need it). |

- **`deps\build-deps.ps1`** builds both libraries for Debug and Release (finds MSBuild with vswhere). Each can also be
  built by hand in Visual Studio. The first wxWidgets build takes several minutes.
- **`deps\wx_local.props`** sets `wxRuntimeLibs=static`, `wxReleaseRuntimeLibrary=MultiThreaded` and
  `wxDebugRuntimeLibrary=MultiThreadedDebug`. It is kept in `deps\` (tracked) and `build-deps.ps1` copies it to
  `deps\wxWidgets\build\msw\`, where wxWidgets' projects read it automatically, so a fresh unpack of wxWidgets
  works. It's a new file; no wxWidgets file is edited. The script also refuses to start when a source folder is
  missing.
- **wxWidgets libraries are linked by its own `#pragma comment(lib)`** in `wx/setup.h`. The app defines `wxNO_*_LIB` for
  everything it doesn't use (`NET XML EXPAT JPEG TIFF WEBP HTML QA XRC AUI PROPGRID RIBBON RICHTEXT MEDIA WEBVIEW GL`,
  plus `STC` in the Check app only), so those never reach the `.exe`. JPEG, TIFF and WebP stay enabled in wx's
  `setup.h`, because turning them off would mean editing it.
- **Other libraries** linked by the app: `libssh2.lib;ws2_32.lib;bcrypt.lib;crypt32.lib;iphlpapi.lib;winhttp.lib`
  (`iphlpapi` for Find car, `winhttp` for From GitHub).
- **No new third-party library for From GitHub**: HTTPS through Windows' own **WinHTTP**, SHA-256 through `bcrypt`
  (already linked), zips through wxWidgets' **`wxZipInputStream`** (wxbase, using the zlib already inside wx), JSON
  through nlohmann/json (already planned).
- Open: commit the `deps\` sources (wxWidgets is 1.7 GB once built) or have a script download them and git-ignore them.
  Not decided yet. The Check app's `.gitignore` already leaves out the downloads and all build output.

### App project settings (as in the Check app)

- `/std:c++20`, `/W4`, SDL checks, conformance mode, multi-processor compile.
- External headers at warning level 0 (`TreatAngleIncludeAsExternal` + `ExternalWarningLevel=TurnOffAllWarnings`).
  wxWidgets and libssh2 are included with `<…>`, our own headers with `"…"`.
- Defines: `__WXMSW__;WIN32;_WINDOWS;UNICODE;_UNICODE;NOMINMAX;_CRT_SECURE_NO_WARNINGS` (the last for wx's headers) plus
  the `wxNO_*_LIB` list.
- Include paths: `deps\wxWidgets\include\msvc`, `deps\wxWidgets\include`, `deps\libssh2\include`. Library paths:
  `deps\wxWidgets\lib\vc_x64_lib`, `deps\lib\x64\$(Configuration)`.
- `App.rc` includes `wx/msw/wx.rc` with `wxUSE_RC_MANIFEST=1;wxUSE_DPI_AWARE_MANIFEST=2` (common controls v6 and
  per-monitor DPI). `GenerateManifest=false`, so the linker doesn't add a second manifest.
- Debug uses `/Z7` (debug info inside each `.obj`), so a stuck compiler can't lock a shared `vc145.pdb`.
- Output in `bin\x64\<Configuration>\`, intermediates in `obj\`. `LICENSE` and `THIRD-PARTY-NOTICES.md` are copied
  next to the `.exe` on every build.
- Code that includes `<winsock2.h>` / `<libssh2.h>` / `<iphlpapi.h>` never includes wx headers in the same file, and
  headers never include winsock (`SshSession.h` forward-declares the libssh2 session). That keeps the two from colliding.

### GUI library

**wxWidgets**, chosen over plain Win32 and MFC. The UI is generated from the JSON, so it needs layout sizers, scrolled
panels, high-DPI handling, thread-to-UI calls, `wxStyledTextCtrl` (line markers for the File tab) and possibly
`wxPropertyGrid`. MFC isn't installed in either Visual Studio.

## 4. Code style (applies to every file)

- Object oriented, with simple, short functions.
- **Don't write templates.** Do use the std library types as much as possible (`std::string`, `std::vector`,
  `std::unique_ptr`, `std::optional`, `std::filesystem`, `std::thread`/`std::mutex`, `std::format`). Convert to wx and
  Win32 types only at the API edge.
- **One line per call or statement, however long**: never wrap arguments, conditions or ternaries across lines.
  Data tables and enums get one entry per line. A multi-line lambda body stays an Allman block, but each statement in
  it is one line. `.clang-format` uses `ColumnLimit: 0`, and `.editorconfig` sets no maximum line length.
- **Constructor initializer lists**: with up to 2 entries, the whole declaration is one line:
  ```cpp
  ReadOnlyShell::ReadOnlyShell(SshSession& session, RunLog& log) : mSession(session), mLog(log)
  {
  }
  ```
  With more than 2, one entry per line with a leading comma (`BreakConstructorInitializers: BeforeComma`; clang-format
  can't count entries, so the single-line form is written by hand):
  ```cpp
  Deployer::Deployer(SshSession& session, RunLog& log, const BuildFolder& build)
      : mSession(session)
      , mLog(log)
      , mBuild(build)
  {
  }
  ```
- **Allman** brace style, always. `.clang-format` (`BreakBeforeBraces: Allman`) and `.editorconfig` at the repo root,
  so Visual Studio's Format Document follows it.
- **Avoid single-use variables**: inline them. The exception is a value that must be checked, such as a C API return
  code.
- **Fixed-width types** (`int32_t`, `uint8_t`, `uint16_t`, ...) instead of `int`/`long`, always the smallest that fits:
  `uint8_t` for setting counts and slots, `uint16_t` for line lengths and ports, `uint32_t`/`uint64_t` for sizes and
  offsets. At wx, libssh2 and Win32 boundaries use the API's own type and convert with `static_cast`. Careful:
  `uint8_t` prints as a character in streams and `wxString`, so widen it when printing.
- **No underscores in variable names, never a leading one.** camelCase. Members get an `m` prefix (`mText`, `mSlot`).
  Constants are `constexpr` with a `k` prefix (`kMaxSettings`); no `#define` constants. Classes and functions are
  PascalCase (`EnvFile`, `IsSetting()`). Library names such as `LIBSSH2_ERROR_EAGAIN` and `wxID_OK` are the only
  underscores.
- **Comments are terse**: one line where possible, trailing on the declaration for members, and they explain **why**,
  not what.

File header:

```cpp
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
```

Example:

```cpp
class EnvLine
{
public:
    explicit EnvLine(const std::string& text);

    bool IsSetting() const;

private:
    std::string mText;            // kept verbatim so unknown or invalid lines are never rewritten
    uint8_t     mSlot = 0;        // 1..15 = passed to the daemon, 0 = ignored by the car
};

bool EnvLine::IsSetting() const
{
    // the car skips only a '#' in column 0, so leading spaces still count as a setting
    return !mText.empty() && mText[0] != '#' && mText.find('=') != std::string::npos;
}
```

**Don't copy code from the Supra repo.** It is PolyForm Noncommercial too, but reimplementing the simple rules below from
their documented behaviour keeps the installer's code clearly its own. The licences are compatible either way. Code
from **NBT-AA-Installer-Check** is ours and is meant to be reused.

## 5. Facts about the car (from the Supra-iDrive6-Android-Auto repo)

References are paths in the [Supra-iDrive6-Android-Auto](https://github.com/Sq00ky/Supra-iDrive6-Android-Auto) repo.

### Connection
- `root@169.254.x.x:22` directly, or `127.0.0.1:6969` through the TurboCode proxy (`install.md`). **No port in the
  UI**: the port follows from the host (`127.0.0.1` → 6969, anything else → 22). The login, both ports and
  `CarConnection` live in `CarLogin.h`, shared by both apps.
- **User and password are hardcoded** (owner's decision): `root` and the password from NBT-AAD's install guide, the
  same on every car. The UI has no login fields. The password is still masked as `***` in logs and the log panel
  (`RunLog::AddSecret`).
- **Old SSH**: the docs use `HostKeyAlgorithms=+ssh-dss` (install.md) and `+ssh-rsa` (updating.md), with `-m hmac-sha1`.
  libssh2 1.11.1 with WinCNG supports `ssh-rsa`, `ssh-dss` (with `LIBSSH2_DSA_ENABLE`),
  `diffie-hellman-group1/14-sha1`, `group-exchange-sha1` and `hmac-sha1`. The session offers **every** method the build
  supports (`libssh2_session_supported_algs` then `libssh2_session_method_pref` for key exchange, host key, ciphers and
  MACs).
- **Login**: try `password`, then `keyboard-interactive` (answer every prompt with the password), since old servers
  may offer only one of them.
- **Connect with a timeout**: non-blocking `connect` + `select`, 10 s. On Windows a refused connection is reported in
  `select()`'s **exception** set, not the write set.
- Commands for the main processor run with `on -f /net/hu-omap /bin/sh` (interactive in the docs). The tool needs a
  non-interactive form: `on -f /net/hu-omap <cmd>` or `on -f /net/hu-omap /bin/sh -c "<cmd>"`.
- To write, remount first: `mount -o remount rw /net/hu-omap/fs/sda0`. A reboot puts it back to read-only.

### Install types (`milestone-7/src/nbt_paths.hpp`)

|  | Hand install | NBTApps package install |
|---|---|---|
| app dir (daemon, hand-made flag files) | `/opt/aa` | `/var/opt/nbtapp/apps/com.github.sq00ky.android-auto` |
| data dir (iDrive checkbox flag files) | `/var/opt/aa` | `/var/opt/nbtapp/data/com.github.sq00ky.android-auto` |
| conf dir (`nbt-aad.env`, `pf.bak`) | `/opt/aa` | the data dir |

Detection: an executable `nbt-aad` in the package app dir means a package install. The plugin folder is the same for
both: `/opt/hmi/ID5/data/ro/Plugins/NbtAad/`, with the 5 files `libNbtAad.so`, `NbtAad.manifest`, `NbtAad.xml`,
`NbtAad_ALL_Texts.zip`, `NbtAad_ALL_Images.zip`.

**iDrive checkbox flag files** (in the data dir; one file per ticked box): `autostart`, `usblog`, `menu-idrive`,
`show-drivemode`, `show-warnings`, `kombi-nav`, `kombi-nav-anydist`, `cluster` (holds `j29` / `6wa` / `6wb`), `no-log`,
`speech-bt`, `usb-aa`, `phone-gps`, `wifi-direct`, `native-audio`, `lci-media`, `carplay-compat`. Plus the hand-made
`speech-over-media` in the app dir.

### Other
- **Version strings**: the local `nbt-aad` contains `NBT-AAD-VERSION=v...`. On the car, `grep Build /dev/shmem/nbt-aad.log`
  shows the daemon version and `grep create /dev/shmem/nbt-aad-app.log` the plugin version.
- **Car logs** (RAM, wiped on reboot, at most 5 MB each): `/dev/shmem/nbt-aad.log`, `/dev/shmem/nbt-aad-app.log`,
  `/dev/shmem/nbt-aad-pf_aa.out`.
- **Firewall**: `pf_aa` is built into the daemon (`milestone-7/src/main.cpp:2165`): `nbt-aad --pf-install`,
  `--pf-check`, `--pf-uninstall`. Install adds `NBT_KEEP_PF=1` to the env file. In a package install the `--pf-*`
  commands create the data dir.
- **Setting rows** (owner's choice, 2026-11-20): one line each, the name in bold then the variable in grey parentheses,
  e.g. `Video resolution (NBT_VID_RES)`, then the control. The description shows **only as a tooltip** (on the name, the
  variable and the control), so a tab shows many settings at once.
- **Stopping the daemon**: iDrive Force Stop, `kill <pid>` from `pidin | grep nbt-aad` (updating.md), or `slay nbt-aad`
  (QNX).
- **Restart**: `OnOffDSICommander appreset` (install.md) restarts the HMI and loads the plugin. Holding power for 30 s
  reboots, which also returns the file system to read-only.
- **Golden rule** (updating.md): never copy over a file that is in use. Move it aside to `.old`, copy the new one,
  restart. The old plugin folder goes to `/opt/aa/NbtAad.old`, **outside** `Plugins/`, or iDrive may load it as a second
  app.
- **Uploading without scp**: `ssh … 'cat > /path' < file` (README.md:34). The exec channel is 8-bit clean.
- **Running a command** (`SshSession::Exec`): open a channel, merge stderr into stdout
  (`LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE`, so the order matches what a terminal shows), exec, read until EOF (output
  capped at 4 MB), close, then read the exit status. 30 s timeout per command.

## 6. Finding the car

The address is `169.254.x.x` and differs per car (the guides show `.199.119` and `.199.99`). Users normally learn it
from the TurboCode proxy window (`Forwarding 0.0.0.0:6969 → 169.254.199.119:22`). The installer has the same **Find
car** button as the Check app (class `CarFinder`, copied from there). It fills in the host and **never logs in or
writes anything**:

1. **TurboCode proxy**: connect to `127.0.0.1:6969` and read the greeting. An `SSH-…` line means the proxy is running
   and the car answers through it.
2. **Network adapters** with an automatic `169.254.x.x` address, i.e. the ENET cable is plugged in
   (`GetAdaptersAddresses`).
3. **BMW's HSFZ vehicle identification**: a UDP broadcast to port **6811** with the 6 bytes `00 00 00 00 00 11` (length
   0, control word 0x0011), sent from a socket **bound to each link-local adapter** so it leaves through the ENET cable,
   not Wi-Fi. This is the request ISTA, E-Sys and EdiabasLib send. The car's gateway (ZGW) answers with text such as
   `DIAGADRnnBMWMACxxxxxxxxxxxxBMWVINxxxxxxxxxxxxxxxxx`, and the tool logs the VIN, MAC and diagnostic address. Listen
   2 s per adapter.
4. **Windows' neighbour (ARP) table** (`GetIpNetTable2`): every `169.254.x.x` device already seen, except the broadcast
   address and unreachable or incomplete entries.
5. **SSH knock**: connect to port 22 (1 s timeout) on every address found, plus `169.254.199.119` and `169.254.199.99`
   from the guides, skipping the PC's own addresses, and read the greeting (2 s). A head unit with SSH unlocked answers
   `SSH-…`.

- **A direct address wins over the proxy**, because it works without TurboCode running. The result is saved with the
  other connection settings.
- Runs on the worker thread and can be cancelled between addresses. In the Check app it has its own button and log
  (`logs\find-….log`); in the installer it runs inside Connect and writes to the `connect-…` log.
- Windows Firewall may ask once whether the app may use the network. Allow it on private networks, or the gateway's
  reply to the broadcast may be blocked.
- **Learned on the car** (2026-11-19): only the gateway (ZGW) answers the HSFZ broadcast, and it has no SSH. The head
  unit came from the neighbour table and the known addresses, and answers SSH on both `.199.99` and `.199.119`.
- **In the installer there is no Find car button**: **Connect** tries the address in Host first; when Host is empty,
  or nothing usable answers there, it runs the search above, fills in Host with what it found, and connects to that
  ([section 10](#10-ui-layout)).

## 7. Send to car

Two sections in the window, each with its **own Send to car button**. They differ only in where the files come from
(step 1). Everything after that is the same code (`Deployer`), and both write the **same `nbt-aad.env`** from the
settings tabs, and log to the **same log panel**.

### 7.1 From GitHub

- **Where**: the releases of the NBT-AAD repo, `Sq00ky/Supra-iDrive6-Android-Auto`, through the GitHub REST API:
  `GET https://api.github.com/repos/<repo>/releases?per_page=50`, headers `User-Agent: NBT-AA-Installer` and
  `Accept: application/vnd.github+json`. No login. GitHub allows 60 unauthenticated requests per hour per IP, so the
  list is fetched once at start and on **Refresh**, never in a loop. The repo name is in the settings JSON.
- **Files used from a release**: `nbt-aad` and `Releases.zip` (the 5 plugin files). Ignored: the `.nbtpkg` and
  `nbtpkg-record.json` (the NBTApps package and its record) and `pf_aa` in old releases (the firewall is built into the
  daemon now). A release missing either file is skipped.
- **A list of the last 20 releases** (owner's decision, 2026-11-20, raised from 10 the same day; it replaces "send the latest" and the Stable only
  checkbox). Single selection (`wxListBox`), newest first, the user picks the one to send. Send to car stays disabled
  until a release is selected.
  - **Last 20** = the 20 newest by `published_at`, **not** the API's order: the API sorts by creation date, and an old
    release (`previous-installer`, v1.9.33) sat between November's releases. Drafts and releases missing either file are
    left out before counting.
  - **One line per release**: its name, then **`[stable]`** if it is stable, then the date, then **`[on car]`** when
    its version matches the car's after Connect. For example:
    ```
    v1.9.112 [stable]  2026-11-19  [on car]
    v1.9.111 - Gauge cluster type in the menu [stable]  2026-11-18
    [TESTING] v1.9.76-navsettings / rhmiapp-64-navsettings  2026-11-14
    ```
  - **Preselected**: the newest `[stable]` one, so the common case is one click. The user can pick any other line.
  - **Below the list**: the selected release's tag and a link to its GitHub page (release notes).
- **Which releases are `[stable]`**: GitHub's pre-release flag alone doesn't work: of the 15 newest releases on
  2026-11-20 only one was flagged, while six more were test builds (`[TESTING] …` names, `pr218` / `pr223` /
  `usb-and-settings` tags). A release is marked **`[stable]`** when:
  - it is not a pre-release, **and**
  - its tag starts with a version (`v1.9.112`, `v1.9.111-cluster`), **and**
  - its name has no testing marker (`[TESTING]`).

  The tag pattern and the markers are in the settings JSON, so the rule can change without a rebuild.
- **Download**: each file's `browser_download_url` through WinHTTP (follows the redirect to GitHub's file host). Checked
  against the API's `size`, and against its `digest` (`sha256:…`) when the release has one. Saved in a **`downloads\`
  folder next to the `.exe`**, `downloads\<tag>\`, and reused next time when size and digest still match. Keep the
  newest 5 tags. Git-ignored.
- `Releases.zip` is read with `wxZipInputStream`; the 5 plugin files are found by name at any depth in the zip.
- **Before sending**: an OK / Cancel dialog naming the selected release and the car's version, with an extra line when
  it is the same version, an older one, or not `[stable]`.

### 7.2 From this PC

- A path field with a **Folder…** button. The last folder is remembered in the `.ini`, and the version found in it is
  shown next to the field. **Folder only** (owner's decision, 2026-11-20): the earlier **Zip…** button, which took a
  zip file as the source, was removed.
- **What it accepts**: a folder holding `nbt-aad` and the 5 plugin files, at any depth (up to 3 levels), and also a
  `Releases.zip` with the plugin files inside the folder. So all of these work: your build output folder, a folder laid
  out like the USB stick in the install guide (`nbt-aad` + `Releases\`), or a downloaded `Releases.zip` next to
  `nbt-aad`. A path that isn't a folder is refused with a message.
- Two different copies of the same file name: refuse and list both paths, rather than guess.

### 7.3 Common steps (both buttons)

Every step is logged ([section 11](#11-logging)).

1. **Collect the files** from the section's source (7.1 or 7.2) into memory: `nbt-aad` plus the 5 plugin files (about
   1.3 MB). Log where each came from, the sizes, SHA-256 and `NBT-AAD-VERSION`. Refuse anything missing or empty.
2. **Connect** to the host in the connection row (typed, or found by Connect's car search). Log the host key
   fingerprint and the SSH methods agreed.
3. **Detect the install type and the running version.**
4. **Remount read-write.**
5. **Stage**: upload each file with `cat >` **next to** the current ones (`<app dir>/nbt-aad.staged`,
   `/opt/aa/NbtAad.staged/`), `chmod`, then **check** the size, and the checksum if the car has `cksum`. The working
   version is untouched, and Android Auto keeps running meanwhile.
6. **Stop the daemon** if it is running.
7. **Swap** (one script of renames, same file system): the previous `.old` is deleted, the current files become `.old`,
   the staged ones take their place. If the plugin folder was incomplete (an earlier send stopped half way), the `.old`
   files are kept as they are and only the current files are replaced.
8. **Write `nbt-aad.env`** from the shared settings tabs if it changed ([section 9](#9-nbt-aadenv-rules)). The
   159-character and 15-setting rules apply to both buttons the same way.
9. **Firewall**: `nbt-aad --pf-install`, then `--pf-check`. After the settings file, because `--pf-install` adds
   `NBT_KEEP_PF=1` to it. This is also the new `nbt-aad`'s first run: a failure counts as a failed send.
10. **Restart**: `OnOffDSICommander appreset` when "Restart iDrive" is ticked, or ask the user to reboot.

Also: **roll back** (move the `.old` copies back), **pull the car logs** into the local `logs\` folder, and compare the
local version with the car's.

**The `.old` files of the last version are kept until everything has passed** (owner's rule, 2026-11-20):
- A failure in steps 1 to 6 (download, connection, upload, check, stopping the daemon) changes nothing: the staged files
  are deleted and the car keeps its version.
- A failure in steps 7 to 9 (swap, settings file, firewall) **puts the previous version back automatically**: the `.old`
  files return to their places, `nbt-aad.env` is written back as it was (or removed if it didn't exist), and the car is
  read again. Only a first install, with no previous version, keeps the new files.
- In both cases the car's RAM logs are pulled into `logs\car-send-…\` first. A failed restart (step 10) is only a
  warning: the new version is in place and starts at the next restart.
- **Roll back** stays as a button for going back after a send that succeeded.

## 8. Settings JSON

- **One hand-maintained file**, `data\settings.json`, copied next to the `.exe` and loaded at start. Editing it needs no
  rebuild. **No automatic scanning** of the Supra repo: the owner updates it by hand or with Claude when new `NBT_*`
  settings appear.
- **Generated once** from:
  - `tools/nbtpkg/manifest.json` (10 settings already in JSON; reuse its field names)
  - `milestone-7/readme.md` (the Configuration section; Common table around line 815, the "turn off" features table
    around line 832, Advanced around line 857)
  - the source code, for exact values and ranges (`milestone-7/src/nbt_env.hpp`, `src/audio/config.hpp`, ...)
- Leave out debug-only and identity settings (`NBT_VIDEO_DUMP`, `NBT_MIC_DUMP`, `NBT_DIAG`, `NBT_CAR_MAKE`, ...).
- The descriptions come from the NBT-AAD README, so the file carries its Required Notice ([section 12](#12-licences-and-notices)).

```json
{
  "schemaVersion": 1,
  "notice": "Setting descriptions adapted from NBT-AAD. Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)",
  "maxSettings": 15,
  "maxLineLength": 159,
  "releases": {
    "repo": "Sq00ky/Supra-iDrive6-Android-Auto",
    "daemonAsset": "nbt-aad",
    "pluginAsset": "Releases.zip",
    "stableTag": "^v[0-9]+\\.[0-9]+\\.[0-9]+",
    "testingMarkers": ["[TESTING]"]
  },
  "tabs": [
    {"id": "common",   "label": "Common"},
    {"id": "features", "label": "Features"},
    {"id": "advanced", "label": "Advanced / experimental"},
    {"id": "idrive",   "label": "iDrive overrides", "locked": true,
     "unlockWarning": "These settings override the options in iDrive's Settings page."}
  ],
  "settings": [
    {"key": "NBT_PHONE_GPS", "tab": "idrive", "type": "bool",
     "label": "Use Phone GPS", "description": "...", "unset": "As ticked on iDrive"},
    {"key": "NBT_VIDEO_QUEUE", "tab": "common", "type": "int", "min": 1, "max": 60,
     "label": "Video queue", "description": "...", "unset": "15"},
    {"key": "NBT_MENU_RETURN", "tab": "common", "type": "select", "appOnly": true,
     "values": [{"value": "media", "label": "Media/Radio list"}, {"value": "off", "label": "Stay in our menu"}],
     "unset": "Home"}
  ]
}
```

| Field | Meaning |
|---|---|
| `type` | `bool`, `select`, `int`, `decimal`, `hex`, `text` (with `min`/`max`/`values` where they apply) |
| `unset` | Text for the "Default" choice, which **removes the line**. There's no default-value field: the tool never writes defaults. |
| `appOnly` | Read only by the iDrive app, which reads the whole file. Written at the **end** of the file and **not counted** toward the 15. |
| `advanced` | Hidden until "show advanced" is ticked |
| `secret` | Value masked in logs and diffs (`NBT_WFD_PASS`, `NBT_WFD_OWN_PASS`) |
| `maxSettings`, `maxLineLength` | In the data so a future plugin with other limits needs no rebuild |
| `releases` | The From GitHub source: repo, asset names, and the stable rule (tag regex, name markers) ([section 7.1](#71-from-github)) |

**App-only settings** (read by the iDrive app, not the daemon): `NBT_MENU_RETURN`, `NBT_AA_CAMERA_CTX`,
`NBT_AA_WARN_IGNORE`, `NBT_AA_HOLD`, `NBT_AA_HIDE_ON_KEY`, `NBT_AA_CAR_ASIDE`, `NBT_SETTINGS_RESTART`. This list comes
from a grep of `milestone-7/rhmiapp`, so check it again when generating. `NBT_NO_LOG` and `NBT_RAM_LOG` are read by
**both** and count normally.

**iDrive overrides tab** (a line in the `.env` wins over the iDrive Settings option; the checkboxes themselves are flag
files in the data dir):

| Setting | iDrive option | Flag file |
|---|---|---|
| `NBT_KOMBI_NAV` | Show navigation in HUD/gauge cluster | `kombi-nav` |
| `NBT_KOMBI_NAV_APPROACH` (+ `NBT_KOMBI_NAV_APPROACH_MI`) | Show directions at any distance | `kombi-nav-anydist` |
| `NBT_CLUSTER` | HUD profiles (G Series/Supra J29 / F-series 6WA; the daemon rejects 6WB) | `cluster` (holds the word); **missing from manifest.json** |
| `NBT_KOMBI_MEDIA` | Song title in gauge cluster | `kombi-media` |
| `NBT_HUD_CYRILLIC` | Cyrillic street names in HUD | `hud-cyrillic` |
| `NBT_NO_LOG` | Disable logging (performance test) | `no-log` |
| `NBT_SPEECH_BT` | Prompts on car radio (BT, beta) | `speech-bt` |
| `NBT_SPEECH_OWN_CHANNEL` | Nav prompts mixed into Music (**inverted**: ticked = mixed, i.e. `0`) | `nav-prompt-mix` |
| `NBT_GESTURE_SKIP` | Switch 2 Finger Gesture to Mute (**inverted**: ticked = mute, i.e. `0`) | `gesture-mute` |
| `NBT_USB` | Android Auto over USB | `usb-aa` |
| `NBT_PHONE_GPS` | Use Phone GPS | `phone-gps` |
| `NBT_ALT_WIFI` | Use WiFi Direct Network (own password) | `wifi-direct` |
| `NBT_AUDIO_MODE` (+ `NBT_AUDIO_BT`) | Use Native Audio instead of Bluetooth | `native-audio` |
| `NBT_LCI_MEDIA` | Native Functionality Media Controls Restore (beta) | `lci-media` |
| `NBT_CP_COMPAT` (+ `NBT_CP_YIELD`, `NBT_CP_PURGE`, `NBT_CP_WINGRP`) | CarPlay compatibility | `carplay-compat` |

Updated 2026-11-24 to NBT-AAD v1.9.119-hudprofiles (`origin/main` cd1d9f8).

Checkboxes with no `.env` equivalent, which are not shown: Start automatically, Save logs to USB, MENU key opens iDrive,
Show driving mode changes, Show iDrive warnings. The tool can still read their flag files to show "iDrive: on" next to
the matching setting.

## 9. `nbt-aad.env` rules

### How the car reads it (`milestone-7/rhmiapp/nbt_aad_app.cpp:539-555`)
- The iDrive app passes the **first 15** `NAME=value` lines to the daemon. The array is `envbuf[16][160]` and slot 0 is
  `PATH`. Anything after the 15th is **silently ignored**. This is a buffer chosen by the plugin's author, not a QNX
  limit; a future plugin may raise it.
- Lines are read with `fgets` into 160 bytes, so **at most 159 characters** (not counting the line ending). A longer
  line is split, and its rest comes back as a new line. If the rest contains `=`, it becomes a broken setting that takes
  a slot. **This applies to comments too.**
- Skipped and not counted: empty lines, lines whose **first character** is `#`, lines without `=`.
- `"  # a=b"` (spaces before `#`) **counts** as a setting for the app. `pf_aa` treats it as a comment, so the two
  disagree. `" NBT_X=1"` (spaces before the name) also counts, but the daemon gets a variable named `" NBT_X"`, which it
  doesn't recognise.
- **No comments at the end of a setting's line**: `NBT_X=1  # note` makes the value `1  # note`, which the daemon doesn't
  recognise.
- Duplicate keys: the readers disagree (the menu-return reader takes the last line, the daemon probably sees the
  first). **Never write duplicates.**
- Trailing `\r` and spaces are trimmed, so CRLF works. The tool still writes LF.
- The app-only readers scan the whole file with no 15 limit, matching `strncmp(line, "NAME=", n)` at the start of the
  line.
- Changes apply at the next **Start/Restart** of Android Auto in iDrive. No reboot needed.
- On/off values accepted: `1/on/yes/true`, `0/off/no/false`. Anything else falls back to the default for most settings;
  number settings with a range are clamped.
- `pf_aa --install` adds `NBT_KEEP_PF=1` (required for the firewall rule; it counts toward the 15). Keep it at the top.
- The Check app's `EnvReport` class implements these reading rules and its findings; reuse it as the base for
  `EnvFile`.

### What the tool does
- **Default = line absent.** Every control has a "Default" / "As ticked on iDrive" choice that removes the line. The tool
  never decides something is a default by comparing values, because for some settings no line behaves differently from
  every value you could write (`NBT_VID_FPS`, `NBT_AUDIO_MODE`, `NBT_PROMPT_PAUSE_MEDIA`, all the iDrive overrides).
- **15-limit in the controls**: a counter "N / 15 daemon settings" and a note explaining the limit. At 15, a control
  can't add a 16th setting, but changing or resetting existing ones still works.
- **Disabling without deleting**: the marker `#~off ` in column 0, e.g. `#~off NBT_PHONE_GPS=1`. Only lines with this
  exact marker belong to the tool; the user's own comments are left alone. The commented line must still be ≤159
  characters, or disabling is refused.
- **iDrive overrides tab**: shown but locked. A master checkbox unlocks it only after a **warning popup** saying these
  settings override the iDrive options.
  - Unlocking writes nothing by itself.
  - **Unticking comments out** this tab's lines with `#~off` instead of deleting them; ticking again restores them.
  - Before restoring: recheck the 15-limit, ask which line to keep if an active line for the same key exists (never two),
    and show the warning popup again.
  - If the file already has override lines when it's loaded, the tab opens already unlocked, with a notice.
- **File tab**: the raw file text, editable. **The file text is what counts; the other tabs are a view of it.**
  - A control rewrites only its own line, and only when the user uses that control. Everything else keeps its exact
    text, order and comments.
  - A key not in the JSON stays as typed, counts toward the 15, gets an ⓘ "not in settings list" marker, and changes no
    control.
  - **An invalid value**: the control shows ⚠ and the raw value (a checkbox goes to its in-between state). The tooltip
    says what the car does with it. **The file isn't changed** until the user clicks that control.
  - Marked but **never fixed automatically**: lines past the 15th ("ignored by the car"), lines over 159 characters,
    duplicates, comments at the end of a setting's line, spaces before `#` or before a name, `#~off` lines (greyed), a
    value equal to the default ("could be removed").
  - Override lines typed by hand: no popup while typing. On save, if new override lines appear, the warning popup
    appears once.
  - Unknown lines and comments are always kept.
- **Send-to-car rules for the env file**:
  - **Any line over 159 characters → nothing is sent.** The button only warns, naming the line numbers. This includes
    comments and `#~off` lines.
  - **More than 15 settings → a warning with OK / Cancel** listing the settings that will be ignored. OK sends.
  - Both → the 159 block wins.
  - Before writing, read the car's file again. If it changed since it was loaded (by `pf_aa` or the NBTApps phone app),
    show a diff; otherwise show a short summary of the changes.
- Write the file through a temporary file plus rename (as `pf_aa` does), so the app never reads half a file.

## 10. UI layout

```
+------------------------------------------------------------------------+
| Host [169.254.199.99   ] [Connect]                     car v1.9.111    |
+-----------------------------------+------------------------------------+
| From GitHub             [Refresh] | From this PC                       |
| +-------------------------------+ | [D:\...\AA Versions  ] [Folder...] |
| |v1.9.112 [stable] 11-19 [on car| |                                    |
| |v1.9.111 - Gauge cl… [stable] 1| | v1.9.113                           |
| |v1.9.94 - Wi-Fi Dir… [stable] 1| |                                    |
| |[TESTING] v1.9.76-navsettings …| |                                    |
| +-------------------------------+ |                                    |
| v1.9.112  release notes           |                                    |
|                     [Send to car] |                      [Send to car] |
+-----------------------------------+------------------------------------+
| [Common] [Features] [Advanced] [iDrive overrides] [File]               |
|                                                                        |
|   generated controls (scrolled), ⚠ / ⓘ markers, counter N / 15         |
|                                                                        |
+------------------------------------------------------------------------+
| [Write .env only] [Roll back] [Pull car logs]                          |
+------------------------------------------------------------------------+
| LOG PANEL (full width, bottom)                          [Copy] [Clear] |
+------------------------------------------------------------------------+
```

- **Two install sections side by side** (`wxStaticBoxSizer` each), each with its own **Send to car**. From GitHub
  holds the list of the last 20 releases ([section 7.1](#71-from-github)). Both send the
  settings from the shared tabs below them, and both log to the shared log panel. Side by side keeps the height for
  the tabs.
- **Connection row**: host only (login and port fixed), remembered in `NBT-AA-Installer.ini` next to the `.exe`.
  Empty by default. **Connect** reads the car's install type, versions, `nbt-aad.env` and flag files into the tabs;
  with Host empty, or nothing usable there, it first runs the car search ([section 6](#6-finding-the-car)), which
  also finds the TurboCode proxy (`127.0.0.1`, port 6969).
- **The log panel** (`LogPanel` class): the same widget as in NBT-AA-Installer-Check, copied from there. Full width at
  the bottom; a standard `wxTextCtrl` (read-only RichEdit) inside a `wxPanel`; timestamped lines in **Courier New 9 pt**
  (every line's style carries the font, or bold lines fall back to the UI font); **Ctrl+A selects all** (a read-only
  RichEdit ignores it, so `LogPanel` handles the key); colours for step headers (blue, bold), commands (purple), car
  output (grey), passes (green), warnings (amber) and errors (red, bold); password masked; Copy and Clear buttons (Clear
  empties the panel only, never the log file).
- **While a run is busy**, the connection row and the action buttons are disabled, and the running button's label
  changes ("Finding...", "Downloading...", "Sending..."); the other section's Send to car is disabled too. Closing the window during a run cancels it after the current command.
- **About box**: version, licence, credits, and a **Licences** button showing the embedded third-party notices.
- **Background work**: SSH and discovery run on a worker thread (`std::thread`); progress goes to the UI with
  `CallAfter`. One run at a time.

## 11. Logging

- **One log file per run** in a `logs` folder **next to the `.exe`**: `send-…` (both sections; the first lines say
  which source), `connect-…` (including the car search when it ran), `releases-…` (the GitHub list on start / Refresh), each
  `<prefix>-YYYY-MM-DD_HH-MM-SS.log`. Keep the newest **50** per prefix (a setting). If `logs\` can't be created, refuse
  to run. `logs\` and `*.ini` are in `.gitignore`.
- **Every step** is logged: the exact command, exit code, car output, time taken. The password appears as `***` even
  inside commands.
- **Also logged**:
  - the source: the release tag, name, date, stable or not, each URL, bytes, time and digest check, and whether the
    `downloads\` copy was reused; or the local path and where in it each file was found
  - the local files' sizes, SHA-256 and versions
  - the host key fingerprint and the SSH methods agreed
  - the install type, and the car versions before and after
  - upload bytes, speed and the size/checksum check
  - a line diff of the `.env` (values with `secret` masked)
  - the result, total time and where the `.old` backups are
  - for Find car: the adapters, who answered the broadcast (VIN, MAC), the neighbour table and each port 22 answer
- **On failure**: the failed step, what had been done, the rollback hint, and the car logs pulled into the same folder.
- **Each line is written to disk immediately** (`RunLog`, copied from the Check app, thread-safe).

```
18:42:07.130 [STEP] 1/9 connect to 127.0.0.1:6969 as root
18:42:08.402 host key ssh-dss SHA256:AbC…
18:42:08.615 [STEP] 2/9 remount rw
18:42:08.616   $ mount -o remount rw /net/hu-omap/fs/sda0
18:42:08.790   exit 0 (174 ms)
```

## 12. Licences and notices

```
LICENSE                     PolyForm Noncommercial 1.0.0 text + Required Notice lines (below)
THIRD-PARTY-NOTICES.md      per library: name, version, URL, licence, full licence text (generated from the upstream files)
deps\<lib>\...              upstream licence files kept untouched
data\settings.json          "notice" field with the NBT-AAD Required Notice
```

Required Notice lines at the end of `LICENSE`:

```
Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
```

Add the repo URL to the first line once there is one.

Notices needed for what is actually linked:

| Library | Licence | Upstream file |
|---|---|---|
| wxWidgets | wxWindows Library Licence 3.1 (LGPL + exception: static linking allowed, no source or relink obligation) | `docs\licence.txt` |
| zlib (in wxWidgets) | zlib licence | `src\zlib\LICENSE` |
| libpng (in wxWidgets) | PNG Reference Library License v2 | `src\png\LICENSE` |
| PCRE2 (in wxWidgets, used by wxRegEx) | BSD 3-Clause with PCRE2 exception | `3rdparty\pcre\LICENCE.md` |
| NanoSVG (in wxWidgets) | zlib licence | `3rdparty\nanosvg\LICENSE.txt` |
| Scintilla + Lexilla (in wxWidgets, for `stc`) | Historical permission notice | in `src\stc\` (installer only) |
| libssh2 | BSD 3-Clause | `COPYING` |
| nlohmann/json | MIT | `LICENSE.MIT` (installer only) |
| MSVC runtime | Visual Studio licence; static linking allowed, no notice needed | |

libjpeg, libtiff, libwebp and expat are built by wxWidgets but never linked, so they need no notice. From GitHub adds
no notice: WinHTTP and bcrypt are part of Windows, and zip reading uses the zlib already listed.

- The project copies `LICENSE` and `THIRD-PARTY-NOTICES.md` next to the `.exe` on every build. For the installer, also
  embed the notices as a resource for the About box.
- **README** (written 2026-11-20, `README.md`): what the tool does, how to use it, and a full rebuild guide anyone can
  follow (download commands, checks, unpacking, building the libraries and the app, troubleshooting). It also has:
  - the dependency versions, download links, sizes and checksum / signature
  - credits to NBT-AAD (nanaisu / the Sq00ky repo), saying that NBT-AAD is **not included**
  - links to the SSH unlock tools
  - a no-warranty / own-risk disclaimer
  - a trademark note: BMW, Toyota, MINI and Android Auto are their owners' trademarks; the project isn't affiliated
    with them

## 13. Repository layout

```
NBT-AA-Installer\
  NBT-AA-Installer.sln                 the app project only
  .clang-format  .editorconfig  .gitignore  LICENSE  THIRD-PARTY-NOTICES.md  README.md  DESIGN.md
  deps\
    build-deps.ps1                     builds libssh2 and wxWidgets (core + stc), Debug and Release
    downloads\                         the verified archives (git-ignored)
    wx_local.props                     static C++ runtime for wxWidgets; build-deps.ps1 copies it into wxWidgets\build\msw\
    wxWidgets\                         wxWidgets 3.3.4 source (unpacked, unchanged)
    libssh2\                           libssh2 1.11.1 source
    libssh2-build\libssh2.vcxproj      our project for libssh2
    lib\x64\<Configuration>\           libssh2.lib (git-ignored)
    json\nlohmann\json.hpp             nlohmann/json 3.12.0, included as <nlohmann/json.hpp>
    json\LICENSE.MIT
  src\     NBT-AA-Installer.vcxproj (+ .filters)  *.cpp *.h  App.rc (+ icon, embedded notices)
  data\    settings.json
  tests\   NBT-AA-Installer.Tests.vcxproj  (env parsing, 15/159 rules, #~off, default removal, diff)
  bin\ obj\                            build output (git-ignored)
  (next to the .exe at run time: logs\, downloads\<tag>\, NBT-AA-Installer.ini, all git-ignored)
```

Classes:
- **From the Check app** (copy, then adapt): `LogPanel`, `RunLog`, `SshSession` (and `SshLibrary`, which starts winsock
  and libssh2 once), `CarFinder`, `DirListing`, `EnvReport` (becomes the base of `EnvFile`).
- **New**: `App`, `MainFrame`, `CarCommands` (builds the car commands; unlike the Check app's `ReadOnlyShell`, it also
  writes), `CarProbe` (install type, versions, flag files), `Deployer` (the common send steps, 7.3), `PackageFiles` (the
  6 files in memory, with checksums and version), `PackageSource` (abstract: fills a `PackageFiles`) with
  `GitHubSource` and `LocalSource` (7.1, 7.2), `GitHubReleases` (the last 20 releases and the `[stable]` rule), `HttpClient`
  (WinHTTP, the only file that includes it), `ZipReader` (`wxZipInputStream`), `SourcePanel` (one install section;
  two instances), `SettingsCatalog` (JSON loader, the only nlohmann user), `SettingDef`, `EnvFile`, `EnvLine`,
  `SettingsTab` / `IdriveTab` / `FileTab`, `SettingControl` (one per setting type).

## 14. Open questions to settle with NBT-AA-Installer-Check

Answered by the first car run (2026-11-19):
- SSH: ssh-rsa host key, `diffie-hellman-group-exchange-sha256`, `aes256-ctr`, `hmac-sha1`; libssh2 (WinCNG) logs in
  with the hardcoded login. Server banner `SSH-2.0-OpenSSH_5.2 QNX_Secure_Shell-20090621-hpn13v6`.
- Non-interactive `on -f /net/hu-omap <cmd>` works over an exec channel, and `/net/hu-omap/fs/sda0/opt/aa` is
  reachable directly from hu-jacinto.
- `/opt/aa`, the plugin folder and `/opt` are all on hu-omap's `/fs/sda0` (qnx6, `/dev/sda0t179`, 1.2 GB free);
  `/var/opt` is `/fs/sda1` (qnx6, `/dev/sda0t178`, 416 MB free).
- Find car: the HSFZ broadcast is answered by the **gateway** (ZGW, 169.254.28.99, no SSH), not the head unit; the head
  unit's two addresses (169.254.199.99 and .119) came from the neighbour table and the known-address list. Both
  answer SSH. .99 is hu-jacinto.
- The car has **no `which`**: tools are found by listing the PATH folders.

Still open:
- Which of `cksum`, `md5`, `sha256`, `slay` and `OnOffDSICommander` exist on hu-omap (the Check app now lists the PATH
  folders; needs a second car run).
- Whether 169.254.199.119 is hu-omap itself (then the installer could skip `on -f` and Find car should prefer it).
- Whether `/var/opt/aa` is writable without a remount (the Check app never writes).
- Not decided: commit `deps\` or fetch it with a script.
- From GitHub: is the `[stable]` rule right (not pre-release, tag starts with a version, no `[TESTING]` in the name)?
- From GitHub: whether `Releases.zip` holds the 5 files at its root or in a folder (the tool searches any depth, so
  either works).

## 15. NBT-AA-Installer-Check (built 2026-11-19, first car run 2026-11-19)

- Folder `NBT-AA-Installer-Check`, a separate project (not in this repo), **same rules** (style,
  licence, `deps\` with wxWidgets 3.3.4 + libssh2 1.11.1 built separately, static, `logs\` next to the `.exe`). No JSON
  library.
- UI: Host [Find car], **Check car**, and the **log panel** filling the rest. The host is remembered in
  `NBT-AA-Installer-Check.ini`; the login is hardcoded.
- **Read-only by design**: `ReadOnlyShell` only sends `ls`, `cat`, `grep`, `pidin`, `df`, `uname` and `mount`
  (with no arguments, so it only lists mounts), each with a maximum number of arguments that may contain only letters,
  digits and `._/-:=,+`. Optionally wrapped in `on -f /net/hu-omap`. No `cp`/`mv`/`rm`, no free-form shell. It skips
  `nbt-aad --pf-check`, because on a package install it creates a folder.
- **Check car**, 9 steps:
  1. connect, with host key, fingerprint and SSH methods
  2. `uname -a` directly and through `on -f`, and `ls -ld /net/hu-omap/fs/sda0/opt/aa`
  3. daemon present (hand install / package install), size, executable
  4. the 5 plugin files and their sizes, and any extra files
  5. the env file shown, with the 15/159 checks and the other findings
  6. the flag files (iDrive checkboxes) in the data dir and hand-made ones in the app dir, other files in the data
     dir, and the cluster type
  7. whether the daemon is running (`pidin`, output not logged whole; not running is normal unless `autostart` is
     set, iDrive starts it on Start), and the daemon and plugin versions
  8. `df -k` for `/opt/aa`, the plugin folder and `/var/opt`, and `mount`
  9. `cksum`, `md5`, `sha256`, `slay`, `OnOffDSICommander`, looked up by listing the car's PATH folders (`/proc/boot`,
     `/bin`, `/usr/bin`, `/sbin`, `/usr/sbin`, `/opt/os|sys|core|car/bin`): **the car has no `which`**
- **Find car**: the 5 steps of [section 6](#6-finding-the-car), with its own log.
- **Tested on the car 2026-11-19** (NBT Evo, hand install, laptop on ENET "Ethernet 7"): Find car got the HSFZ reply
  from the gateway (169.254.28.99, no SSH) and found SSH on 169.254.199.99 and .119; Check car on .99 logged in
  (ssh-rsa host key, `diffie-hellman-group-exchange-sha256`, `aes256-ctr`, `hmac-sha1`). .99 is **hu-jacinto**;
  `on -f /net/hu-omap` reaches hu-omap and `/net/hu-omap/fs/sda0/opt/aa` is readable directly. `/opt/aa/nbt-aad.env`
  did not exist. Fixed after that run: `.`/`..` counted as extra plugin files, and `which` is missing on the car.
  Still to learn: whether .119 is hu-omap itself (then the installer could skip `on -f`).
- Tested on this PC: both buttons, the log file and panel, the `.ini`, masking (the password never appears in a
  log), closing during a run, and the refused-connection path.

## 16. As built (2026-11-20)

- **Settings list**: `data\settings.json` had 163 settings at first; since the 2026-11-24 update to NBT-AAD
  v1.9.119-hudprofiles it has 167 (Common 27, Features 24, iDrive overrides 20, Advanced 96). That update removed three
  settings that existed only in uncommitted local work (`NBT_SPEECH_OVER_MEDIA`, `NBT_PROMPT_PAUSE_MEDIA`,
  `NBT_PROMPT_RESUME_MS`), added `NBT_SPEECH_OWN_CHANNEL`, `NBT_GESTURE_SKIP`, `NBT_GESTURE_SNIFF`, `NBT_GESTURE_IF`,
  `NBT_HUD_GLYPHS`, `NBT_KOMBI_MEDIA` and `NBT_HUD_CYRILLIC`, made `NBT_PTT_ASSISTANT` default to on, gave `NBT_MFL_*`
  their head-unit button defaults, and renamed the iDrive rows to match iDrive's current wording. Generate from
  `origin/main` (read with `git show` / `git grep`), never from a working copy with uncommitted changes. App-only: `NBT_MENU_RETURN`, `NBT_AA_HIDE_ON_KEY`,
  `NBT_AA_CAMERA_CTX`, `NBT_AA_WARN_IGNORE`, `NBT_AA_HOLD`, `NBT_AA_CAR_ASIDE`, `NBT_SETTINGS_RESTART`. Secret:
  `NBT_WFD_PASS` (`NBT_WFD_OWN_PASS` turned out to be a mode switch, not a password). `NBT_CLUSTER` offers `j29` and
  `6wa` only: the daemon treats `6wb` as unknown. Left out: debug, identity, test-hook and path/port plumbing settings.
  Generated once by a script from the readme and the source; maintained by hand from now on.
- **Step order changed**: the settings file is written **before** the firewall step, because `nbt-aad --pf-install`
  adds `NBT_KEEP_PF=1` to that same file and writing it afterwards would drop the line. After the firewall step the file
  is read back from the car into the tabs.
- **Commands on hu-omap** go through `on -f /net/hu-omap /bin/sh -c '<script>'` (single-quoted), so redirects such as
  `cat > file` happen on hu-omap. When SSH lands on hu-omap itself (`uname -n`), scripts run directly. The remount runs
  on the processor SSH landed on, as in the install guide.
- **Tools** are found by testing the car's PATH folders with `[ -x ]` (the car has no `which`). Uploads are checked by
  size, plus POSIX `cksum` when the car has it. Restart uses `OnOffDSICommander appreset` when found, otherwise the log
  asks for a 30-second power-button reboot. A **Restart iDrive** checkbox (remembered) decides whether to restart at all.
- **Stopping the daemon**: pids from `pidin` lines naming nbt-aad, `kill`, wait up to 5 s, then `kill -9`.
- **Send needs Connect first**: the tabs must hold the car's file, or sending would wipe the settings on the car.
  Before writing, the car's file is read again; if it changed since Connect, a dialog shows the diff and asks.
- **Version on the car** comes from the `NBT-AAD-VERSION=` tag in the car's own `nbt-aad` (read with `cat`, about 1 MB),
  so it is known even when the daemon hasn't run since boot.
- **First Connect on the car (2026-11-20)**: 169.254.199.119 **is hu-omap**, so commands run there directly; .99 is
  hu-jacinto. The car has `/opt/core/bin/OnOffDSICommander` but **no `cksum`**: uploads are checked by size. Login on
  .119 took about 5 s, so the log now shows the login method and time.
- **One button to reach the car** (owner's decision, 2026-11-20): Find car and Connect were redundant, so Connect does
  both. It tries Host first (fast, and the only way to pick the proxy by hand); if that fails or Host is empty, it runs
  `CarFinder`, puts the address it found in Host, and connects there. Host now starts empty.
- **Version 0.1.0** (owner's request, 2026-11-20): `src/Version.h` holds it as macros (the only `#define` constants in
  the project, because `App.rc`'s VERSIONINFO needs the preprocessor) plus `kAppVersion` for C++.
- **Untagged builds**: the `NBT-AAD-VERSION` tag exists only from v1.9.68 (2026-11-13). For an older `nbt-aad` the tool
  logs its size and SHA-256 and names the GitHub release with the same SHA-256 (or the same size when a release lists
  no digest), e.g. "cluster (recognised by SHA-256)". It fetches 50 releases for this, while the list shows 20.
- **Staged sends with automatic restore** (owner's rule, 2026-11-20): section 7.3 describes it. It replaced the first
  version, which moved the current files to `.old` before uploading and left the car half updated on any failure.
- **v1.9.115 from GitHub and v1.9.116 from a local folder (2026-11-20)** both installed, firewall active, iDrive
  restarted (`rhmiapp-92` afterwards). But `OnOffDSICommander appreset` never returns: the command sat until the 60 s
  SSH timeout and then read a made-up exit 0. Now (1) a command that times out reports exit -1 and "no answer within
  60 s" instead of a fake 0, and (2) the restart runs in the background with its output in
  `/dev/shmem/nbt-aa-installer-restart.out`; after 3 s an `exit N` line other than `exit 0` means it failed (a missing
  library ends it at once), no line means iDrive is restarting.
- **Third Send (2026-11-20) succeeded**: v1.9.111-cluster installed, v1.9.58 kept as `.old`, `NBT_KEEP_PF=1` added by
  pf_aa. Only the restart failed: `OnOffDSICommander` could not load `libsys_dsi_servicebroker.cpp-ne.so.4`, because an
  exec channel has no login environment. Restart now sets `LD_LIBRARY_PATH` to the car's library folders (as NBT-AAD's
  `mmr_launch.cpp` does), then tries with `/etc/profile` sourced, and on failure logs where the library lives.
- **Second Send (2026-11-20)**: staging, the swap and the automatic restore all worked on the car. It failed only
  because `nbt-aad --pf-check` returned 9, which pf_aa uses for "pf.conf is right, the running filter loads it at the
  next reboot", the normal state right after `--pf-install`. Exit 0 and 9 now pass; 7 (rules, pf.conf syntax or
  `NBT_KEEP_PF` wrong) and anything else fail.
- **First Send (2026-11-20) stopped after a good upload**: the size check looked for `nbt-aad` in `ls -l /opt/aa/nbt-aad`,
  which prints the full path. Fixed (and tested). The same run showed a worse risk: the next Send would have replaced
  the `.old` backups with the half-finished state. Now Connect records whether the plugin folder holds all 5 files;
  when it doesn't and `.old` files exist, Send keeps the `.old` files and only replaces the current ones.
- **Failed commands always log their output**, even when the output of that command is normally hidden (it's the error).
- **Sources are compiled with `/utf-8`**: the UI strings use symbols (⚠ ⓘ ⛔ ⏸ ✔ ·), and without the switch MSVC reads
  BOM-less files as Windows-1252.
- **WinHTTP uses the default (registry) proxy setting**, not automatic proxy discovery: discovery added about 10 s to
  every request on the owner's PC.
- **Classes**: `GitHubPanel` and `LocalPanel` (two classes, not one `SourcePanel`), `SettingsBook` holds the tabs and
  the File tab (no separate `SettingsTab` / `IdriveTab` / `FileTab` classes), `CarShell` replaces `CarCommands`, and
  `Deployer` also does what `CarProbe` was meant to (Connect). `PackageSource` is the abstract base of `LocalSource`
  and `GitHubSource`.
- **Tests** (`tests\NBT-AA-Installer.Tests.vcxproj`, 45 checks): env parsing, the 15 and 159 rules, where new lines go,
  `#~off`, duplicates, the diff with secrets masked, value checks, POSIX cksum, SHA-256, the version tag.
- **Tested without a car**: the GitHub list (10 releases, `[stable]` marks match section 7.1), downloading v1.9.112 with
  size and SHA-256 checked, a nested `Releases.zip` inside a zip (the zip source was removed later), the File tab markers, the ⚠ on an invalid value, and
  sending stopping cleanly at the connect step. **Not yet tested**: everything that talks to the car.

