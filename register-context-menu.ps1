# register-context-menu.ps1 - Registers Windows Explorer Right-Click Context Menu for Windows PDF Reader
# Requires NO administrator privileges (registers under HKEY_CURRENT_USER).

$ErrorActionPreference = "Stop"

# Locate the executable
$candidatePaths = @(
    (Join-Path $PSScriptRoot "x64\Release\window-pdf-reader.exe"),
    (Join-Path $PSScriptRoot "window-pdf-reader\x64\Release\window-pdf-reader.exe"),
    (Join-Path $PSScriptRoot "x64\Debug\window-pdf-reader.exe")
)

$exePath = $null
foreach ($path in $candidatePaths) {
    if (Test-Path $path) {
        $exePath = (Resolve-Path $path).Path
        break
    }
}

if (-not $exePath) {
    Write-Error "Could not find window-pdf-reader.exe. Please build the project first using .\build.ps1."
}

Write-Host "==> Registering Windows Explorer Context Menu for: $exePath" -ForegroundColor Cyan

$iconVal = "`"$exePath`",0"
$cmdVal = "`"$exePath`" `"%1`""
$menuTitle = "Open with Windows PDF Reader"

# 1. Register context menu for all files (*)
$allFilesKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("Software\Classes\*\shell\WindowsPdfReader")
$allFilesKey.SetValue("", $menuTitle)
$allFilesKey.SetValue("Icon", $iconVal)
$allFilesCmdKey = $allFilesKey.CreateSubKey("command")
$allFilesCmdKey.SetValue("", $cmdVal)

# 2. Register context menu for .pdf SystemFileAssociations
$pdfKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("Software\Classes\SystemFileAssociations\.pdf\shell\WindowsPdfReader")
$pdfKey.SetValue("", $menuTitle)
$pdfKey.SetValue("Icon", $iconVal)
$pdfCmdKey = $pdfKey.CreateSubKey("command")
$pdfCmdKey.SetValue("", $cmdVal)

# 3. Register under Applications for Windows 'Open With' dialog
$appKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey("Software\Classes\Applications\window-pdf-reader.exe")
$appKey.SetValue("FriendlyAppName", "Windows PDF Reader")
$appIconKey = $appKey.CreateSubKey("DefaultIcon")
$appIconKey.SetValue("", $iconVal)
$appCmdKey = $appKey.CreateSubKey("shell\open\command")
$appCmdKey.SetValue("", $cmdVal)
$appTypesKey = $appKey.CreateSubKey("SupportedTypes")
$appTypesKey.SetValue(".pdf", "")

# 4. Notify Windows Shell of association changes
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotify {
    [DllImport("shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern void SHChangeNotify(int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue

try {
    [ShellNotify]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero) # SHCNE_ASSOCCHANGED
} catch {
    # Ignore if notification cannot be sent
}

Write-Host "==> Context menu successfully registered in Windows Explorer!" -ForegroundColor Green
Write-Host "    - Item: '$menuTitle'"
Write-Host "    - Target: $exePath"
Write-Host "    - Action: You can now right-click any file in Windows Explorer to open it."
