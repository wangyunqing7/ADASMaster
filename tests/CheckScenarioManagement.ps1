$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$pagePath = Join-Path $workspace 'qml\pages\ScenarioPage.qml'
$editorPath = Join-Path $workspace 'qml\components\TrajectorySegmentEditor.qml'
$cmakePath = Join-Path $workspace 'CMakeLists.txt'
$page = Get-Content -Raw -Encoding UTF8 $pagePath
$cmake = Get-Content -Raw -Encoding UTF8 $cmakePath

if ($page -notmatch 'id:\s*createNodeButton') { throw 'Top scenario create button is missing.' }
if ($page -match 'id:\s*btnNewScenario') { throw 'Legacy bottom create button remains.' }
if ($page -notmatch 'createScenarioNode') { throw 'Scenario node API is not used.' }
if ($page -notmatch 'TrajectorySegmentEditor\s*\{') { throw 'Parametric trajectory editor is not used.' }
if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) { throw 'TrajectorySegmentEditor.qml is missing.' }
$editor = Get-Content -Raw -Encoding UTF8 $editorPath
foreach ($token in @('straight','arc','radiusM','turnDirection','rotateDegrees','replaceHeading','saveRequested')) {
    if ($editor -notmatch [regex]::Escape($token)) { throw "Trajectory editor is missing '$token'." }
}
if ($cmake -notmatch 'qml/components/TrajectorySegmentEditor\.qml') {
    throw 'TrajectorySegmentEditor.qml is not registered.'
}

Write-Host 'PASS: scenario hierarchy creation and parametric trajectory editor are present.'
