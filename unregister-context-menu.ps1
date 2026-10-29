# unregister-context-menu.ps1 - Removes Windows Explorer Right-Click Context Menu for Windows PDF Reader
# Requires NO administrator privileges (unregisters from HKEY_CURRENT_USER).

$ErrorActionPreference = "SilentlyContinue"

Write-Host "==> Removing Windows Explorer Context Menu for Windows PDF Reader..." -ForegroundColor Cyan

# 1. Remove from all files (*)
[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("Software\Classes\*\shell\WindowsPdfReader", $false)

# 2. Remove from .pdf SystemFileAssociations
[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("Software\Classes\SystemFileAssociations\.pdf\shell\WindowsPdfReader", $false)

# 3. Remove from Applications
[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree("Software\Classes\Applications\window-pdf-reader.exe", $false)

# 4. Notify Windows Shell of association changes
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotifyUnreg {
    [DllImport("shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern void SHChangeNotify(int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue

try {
    [ShellNotifyUnreg]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero) # SHCNE_ASSOCCHANGED
} catch {
}

Write-Host "==> Context menu successfully removed from Windows Explorer." -ForegroundColor Green
