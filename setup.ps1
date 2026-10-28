# setup.ps1 - Automated dependency setup for Windows PDF Reader
param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$rootDir = $PSScriptRoot

Write-Host "==> Checking build dependencies..." -ForegroundColor Cyan

# 1. Download nuget.exe if not present
$nugetPath = Join-Path $rootDir "nuget.exe"
if (-not (Test-Path $nugetPath) -or $Force) {
    Write-Host "--> Downloading nuget.exe..." -ForegroundColor Yellow
    curl.exe -s -L -o $nugetPath https://dist.nuget.org/win-x86-commandline/latest/nuget.exe
}

# 2. Restore NuGet packages (PDFium & Windows SDK)
Write-Host "--> Installing PDFium and Windows SDK packages..." -ForegroundColor Yellow
& $nugetPath install bblanchon.PDFium.Win32 -Version 158.0.8125 -OutputDirectory (Join-Path $rootDir "packages")
& $nugetPath install Microsoft.Windows.SDK.CPP -Version 10.0.26100.2454 -OutputDirectory (Join-Path $rootDir "packages")
& $nugetPath install Microsoft.Windows.SDK.CPP.x64 -Version 10.0.26100.2454 -OutputDirectory (Join-Path $rootDir "packages")

# 3. Setup wxWidgets 3.2.6
$wxDir = Join-Path $rootDir "vendor\wxWidgets"
if (-not (Test-Path $wxDir) -or $Force) {
    Write-Host "--> Downloading and extracting wxWidgets 3.2.6 (x64)..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $wxDir | Out-Null
    
    $headersArchive = Join-Path $rootDir "vendor\headers.7z"
    $devArchive = Join-Path $rootDir "vendor\dev.7z"
    $dllArchive = Join-Path $rootDir "vendor\dll.7z"

    curl.exe -s -L -o $headersArchive https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxWidgets-3.2.6-headers.7z
    tar.exe -xf $headersArchive -C $wxDir
    Remove-Item $headersArchive -Force

    curl.exe -s -L -o $devArchive https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxMSW-3.2.6_vc14x_x64_Dev.7z
    tar.exe -xf $devArchive -C $wxDir
    Remove-Item $devArchive -Force

    curl.exe -s -L -o $dllArchive https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxMSW-3.2.6_vc14x_x64_ReleaseDLL.7z
    tar.exe -xf $dllArchive -C $wxDir
    Remove-Item $dllArchive -Force
}

Write-Host "==> Setup complete! You can now build with .\build.ps1 or open window-pdf-reader.slnx in Visual Studio." -ForegroundColor Green
