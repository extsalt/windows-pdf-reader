# build.ps1 - Automated build script for Windows PDF Reader
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$rootDir = $PSScriptRoot

# Locate MSBuild
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath

if (-not $vsPath) {
    Write-Error "Could not locate Visual Studio installation."
}

$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "Could not find MSBuild.exe at $msbuild"
}

Write-Host "==> Building Windows PDF Reader ($Configuration | $Platform)..." -ForegroundColor Cyan
$solution = Join-Path $rootDir "window-pdf-reader.slnx"

& $msbuild $solution /p:Configuration=$Configuration /p:Platform=$Platform

if ($LASTEXITCODE -eq 0) {
    $exePath = Join-Path $rootDir "$Platform\$Configuration\window-pdf-reader.exe"
    Write-Host "==> Build succeeded: $exePath" -ForegroundColor Green
} else {
    Write-Error "Build failed with exit code $LASTEXITCODE"
}
