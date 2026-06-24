#include <QtTest>
#include "src/config/ScenarioParser.h"

// =============================================================================
// TestScenarioParser.cpp
// 功能：验证 ScenarioParser 对 target.waypoints 字段的解析。
//       覆盖：正常解析、缺失字段向后兼容、格式错误报错。
// =============================================================================
class TestScenarioParser : public QObject
{
    Q_OBJECT

private slots:
    // 有 waypoints 时，解析填充 targetPath
    void parsesWaypoints()
    {
        const QByteArray json = R"({
            "name":"WP测试", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":3.0,"start_x":0,"start_y":0,"start_yaw":0,
                "waypoints":[
                    {"x":0,"y":0,"speed":0},
                    {"x":20,"y":0,"speed":3},
                    {"x":40,"y":2,"speed":3}
                ]},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));
        QCOMPARE(config.targetPath.size(), 3);
        QCOMPARE(config.targetPath.at(0).x, 0.0);
        QCOMPARE(config.targetPath.at(1).x, 20.0);
        QCOMPARE(config.targetPath.at(2).y, 2.0);
        QCOMPARE(config.targetPath.at(2).speed, 3.0);
    }

    // 缺少 waypoints 字段时，targetPath 为空（向后兼容）
    void emptyWaypointsWhenAbsent()
    {
        const QByteArray json = R"({
            "name":"无航点", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":0,"start_x":0,"start_y":0,"start_yaw":0},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));
        QVERIFY(config.targetPath.isEmpty());
    }

    // waypoints 存在但不是数组时报错
    void rejectsNonArrayWaypoints()
    {
        const QByteArray json = R"({
            "name":"坏航点", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":0,"start_x":0,"start_y":0,"start_yaw":0,
                "waypoints":"not-an-array"},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY(!ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error));
        QVERIFY2(error.contains(QStringLiteral("waypoints")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestScenarioParser)
#include "TestScenarioParser.moc"
