// =============================================================================
// TestDeviceTelemetry.cpp
// 功能：验证模拟设备网络质量指标及其列表模型角色映射。
// 输入：DrivingRobot 类型的 FakeDevice、连接状态变化和 DeviceListModel。
// 输出：通过 QtTest 断言指标范围、复位行为、角色名称和模型数据。
// =============================================================================

#include <QtTest>

#include "src/device/DeviceManager.h"
#include "src/device/DeviceListModel.h"
#include "src/device/FakeDevice.h"

class TestDeviceTelemetry : public QObject
{
    Q_OBJECT

private slots:
    void fakeNetworkMetricsStayInRangeAndResetOnDisconnect()
    {
        FakeDevice device(QStringLiteral("Driving Robot"),
                          QStringLiteral("DrivingRobot"),
                          QStringLiteral("127.0.0.1"), 9000);

        device.connectDevice();

        const DeviceStatus initialStatus = device.status();
        QCOMPARE(initialStatus.packetLossPercent, 0.0);
        QCOMPARE(initialStatus.signalStrengthPercent, 90);

        for (int sample = 0; sample < 20; ++sample) {
            device.tick(1000 + sample);

            const DeviceStatus connectedStatus = device.status();
            QVERIFY(connectedStatus.packetLossPercent >= 0.0);
            QVERIFY(connectedStatus.packetLossPercent <= 100.0);
            QVERIFY(connectedStatus.signalStrengthPercent >= 0);
            QVERIFY(connectedStatus.signalStrengthPercent <= 100);
            QVERIFY(connectedStatus.signalStrengthPercent > 0);
        }

        device.disconnectDevice();

        const DeviceStatus disconnectedStatus = device.status();
        QCOMPARE(disconnectedStatus.packetLossPercent, 0.0);
        QCOMPARE(disconnectedStatus.signalStrengthPercent, 0);
    }

    void modelExposesNetworkMetricRolesAndValues()
    {
        FakeDevice device(QStringLiteral("Driving Robot"),
                          QStringLiteral("DrivingRobot"),
                          QStringLiteral("127.0.0.1"), 9000);
        device.connectDevice();
        device.tick(1000);

        DeviceListModel model;
        model.setDevices({&device});

        const QHash<int, QByteArray> roles = model.roleNames();
        QCOMPARE(roles.value(DeviceListModel::PacketLossRole),
                 QByteArray("devicePacketLoss"));
        QCOMPARE(roles.value(DeviceListModel::SignalStrengthRole),
                 QByteArray("deviceSignalStrength"));

        const QModelIndex deviceIndex = model.index(0, 0);
        QVERIFY(deviceIndex.isValid());

        const DeviceStatus status = device.status();
        QCOMPARE(model.data(deviceIndex, DeviceListModel::PacketLossRole).toDouble(),
                 status.packetLossPercent);
        QCOMPARE(model.data(deviceIndex, DeviceListModel::SignalStrengthRole).toInt(),
                 status.signalStrengthPercent);
    }

    void modelExposesMotionTelemetryRolesAndValues()
    {
        FakeDevice device(QStringLiteral("Driving Robot"),
                          QStringLiteral("DrivingRobot"),
                          QStringLiteral("127.0.0.1"), 9000, true);
        device.connectDevice();
        device.arm();
        device.start();
        device.tick(1000);
        device.tick(2000);

        DeviceListModel model;
        model.setDevices({&device});
        const auto roles = model.roleNames();
        QCOMPARE(roles.value(DeviceListModel::MotionCapableRole), QByteArray("deviceMotionCapable"));
        QCOMPARE(roles.value(DeviceListModel::PositionXRole), QByteArray("deviceX"));
        QCOMPARE(roles.value(DeviceListModel::PositionYRole), QByteArray("deviceY"));
        QCOMPARE(roles.value(DeviceListModel::SpeedRole), QByteArray("deviceSpeed"));
        QCOMPARE(roles.value(DeviceListModel::YawRole), QByteArray("deviceYaw"));

        const QModelIndex index = model.index(0, 0);
        QVERIFY(model.data(index, DeviceListModel::MotionCapableRole).toBool());
        QVERIFY(model.data(index, DeviceListModel::PositionXRole).toDouble() > 0.0);
        QVERIFY(model.data(index, DeviceListModel::SpeedRole).toDouble() > 0.0);
        const double yaw = model.data(index, DeviceListModel::YawRole).toDouble();
        QVERIFY(yaw >= 0.0 && yaw < 360.0);
    }

    void exposesVehicleSnapshotsByTypeInsteadOfListOrder()
    {
        DeviceConfig platform;
        platform.name = QStringLiteral("Target Platform");
        platform.type = QStringLiteral("TargetPlatform");
        platform.ip = QStringLiteral("127.0.0.2");
        platform.port = 9001;

        DeviceConfig robot;
        robot.name = QStringLiteral("Driving Robot");
        robot.type = QStringLiteral("DrivingRobot");
        robot.ip = QStringLiteral("127.0.0.1");
        robot.port = 9000;

        DeviceManager manager;
        manager.configure({platform, robot});
        manager.connectAll();
        manager.tick(1000);

        const QVariantMap robotStatus = manager.drivingRobotStatus();
        const QVariantMap platformStatus = manager.targetPlatformStatus();

        QCOMPARE(robotStatus.value(QStringLiteral("type")).toString(),
                 QStringLiteral("DrivingRobot"));
        QVERIFY(robotStatus.value(QStringLiteral("connected")).toBool());
        QVERIFY(robotStatus.value(QStringLiteral("heartbeatOk")).toBool());
        QVERIFY(robotStatus.value(QStringLiteral("signalStrengthPercent")).toInt() > 0);

        QCOMPARE(platformStatus.value(QStringLiteral("type")).toString(),
                 QStringLiteral("TargetPlatform"));
        QVERIFY(platformStatus.value(QStringLiteral("connected")).toBool());
        QVERIFY(platformStatus.value(QStringLiteral("heartbeatOk")).toBool());
        QVERIFY(platformStatus.value(QStringLiteral("signalStrengthPercent")).toInt() > 0);
    }

    void emitsDeviceStatusChangedAtSnapshotBoundaries()
    {
        DeviceConfig robot;
        robot.name = QStringLiteral("Driving Robot");
        robot.type = QStringLiteral("DrivingRobot");
        robot.ip = QStringLiteral("127.0.0.1");
        robot.port = 9000;

        DeviceManager manager;
        QSignalSpy statusSpy(&manager, &DeviceManager::deviceStatusChanged);
        QVERIFY(statusSpy.isValid());

        manager.configure({robot});
        QCOMPARE(statusSpy.count(), 1);

        statusSpy.clear();
        manager.connectDevice(0);
        QCOMPARE(statusSpy.count(), 1);

        statusSpy.clear();
        manager.tick(1000);
        QCOMPARE(statusSpy.count(), 1);

        statusSpy.clear();
        manager.disconnectDevice(0);
        QCOMPARE(statusSpy.count(), 1);
    }

    void marksMissingVehicleTypeUnavailable()
    {
        DeviceConfig robot;
        robot.name = QStringLiteral("Driving Robot");
        robot.type = QStringLiteral("DrivingRobot");
        robot.ip = QStringLiteral("127.0.0.1");
        robot.port = 9000;

        DeviceManager manager;
        manager.configure({robot});

        QCOMPARE(manager.drivingRobotStatus().value(QStringLiteral("available")).toBool(),
                 true);
        QCOMPARE(manager.targetPlatformStatus().value(QStringLiteral("available")).toBool(),
                 false);
    }

    void reconfigureReplacesVehicleSnapshots()
    {
        DeviceConfig initialRobot;
        initialRobot.name = QStringLiteral("Initial Robot");
        initialRobot.type = QStringLiteral("DrivingRobot");
        initialRobot.ip = QStringLiteral("127.0.0.1");
        initialRobot.port = 9000;

        DeviceConfig platform;
        platform.name = QStringLiteral("Target Platform");
        platform.type = QStringLiteral("TargetPlatform");
        platform.ip = QStringLiteral("127.0.0.2");
        platform.port = 9001;

        DeviceConfig replacementRobot;
        replacementRobot.name = QStringLiteral("Replacement Robot");
        replacementRobot.type = QStringLiteral("DrivingRobot");
        replacementRobot.ip = QStringLiteral("192.168.1.10");
        replacementRobot.port = 9100;

        DeviceManager manager;
        manager.configure({initialRobot, platform});
        manager.configure({replacementRobot});

        const QVariantMap robotStatus = manager.drivingRobotStatus();
        QCOMPARE(robotStatus.value(QStringLiteral("available")).toBool(), true);
        QCOMPARE(robotStatus.value(QStringLiteral("name")).toString(),
                 replacementRobot.name);
        QCOMPARE(robotStatus.value(QStringLiteral("ip")).toString(),
                 replacementRobot.ip);
        QCOMPARE(manager.targetPlatformStatus().value(QStringLiteral("available")).toBool(),
                 false);
    }

    void vehicleSnapshotHasStableKeysAndValueTypes()
    {
        DeviceConfig robot;
        robot.name = QStringLiteral("Driving Robot");
        robot.type = QStringLiteral("DrivingRobot");
        robot.ip = QStringLiteral("127.0.0.1");
        robot.port = 9000;

        DeviceManager manager;
        manager.configure({robot});
        manager.connectAll();
        manager.tick(1000);

        const QVariantMap status = manager.drivingRobotStatus();
        QCOMPARE(status.keys(), QStringList({
            QStringLiteral("available"),
            QStringLiteral("connected"),
            QStringLiteral("fault"),
            QStringLiteral("heartbeatOk"),
            QStringLiteral("ip"),
            QStringLiteral("latencyMs"),
            QStringLiteral("name"),
            QStringLiteral("packetLossPercent"),
            QStringLiteral("port"),
            QStringLiteral("signalStrengthPercent"),
            QStringLiteral("type")
        }));

        QCOMPARE(status.value(QStringLiteral("name")).metaType().id(), QMetaType::QString);
        QCOMPARE(status.value(QStringLiteral("type")).metaType().id(), QMetaType::QString);
        QCOMPARE(status.value(QStringLiteral("ip")).metaType().id(), QMetaType::QString);
        QCOMPARE(status.value(QStringLiteral("port")).metaType().id(), QMetaType::Int);
        QCOMPARE(status.value(QStringLiteral("latencyMs")).metaType().id(), QMetaType::Int);
        QCOMPARE(status.value(QStringLiteral("signalStrengthPercent")).metaType().id(),
                 QMetaType::Int);
        QCOMPARE(status.value(QStringLiteral("connected")).metaType().id(), QMetaType::Bool);
        QCOMPARE(status.value(QStringLiteral("heartbeatOk")).metaType().id(), QMetaType::Bool);
        QCOMPARE(status.value(QStringLiteral("fault")).metaType().id(), QMetaType::Bool);
        QCOMPARE(status.value(QStringLiteral("available")).metaType().id(), QMetaType::Bool);
        QVERIFY(status.value(QStringLiteral("packetLossPercent")).canConvert<double>());
    }
};

QTEST_GUILESS_MAIN(TestDeviceTelemetry)
#include "TestDeviceTelemetry.moc"
