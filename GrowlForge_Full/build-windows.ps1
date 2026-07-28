[CmdletBinding()]
param(
    [switch]$Clean,
    [ValidateSet("vs2026", "vs2022")]
    [string]$VisualStudio = "vs2026"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "GrowlForge requires 64-bit Windows."
}

$preset = if ($VisualStudio -eq "vs2022") {
    "windows-x64-release-vs2022"
} else {
    "windows-x64-release"
}

$buildDir = if ($VisualStudio -eq "vs2022") {
    Join-Path $PSScriptRoot "out\build\windows-x64-release-vs2022"
} else {
    Join-Path $PSScriptRoot "out\build\windows-x64-release"
}

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing stale build directory: $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "Configuring preset: $preset"
cmake --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

Write-Host "Building preset: $preset"
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

$plugin = Join-Path $buildDir "plugins\GrowlForge.clap"
if (-not (Test-Path $plugin)) {
    throw "Build completed but the expected plug-in was not found: $plugin"
}

Write-Host ""
Write-Host "Build succeeded."
Write-Host "Plug-in: $plugin"
Write-Host ""
Write-Host "Install it to:"
Write-Host "$env:LOCALAPPDATA\Programs\Common\CLAP"
