#include <QtTest>
#include <QDir>
#include <algorithm>

#include "src/config/ConfigManager.h"
#include "src/config/ScenarioParser.h"

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void parsesAebScenario()
    {
        ScenarioConfig config;
        QString error;

        QVERIFY2(ScenarioParser::parseFile(QStringLiteral("scenarios/AEB_CCR_50.json"),
                                           &config, &error),
                 qPrintable(error));
        QCOMPARE(config.name, QStringLiteral("AEB CCRs 50"));
        QCOMPARE(config.type, QStringLiteral("AEB"));
        QCOMPARE(config.egoTargetSpeed, 13.89);
        QCOMPARE(config.targetType, QStringLiteral("静止车辆"));
        QVERIFY(!config.rawJson.isEmpty());
    }

    void parsesAllProjectConfiguration()
    {
        ConfigManager manager(QDir::currentPath());
        QString error;

        QVERIFY2(manager.load(&error), qPrintable(error));
        QCOMPARE(manager.devices().size(), 5);
        QCOMPARE(manager.scenarios().size(), 3);
        QVERIFY(std::all_of(manager.devices().cbegin(), manager.devices().cend(),
                            [](const DeviceConfig &device) { return device.required; }));
        QVERIFY(manager.scenarioByName(QStringLiteral("RCTA 01")) != nullptr);
        QVERIFY(manager.scenarioByName(QStringLiteral("LKA 01")) != nullptr);
        QCOMPARE(std::count_if(manager.devices().cbegin(), manager.devices().cend(),
                               [](const DeviceConfig &device) { return device.motionTelemetry; }),
                 2);
    }

    void rejectsScenarioWithMissingSpeed()
    {
        const QByteArray json = R"({
            "name":"Broken", "type":"AEB",
            "ego":{"start_x":0,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":0,"start_x":1,"start_y":0,"start_yaw":0},
            "max_test_time_sec":10, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;

        QVERIFY(!ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error));
        QVERIFY2(error.contains(QStringLiteral("ego.target_speed_mps")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "TestConfig.moc"
