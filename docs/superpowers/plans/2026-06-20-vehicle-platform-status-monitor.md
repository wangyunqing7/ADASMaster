# Vehicle and Target Platform Status Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the lower-right placeholder with two live telemetry cards for the ego vehicle and target platform, including position, speed, connection state, heartbeat, latency, packet loss, and signal strength.

**Architecture:** Keep motion data in `StateManager` and network data in `DeviceStatus`. `DeviceManager` exposes stable type-based status snapshots to QML, so the UI does not depend on device ordering. A focused QML component renders the two cards and `Main.qml` only places it.

**Tech Stack:** C++17, Qt 6 Core/Quick/QML/Test, QAbstractListModel, QML, CMake/CTest, PowerShell gates.

---

## File map

- Modify `src/device/DeviceStatus.h`: add packet-loss and signal-strength fields.
- Modify `src/device/FakeDevice.cpp`: produce and reset simulated network-quality values.
- Modify `src/device/DeviceListModel.h/.cpp`: publish the new fields as model roles.
- Modify `src/device/DeviceManager.h/.cpp`: expose `drivingRobotStatus` and `targetPlatformStatus` snapshots with change notification.
- Create `tests/TestDeviceTelemetry.cpp`: unit tests for network metrics and type-based snapshots.
- Create `qml/components/VehiclePlatformStatusMonitor.qml`: two-card status monitor.
- Modify `qml/Main.qml`: replace the placeholder with the monitor.
- Create `tests/CheckStatusMonitor.ps1`: static UI integration gate.
- Modify `CMakeLists.txt`: register the new C++ test, QML file, and UI gate.

### Task 1: Add device network-quality metrics

**Files:**
- Create: `tests/TestDeviceTelemetry.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/device/DeviceStatus.h`
- Modify: `src/device/FakeDevice.cpp`
- Modify: `src/device/DeviceListModel.h`
- Modify: `src/device/DeviceListModel.cpp`

- [ ] **Step 1: Write the failing metric test**

Create `tests/TestDeviceTelemetry.cpp` with the first test slot:

```cpp
#include <QtTest>
#include "src/device/FakeDevice.h"

class TestDeviceTelemetry : public QObject
{
    Q_OBJECT

private slots:
    void fakeNetworkMetricsStayInRangeAndResetOnDisconnect()
    {
        FakeDevice device(QStringLiteral("Driving Robot"),
                          QStringLiteral("DrivingRobot"),
                          QStringLiteral("192.168.1.10"), 5000);
        device.connectDevice();
        device.tick(1000);

        DeviceStatus status = device.status();
        QVERIFY(status.packetLossPercent >= 0.0);
        QVERIFY(status.packetLossPercent <= 100.0);
        QVERIFY(status.signalStrengthPercent >= 0);
        QVERIFY(status.signalStrengthPercent <= 100);
        QVERIFY(status.signalStrengthPercent > 0);

        device.disconnectDevice();
        status = device.status();
        QCOMPARE(status.packetLossPercent, 0.0);
        QCOMPARE(status.signalStrengthPercent, 0);
    }
};

QTEST_GUILESS_MAIN(TestDeviceTelemetry)
#include "TestDeviceTelemetry.moc"
```

Register the target in `CMakeLists.txt`:

```cmake
add_adasmaster_test(test_device_telemetry tests/TestDeviceTelemetry.cpp)
```

- [ ] **Step 2: Build the new test and verify RED**

Run:

```powershell
cmake --build build --target test_device_telemetry
```

Expected: compilation fails because `DeviceStatus` has no `packetLossPercent` or `signalStrengthPercent` members.

- [ ] **Step 3: Add the minimal status fields and simulation**

Add to `DeviceStatus`:

```cpp
double packetLossPercent = 0.0;
int signalStrengthPercent = 0;

Q_PROPERTY(double packetLossPercent MEMBER packetLossPercent)
Q_PROPERTY(int signalStrengthPercent MEMBER signalStrengthPercent)
```

In `FakeDevice::connectDevice()` initialize the connected values:

```cpp
m_status.packetLossPercent = 0.0;
m_status.signalStrengthPercent = 90;
```

In `FakeDevice::disconnectDevice()` clear them:

```cpp
m_status.packetLossPercent = 0.0;
m_status.signalStrengthPercent = 0;
```

In `FakeDevice::tick()` update them while connected:

```cpp
m_status.packetLossPercent = QRandomGenerator::global()->bounded(21) / 10.0;
m_status.signalStrengthPercent = 75 + QRandomGenerator::global()->bounded(26);
```

Add `PacketLossRole` and `SignalStrengthRole` after `LastUpdateRole` in `DeviceListModel::Roles`, return the two values from `data()`, and publish role names `devicePacketLoss` and `deviceSignalStrength`.

- [ ] **Step 4: Verify GREEN**

Run:

```powershell
cmake --build build --target test_device_telemetry
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R test_device_telemetry --output-on-failure
```

Expected: `test_device_telemetry` passes.

### Task 2: Expose type-based device snapshots

**Files:**
- Modify: `tests/TestDeviceTelemetry.cpp`
- Modify: `src/device/DeviceManager.h`
- Modify: `src/device/DeviceManager.cpp`

- [ ] **Step 1: Add a failing type lookup test**

Add includes and this test slot to `TestDeviceTelemetry.cpp`:

```cpp
#include "src/device/DeviceManager.h"
#include "src/config/DeviceConfig.h"

void exposesVehicleSnapshotsByTypeInsteadOfListOrder()
{
    DeviceConfig platform;
    platform.name = QStringLiteral("Target Platform #1");
    platform.type = QStringLiteral("TargetPlatform");
    platform.ip = QStringLiteral("192.168.1.11");
    platform.port = 5001;
    platform.protocol = QStringLiteral("udp");
    platform.enabled = true;
    platform.required = true;

    DeviceConfig robot = platform;
    robot.name = QStringLiteral("Driving Robot #1");
    robot.type = QStringLiteral("DrivingRobot");
    robot.ip = QStringLiteral("192.168.1.10");
    robot.port = 5000;

    DeviceManager manager;
    manager.configure({platform, robot});
    manager.connectAll();
    manager.tick(1000);

    const QVariantMap vehicle = manager.drivingRobotStatus();
    const QVariantMap target = manager.targetPlatformStatus();
    QCOMPARE(vehicle.value(QStringLiteral("type")).toString(), QStringLiteral("DrivingRobot"));
    QCOMPARE(target.value(QStringLiteral("type")).toString(), QStringLiteral("TargetPlatform"));
    QVERIFY(vehicle.value(QStringLiteral("connected")).toBool());
    QVERIFY(target.value(QStringLiteral("heartbeatOk")).toBool());
    QVERIFY(vehicle.value(QStringLiteral("signalStrengthPercent")).toInt() > 0);
}
```

- [ ] **Step 2: Build and verify RED**

Run `cmake --build build --target test_device_telemetry`.

Expected: compilation fails because `DeviceManager` does not expose `drivingRobotStatus()` or `targetPlatformStatus()`.

- [ ] **Step 3: Add snapshot properties and conversion**

Add to `DeviceManager.h`:

```cpp
#include <QVariantMap>

Q_PROPERTY(QVariantMap drivingRobotStatus READ drivingRobotStatus NOTIFY deviceStatusChanged)
Q_PROPERTY(QVariantMap targetPlatformStatus READ targetPlatformStatus NOTIFY deviceStatusChanged)

QVariantMap drivingRobotStatus() const;
QVariantMap targetPlatformStatus() const;

signals:
    void deviceStatusChanged();

private:
    QVariantMap statusForType(const QString &type) const;
    static QVariantMap statusToMap(const DeviceStatus &status);
```

Implement in `DeviceManager.cpp`:

```cpp
QVariantMap DeviceManager::drivingRobotStatus() const
{
    return statusForType(QStringLiteral("DrivingRobot"));
}

QVariantMap DeviceManager::targetPlatformStatus() const
{
    return statusForType(QStringLiteral("TargetPlatform"));
}

QVariantMap DeviceManager::statusForType(const QString &type) const
{
    for (const IDevice *device : m_devices) {
        const DeviceStatus status = device->status();
        if (status.type == type)
            return statusToMap(status);
    }
    return {{QStringLiteral("available"), false}};
}

QVariantMap DeviceManager::statusToMap(const DeviceStatus &status)
{
    return {
        {QStringLiteral("available"), true},
        {QStringLiteral("name"), status.name},
        {QStringLiteral("type"), status.type},
        {QStringLiteral("ip"), status.ip},
        {QStringLiteral("port"), status.port},
        {QStringLiteral("connected"), status.connected},
        {QStringLiteral("heartbeatOk"), status.heartbeatOk},
        {QStringLiteral("fault"), status.fault},
        {QStringLiteral("latencyMs"), status.latencyMs},
        {QStringLiteral("packetLossPercent"), status.packetLossPercent},
        {QStringLiteral("signalStrengthPercent"), status.signalStrengthPercent}
    };
}
```

Emit `deviceStatusChanged()` after a device `statusChanged` signal is reflected into the model and once after `configure()` finishes.

- [ ] **Step 4: Verify GREEN and regression safety**

Run:

```powershell
cmake --build build --target test_device_telemetry
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R "test_device_telemetry|test_app_controller" --output-on-failure
```

Expected: both tests pass.

### Task 3: Build and integrate the two-card QML monitor

**Files:**
- Create: `tests/CheckStatusMonitor.ps1`
- Modify: `CMakeLists.txt`
- Create: `qml/components/VehiclePlatformStatusMonitor.qml`
- Modify: `qml/Main.qml`

- [ ] **Step 1: Write the failing UI integration gate**

Create `tests/CheckStatusMonitor.ps1`:

```powershell
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw -Encoding utf8 (Join-Path $workspace 'qml\Main.qml')
$componentPath = Join-Path $workspace 'qml\components\VehiclePlatformStatusMonitor.qml'
$cmake = Get-Content -Raw -Encoding utf8 (Join-Path $workspace 'CMakeLists.txt')

if ($main -notmatch 'VehiclePlatformStatusMonitor\s*\{') {
    throw 'Main.qml does not place VehiclePlatformStatusMonitor.'
}
if ($main -match '独立区块（下 1/4）') {
    throw 'The lower-right placeholder is still present.'
}
if (-not (Test-Path $componentPath)) {
    throw 'VehiclePlatformStatusMonitor.qml is missing.'
}
$component = Get-Content -Raw -Encoding utf8 $componentPath
foreach ($token in @('drivingRobotStatus', 'targetPlatformStatus', 'packetLossPercent',
                      'signalStrengthPercent', 'egoX', 'tgtX')) {
    if ($component -notmatch [regex]::Escape($token)) {
        throw "Status monitor is missing binding: $token"
    }
}
if ($cmake -notmatch 'qml/components/VehiclePlatformStatusMonitor.qml') {
    throw 'Status monitor is not registered in qt_add_qml_module.'
}
Write-Host 'PASS: vehicle/platform status monitor is integrated.'
```

Register it in `CMakeLists.txt`:

```cmake
add_test(NAME status_monitor_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckStatusMonitor.ps1)
set_tests_properties(status_monitor_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 2: Run the gate and verify RED**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\CheckStatusMonitor.ps1
```

Expected: FAIL with `Main.qml does not place VehiclePlatformStatusMonitor.`

- [ ] **Step 3: Create the focused QML component**

Create `qml/components/VehiclePlatformStatusMonitor.qml` with this implementation:

```qml
// =============================================================================
// VehiclePlatformStatusMonitor.qml
// 功能：并列显示主车与目标车平台的运动状态和网络质量。
// 输入：StateManager 位置/速度；DeviceManager 按类型提供的设备状态快照。
// 输出：主界面右侧下方的双卡实时监测区。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

PanelFrame {
    id: root

    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var stm: root.ctrl.stateManager
    readonly property var dm: root.ctrl.deviceManager
    readonly property bool bothOnline: root.dm.drivingRobotStatus.connected === true
                                       && root.dm.targetPlatformStatus.connected === true

    title: "车辆状态监测"
    titleEn: "vehicle / platform telemetry"
    liveText: root.bothOnline ? "双端在线" : "通信异常"
    liveColor: root.bothOnline ? Theme.colorNormal : Theme.colorEmergency

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.spacing

        TelemetryCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            vehicleName: "主车"
            marker: "◆"
            posX: root.stm.egoX
            posY: root.stm.egoY
            speed: root.stm.egoSpeed
            network: root.dm.drivingRobotStatus
        }

        TelemetryCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            vehicleName: "目标车平台"
            marker: "■"
            posX: root.stm.tgtX
            posY: root.stm.tgtY
            speed: root.stm.tgtSpeed
            network: root.dm.targetPlatformStatus
        }
    }

    component TelemetryCard: Rectangle {
        id: card

        property string vehicleName: ""
        property string marker: ""
        property real posX: 0.0
        property real posY: 0.0
        property real speed: 0.0
        property var network: ({})

        function online(): bool {
            return card.network.available === true && card.network.connected === true
        }

        function packetLossLevel(): int {
            if (!card.online() || card.network.heartbeatOk !== true) return 3
            if (card.network.packetLossPercent > 3.0) return 3
            if (card.network.packetLossPercent > 1.0) return 1
            return 0
        }

        function signalLevel(): int {
            if (!card.online() || card.network.heartbeatOk !== true) return 3
            if (card.network.signalStrengthPercent < 40) return 3
            if (card.network.signalStrengthPercent < 70) return 1
            return 0
        }

        color: Theme.colorPanelAlt
        radius: Theme.radius
        border.width: 1
        border.color: card.online() ? Theme.colorAccent : Theme.colorEmergency

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingSm
            spacing: Theme.spacingSm

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                Text {
                    text: card.marker
                    color: Theme.colorAccent
                    font.pixelSize: Theme.fontNormal
                }
                Text {
                    text: card.vehicleName
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                StatusLamp {
                    level: card.online() ? 0 : 3
                    text: card.online() ? "在线" : "离线"
                    dotSize: 8
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                color: Theme.colorBorder

                RowLayout {
                    anchors.fill: parent
                    spacing: 1
                    MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "X 位置"; value: card.posX.toFixed(2); unit: "m" }
                    MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "Y 位置"; value: card.posY.toFixed(2); unit: "m" }
                    MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "速度"; value: card.speed.toFixed(2); unit: "m/s" }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLg

                NetworkMetric {
                    Layout.fillWidth: true
                    label: "心跳"
                    value: !card.online() ? "—" : (card.network.heartbeatOk ? "正常" : "异常")
                    level: card.network.heartbeatOk === true && card.online() ? 0 : 3
                }
                NetworkMetric {
                    Layout.fillWidth: true
                    label: "延迟"
                    value: card.online() ? card.network.latencyMs + " ms" : "—"
                    level: card.online() ? 0 : 3
                }
                NetworkMetric {
                    Layout.fillWidth: true
                    label: "丢包"
                    value: card.online() ? Number(card.network.packetLossPercent).toFixed(1) + "%" : "—"
                    level: card.packetLossLevel()
                }
                NetworkMetric {
                    Layout.fillWidth: true
                    label: "信号"
                    value: card.online() ? card.network.signalStrengthPercent + "%" : "—"
                    level: card.signalLevel()
                }
            }

            Text {
                Layout.fillWidth: true
                text: card.online() ? card.network.ip + ":" + card.network.port : "设备未连接"
                color: Theme.colorTextMuted
                font.family: Theme.fontNumeric
                font.pixelSize: Theme.fontTiny
                elide: Text.ElideRight
            }
        }
    }

    component NetworkMetric: ColumnLayout {
        id: metric
        property string label: ""
        property string value: ""
        property int level: 0
        spacing: 2

        Text {
            text: metric.label
            color: Theme.colorTextMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTiny
        }
        Text {
            text: metric.value
            color: Theme.levelColor(metric.level)
            font.family: Theme.fontNumeric
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }
    }
}
```

Add the QML file to the `QML_FILES` list in `CMakeLists.txt`:

```cmake
qml/components/VehiclePlatformStatusMonitor.qml
```

Replace the lower placeholder `Rectangle` in `Main.qml` with:

```qml
VehiclePlatformStatusMonitor {
    Layout.fillWidth: true
    Layout.preferredHeight: rightContent.height / 4
    Layout.leftMargin: Theme.spacing
    Layout.rightMargin: Theme.spacing
    Layout.bottomMargin: Theme.spacing
}
```

- [ ] **Step 4: Verify GREEN and QML validity**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\CheckStatusMonitor.ps1
cmake --build build --target all_qmllint
```

Expected: the status monitor gate passes and qmllint reports no critical errors.

### Task 4: Full verification and visual inspection

**Files:**
- Verify all modified files.

- [ ] **Step 1: Build all targets**

Run `cmake --build build`.

Expected: build completes with exit code 0.

- [ ] **Step 2: Run the complete test suite**

Run:

```powershell
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build --output-on-failure
```

Expected: all existing tests plus `test_device_telemetry` and `status_monitor_gate` pass.

- [ ] **Step 3: Inspect the running application**

Launch `build\ADASMaster.exe` and verify at 1280×1000:

- The placeholder is gone.
- The two cards have equal width and no clipped labels.
- Main vehicle and target platform position/speed values update during a test.
- Online, heartbeat, latency, packet loss, and signal values refresh.
- Disconnecting either corresponding device changes that card to offline and displays dashes for network values.
- Existing navigation and emergency-stop controls remain accessible.

- [ ] **Step 4: Record the actual verification evidence**

Report the build exit code, exact CTest pass count, and any visual issue still present. This workspace has no `.git` directory, so no commit step is possible; preserve changes directly in the workspace.
