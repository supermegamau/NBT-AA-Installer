# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#
# Builds the static libraries the app links (wxWidgets base + core + stc, libssh2) for x64 Debug and Release.
# nlohmann/json (deps\json\nlohmann\json.hpp) is header-only and needs no build.
# Each library can also be built by hand in Visual Studio:
#   deps\wxWidgets\build\msw\wx_vc17.sln  (projects "core" and "stc"; copy deps\wx_local.props next to it first: static CRT)
#   deps\libssh2-build\libssh2.vcxproj    (output in deps\lib\x64\<Configuration>)
param
(
    [string[]] $Configurations = @('Debug', 'Release')
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild)
{
    throw 'MSBuild not found: install Visual Studio with the C++ desktop workload.'
}
Write-Host "Using $msbuild"

$deps = $PSScriptRoot
foreach ($source in 'wxWidgets\build\msw\wx_vc17.sln', 'libssh2\src\session.c', 'json\nlohmann\json.hpp')
{
    if (-not (Test-Path (Join-Path $deps $source)))
    {
        throw "deps\$source not found: download and unpack the library sources first (see README.md, Building it yourself)."
    }
}

# The only change to wxWidgets: this added file switches its projects to the static C++ runtime.
Copy-Item (Join-Path $deps 'wx_local.props') (Join-Path $deps 'wxWidgets\build\msw\wx_local.props') -Force

foreach ($configuration in $Configurations)
{
    Write-Host "=== libssh2 $configuration"
    & $msbuild (Join-Path $deps 'libssh2-build\libssh2.vcxproj') /m /nologo /v:minimal "/p:Configuration=$configuration;Platform=x64"
    if ($LASTEXITCODE -ne 0) { throw "libssh2 $configuration failed" }

    # "core" pulls in base and the bundled 3rd-party libs; "stc" adds the File tab's editor with Scintilla and Lexilla.
    Write-Host "=== wxWidgets $configuration"
    & $msbuild (Join-Path $deps 'wxWidgets\build\msw\wx_vc17.sln') /m /nologo /v:minimal '/t:core;stc' "/p:Configuration=$configuration;Platform=x64"
    if ($LASTEXITCODE -ne 0) { throw "wxWidgets $configuration failed" }
}
Write-Host 'Done: libssh2 in deps\lib\x64\<Configuration>, wxWidgets in deps\wxWidgets\lib\vc_x64_lib'
