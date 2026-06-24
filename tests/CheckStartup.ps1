$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $workspace 'build\ADASMaster.exe'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ADASMaster-smoke-" + [guid]::NewGuid())

try {
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    Copy-Item (Join-Path $workspace 'config') $tempRoot -Recurse
    Copy-Item (Join-Path $workspace 'scenarios') $tempRoot -Recurse
    $env:ADASMASTER_DATA_DIR = $tempRoot
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue

    $process = Start-Process -FilePath $executable -ArgumentList '--smoke-test' -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(5000)) {
        $process.Kill()
        throw 'ADASMaster did not finish its smoke-test startup within 5 seconds.'
    }
    if ($process.ExitCode -ne 0) {
        throw "ADASMaster smoke-test exited with code $($process.ExitCode)."
    }
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'PASS: packaged application loaded QML and completed startup.'
