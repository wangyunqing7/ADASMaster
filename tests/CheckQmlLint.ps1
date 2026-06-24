$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$build = Join-Path $workspace 'build'
$output = & cmake --build $build --target all_qmllint 2>&1 | Out-String

$categories = @(
    '[missing-property]',
    '[Quick.layout-positioning]',
    '[property-override]'
)

$failures = @()
foreach ($category in $categories) {
    $count = ([regex]::Matches($output, [regex]::Escape($category))).Count
    if ($count -ne 0) {
        $failures += "$category=$count"
    }
}

if ($failures.Count -gt 0) {
    throw "QML lint gate failed: $($failures -join ', ')"
}

Write-Host 'PASS: critical QML lint categories are clean.'
