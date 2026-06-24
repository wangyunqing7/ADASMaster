#include <QtTest>
#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "src/app/AppController.h"

class TestAppController : public QObject
{
    Q_OBJECT

private slots:
    void initializesConfigDevicesAndDatabase()
    {
        QTemporaryDir storage;
        AppController controller(QDir::currentPath(), storage.path());

        QVERIFY2(controller.operationalReady(), qPrintable(controller.startupError()));
        QVERIFY(controller.configManager()->ready());
        QVERIFY(controller.databaseManager()->ready());
        QCOMPARE(controller.databaseManager()->schemaVersion(), 1);
        QCOMPARE(controller.deviceManager()->totalCount(), 5);
        QVERIFY(controller.databaseManager()->databasePath().startsWith(storage.path()));

        controller.startup();
        QCOMPARE(controller.deviceManager()->onlineCount(), 5);
        QVERIFY(controller.deviceManager()->requiredDevicesHealthy());
    }

    void blocksStartupWhenConfigurationIsInvalid()
    {
        QTemporaryDir invalidConfig;
        QTemporaryDir storage;
        AppController controller(invalidConfig.path(), storage.path());

        QVERIFY(!controller.operationalReady());
        QVERIFY(!controller.startupError().isEmpty());
        controller.startup();
        QCOMPARE(controller.deviceManager()->totalCount(), 0);
        QCOMPARE(controller.deviceManager()->onlineCount(), 0);
    }

    void createsAllScenarioNodeKindsAndPersistsThem()
    {
        QTemporaryDir storage;
        AppController controller(QDir::currentPath(), storage.path());

        const QString groupId = controller.createScenarioNode(
            QStringLiteral("group"), {}, QStringLiteral("VRU"));
        QVERIFY(!groupId.isEmpty());
        const QString subgroupId = controller.createScenarioNode(
            QStringLiteral("subgroup"), groupId, QStringLiteral("Pedestrian"));
        QVERIFY(!subgroupId.isEmpty());
        const QString scenarioId = controller.createScenarioNode(
            QStringLiteral("scenario"), subgroupId, QStringLiteral("Crossing 20"));
        QVERIFY(!scenarioId.isEmpty());
        QVERIFY(controller.createScenarioNode(QStringLiteral("scenario"), subgroupId,
                                              QStringLiteral("Crossing 20")).isEmpty());

        AppController reloaded(QDir::currentPath(), storage.path());
        const QByteArray json = QJsonDocument::fromVariant(reloaded.scenarioTree()).toJson();
        QVERIFY(json.contains("Crossing 20"));
        QVERIFY(reloaded.scenarioNames().contains(QStringLiteral("Crossing 20")));
    }

    void savesAndReloadsParametricTrajectory()
    {
        QTemporaryDir storage;
        QVariantList segments;
        segments.append(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("straight")},
            {QStringLiteral("startX"), 0.0}, {QStringLiteral("startY"), 0.0},
            {QStringLiteral("endX"), 100.0}, {QStringLiteral("endY"), 0.0},
            {QStringLiteral("startSpeedKph"), 0.0}, {QStringLiteral("endSpeedKph"), 10.0}
        });
        segments.append(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("arc")},
            {QStringLiteral("startX"), 100.0}, {QStringLiteral("startY"), 0.0},
            {QStringLiteral("endX"), 110.0}, {QStringLiteral("endY"), 10.0},
            {QStringLiteral("startSpeedKph"), 10.0}, {QStringLiteral("endSpeedKph"), 12.0},
            {QStringLiteral("radiusM"), 10.0}, {QStringLiteral("turnDirection"), QStringLiteral("left")}
        });

        {
            AppController controller(QDir::currentPath(), storage.path());
            QVERIFY(controller.saveScenarioTrajectory(QStringLiteral("AEB CCRs 50"),
                                                       QStringLiteral("ego"), segments));
            const QVariantList preview = controller.previewTrajectory(segments);
            QVERIFY(preview.size() > 100);
            QCOMPARE(controller.scenarioEgoWaypoints(QStringLiteral("AEB CCRs 50")).size(),
                     preview.size());
            const QVariantList normalized = controller.normalizeTrajectory(segments);
            QCOMPARE(normalized.size(), 2);
            QVERIFY(normalized.first().toMap().value(QStringLiteral("valid")).toBool());
            QCOMPARE(normalized.at(1).toMap().value(QStringLiteral("startX")).toDouble(), 100.0);
        }

        AppController reloaded(QDir::currentPath(), storage.path());
        const QVariantList loaded = reloaded.scenarioTrajectory(
            QStringLiteral("AEB CCRs 50"), QStringLiteral("ego"));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(1).toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("arc"));
        QCOMPARE(reloaded.scenarioEgoWaypoints(QStringLiteral("AEB CCRs 50")).size(),
                 reloaded.previewTrajectory(loaded).size());
    }

    void persistsMotionMonitorSelections()
    {
        QTemporaryDir storage;
        {
            AppController controller(QDir::currentPath(), storage.path());
            controller.setMonitorDeviceSelection(0, QStringLiteral("驾驶机器人 #1"));
            controller.setMonitorDeviceSelection(1, QStringLiteral("目标车平台 #1"));
        }
        AppController reloaded(QDir::currentPath(), storage.path());
        QCOMPARE(reloaded.monitorDeviceSelection(0), QStringLiteral("驾驶机器人 #1"));
        QCOMPARE(reloaded.monitorDeviceSelection(1), QStringLiteral("目标车平台 #1"));
    }
};

QTEST_GUILESS_MAIN(TestAppController)
#include "TestAppController.moc"
