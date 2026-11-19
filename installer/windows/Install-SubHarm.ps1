# Installs SubHarm.vst3 into the shared VST3 folder every DAW scans.
# Right-click -> "Run with PowerShell". It asks for admin rights once.
$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell.exe "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}
$src = Join-Path $PSScriptRoot "SubHarm.vst3"
$dst = Join-Path $env:CommonProgramFiles "VST3\SubHarm.vst3"
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
Copy-Item -Recurse $src $dst
Write-Host "SubHarm installed to $dst"
Write-Host "Rescan plugins in your DAW (FL Studio: Options > Manage plugins > Find more plugins)."
Read-Host "Press Enter to close"
