# Alternative to Install-SubHarm.bat. Right-click -> "Run with PowerShell".
try {
    if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Start-Process powershell.exe "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
        exit
    }
    $src = Join-Path $PSScriptRoot "SubHarm.vst3"
    if (-not (Test-Path $src)) { throw "SubHarm.vst3 not found next to this script. Unzip the whole archive first." }
    $dst = Join-Path $env:CommonProgramFiles "VST3\SubHarm.vst3"
    if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
    New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
    Copy-Item -Recurse $src $dst
    Write-Host "SubHarm installed to $dst" -ForegroundColor Green
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
}
Read-Host "Press Enter to close"
