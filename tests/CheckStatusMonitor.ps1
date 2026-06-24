$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $workspace 'qml\Main.qml'
$componentPath = Join-Path $workspace 'qml\components\VehiclePlatformStatusMonitor.qml'
$cmakePath = Join-Path $workspace 'CMakeLists.txt'

$main = Get-Content -Raw -Encoding UTF8 $mainPath

if ($main -notmatch 'VehiclePlatformStatusMonitor\s*\{') {
    throw 'VehiclePlatformStatusMonitor instance was not found in Main.qml.'
}

if (-not (Test-Path -LiteralPath $componentPath -PathType Leaf)) {
    throw 'VehiclePlatformStatusMonitor.qml does not exist.'
}

$component = Get-Content -Raw -Encoding UTF8 $componentPath
$cmake = Get-Content -Raw -Encoding UTF8 $cmakePath

$requiredBindings = @(
    'deviceListModel',
    'deviceConnected',
    'deviceHeartbeat',
    'deviceIp',
    'devicePort',
    'deviceBattery',
    'deviceLatency',
    'devicePacketLoss',
    'deviceSignalStrength',
    'primaryDeviceCombo',
    'secondaryDeviceCombo',
    'motionRows',
    'deviceMotionCapable',
    'deviceX',
    'deviceY',
    'deviceSpeed',
    'deviceYaw',
    'ComboBox',
    'heartbeatOk',
    'connected'
)

foreach ($binding in $requiredBindings) {
    if ($component -notmatch [regex]::Escape($binding)) {
        throw "VehiclePlatformStatusMonitor.qml is missing '$binding'."
    }
}

# 阈值检查（用简单字符串匹配，避免正则转义问题）
if ($component -notmatch 'packetLossPercent') {
    throw 'Status monitor must reference packetLossPercent.'
}
if ($component -notmatch 'signalStrengthPercent') {
    throw 'Status monitor must reference signalStrengthPercent.'
}
if ($component -notmatch 'heartbeatOk') {
    throw 'Status monitor must reference heartbeatOk.'
}

# 心跳异常状态灯红色
if ($component -notmatch 'heartbeatOk\s*\?\s*0\s*:\s*3') {
    throw 'The top status lamp must turn red when heartbeat is abnormal.'
}

# 离线显示横线
$dash = [string][char]0x2014
if (-not $component.Contains($dash)) {
    throw 'The monitor must display a dash for offline metrics.'
}

# 必须注册到 CMake
if ($cmake -notmatch 'qml/components/VehiclePlatformStatusMonitor\.qml') {
    throw 'VehiclePlatformStatusMonitor.qml is not registered in qt_add_qml_module.'
}

Write-Host 'PASS: device status monitor with selectable device is complete.'
