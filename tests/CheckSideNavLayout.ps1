$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $workspace 'qml\Main.qml'
$sideNavPath = Join-Path $workspace 'qml\components\SideNavBar.qml'

$main = Get-Content -Raw -Encoding UTF8 $mainPath
$sideNav = Get-Content -Raw -Encoding UTF8 $sideNavPath

$instance = [regex]::Match(
    $main,
    'SideNavBar\s*\{(?<body>.*?)(?:onNavClicked|\r?\n\s*\})',
    [System.Text.RegularExpressions.RegexOptions]::Singleline
)

if (-not $instance.Success) {
    throw 'SideNavBar instance was not found in Main.qml.'
}

$body = $instance.Groups['body'].Value

if ($body -notmatch 'Layout\.preferredWidth\s*:\s*176') {
    throw 'SideNavBar must declare Layout.preferredWidth: 176.'
}

if ($body -notmatch 'Layout\.fillHeight\s*:\s*true') {
    throw 'SideNavBar must declare Layout.fillHeight: true.'
}

if ($sideNav -match '(?m)^\s*width\s*:\s*176\s*$') {
    throw 'SideNavBar.qml must not set width while managed by RowLayout.'
}

Write-Host 'PASS: SideNavBar geometry is fully managed by RowLayout constraints.'
