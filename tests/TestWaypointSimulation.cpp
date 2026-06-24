#include <QtTest>
#include "src/scenario/ScenarioEngine.h"
#include "src/scenario/ScenarioConfig.h"
#include "src/config/ScenarioParser.h"

// =============================================================================
// TestWaypointSimulation.cpp
// 功能：验证 ScenarioEngine 能报告加载工况的航点路径（供 QML 预览与测试读取）。
//       航点插值运动学的内部推进逻辑为 private，此处验证公开的只读访问接口。
// =============================================================================
class TestWaypointSimulation : public QObject
{
    Q_OBJECT

private slots:
    // 引擎能报告加载工况的航点数与各航点坐标（供 QML 预览与测试使用）
    void reportsWaypointCount()
    {
        const QByteArray json = R"({
            "name":"WP", "type":"AEB",
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

        ScenarioEngine engine;
        engine.setScenarios({config});
        QVERIFY(engine.loadScenario(QStringLiteral("WP")));
        QCOMPARE(engine.currentTargetWaypointCount(), 3);
        QCOMPARE(engine.currentWaypoint(0).x, 0.0);
        QCOMPARE(engine.currentWaypoint(1).x, 20.0);
        QCOMPARE(engine.currentWaypoint(2).y, 2.0);
    }

    // 无航点工况（LKA）航点数为 0
    void zeroWaypointsForLegacyScenario()
    {
        const QByteArray json = R"({
            "name":"Legacy", "type":"LKA",
            "ego":{"target_speed_mps":20,"start_x":0,"start_y":0,"start_yaw":0},
            "target":{"type":"车道线","speed_mps":0},
            "max_test_time_sec":30, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));

        ScenarioEngine engine;
        engine.setScenarios({config});
        QVERIFY(engine.loadScenario(QStringLiteral("Legacy")));
        QCOMPARE(engine.currentTargetWaypointCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestWaypointSimulation)
#include "TestWaypointSimulation.moc"
