# 功能：实际 QML 按钮/曲线/CSV 冒烟验证，使用独立配置和透明测试窗口。
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$build = Join-Path $workspace 'build'
$executable = Join-Path $build 'ADASMaster.exe'
$contextRoot = Join-Path $build 'mpc-ui-smoke-data'
New-Item -ItemType Directory -Path $contextRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $workspace 'config') -Destination $contextRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $workspace 'scenarios') -Destination $contextRoot -Recurse -Force

$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt')
$qtLine = $cache | Select-String '^Qt6_DIR:PATH=(.+)$' | Select-Object -First 1
if ($null -eq $qtLine) { throw 'Qt installation was not found in CMakeCache.txt.' }
$qtRoot = [System.IO.Path]::GetFullPath((Join-Path $qtLine.Matches[0].Groups[1].Value '../../..'))
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QUICK_BACKEND = 'software'
$env:QT_PLUGIN_PATH = Join-Path $qtRoot 'plugins'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $qtRoot 'plugins/platforms'
$env:ADASMASTER_DATA_DIR = $contextRoot
$imagePath = Join-Path $build 'mpc-ui-preview.png'
$process = Start-Process -FilePath $executable -WorkingDirectory $workspace -ArgumentList @('--mpc-ui-smoke', ('"' + $imagePath + '"')) -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(15000)) {
    $process.Kill()
    throw 'MPC UI smoke test timed out.'
}
if ($process.ExitCode -ne 0) { throw "MPC UI smoke test exited with code $($process.ExitCode)." }
$report = Get-Content -LiteralPath (Join-Path $build 'mpc-ui-smoke.json') -Raw | ConvertFrom-Json
if (-not $report.passed -or $report.samples -ne 160) { throw "MPC UI validation failed: $($report.error)" }
if (-not (Test-Path -LiteralPath $imagePath)) { throw 'MPC screenshot was not generated.' }
Write-Host 'PASS: MPC QML start button, live curves, completed state and CSV export.'
