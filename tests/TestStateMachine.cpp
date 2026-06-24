#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

#include "src/alarm/AlarmManager.h"
#include "src/config/ConfigManager.h"
#include "src/device/DeviceManager.h"
#include "src/persistence/DataLogger.h"
#include "src/persistence/DatabaseManager.h"
#include "src/safety/SafetyManager.h"
#include "src/scenario/ScenarioEngine.h"
#include "src/state/StateManager.h"

class TestStateMachine : public QObject
{
    Q_OBJECT

    struct Fixture {
        QTemporaryDir directory;
        ConfigManager config{QDir::currentPath()};
        DatabaseManager database;
        DeviceManager devices;
        SafetyManager safety;
        StateManager state;
        AlarmManager alarms;
        DataLogger logger;
        ScenarioEngine engine;

        Fixture()
        {
            QString error;
            Q_ASSERT(config.load(&error));
            Q_ASSERT(database.open(directory.filePath(QStringLiteral("state-machine.db"))));
            devices.configure(config.devices());
            devices.connectAll();
            logger.setDatabaseManager(&database);
            logger.setStateManager(&state);
            alarms.setDatabaseManager(&database);
            engine.setDeviceManager(&devices);
            engine.setSafetyManager(&safety);
            engine.setStateManager(&state);
            engine.setAlarmManager(&alarms);
            engine.setDataLogger(&logger);
            engine.setScenarios(config.scenarios());
        }

        void prepare()
        {
            Q_ASSERT(engine.loadScenario(QStringLiteral("AEB CCRs 50")));
            Q_ASSERT(engine.checkDevices());
            Q_ASSERT(engine.arm());
        }
    };

private slots:
    void rejectsDuplicateStart()
    {
        Fixture fixture;
        fixture.prepare();

        // 无倒计时：start() 直接进入 Running，再次 start 应失败（状态不匹配）
        QVERIFY(fixture.engine.start());
        QCOMPARE(fixture.engine.testState(), TestState::Running);
        QVERIFY(!fixture.engine.start());
    }

    void rejectsScenarioLoadOutsideIdle()
    {
        Fixture fixture;
        QVERIFY(fixture.engine.loadScenario(QStringLiteral("AEB CCRs 50")));
        QVERIFY(fixture.engine.checkDevices());

        QVERIFY(!fixture.engine.loadScenario(QStringLiteral("RCTA 01")));
        QCOMPARE(fixture.engine.scenarioName(), QStringLiteral("AEB CCRs 50"));
    }

    void startsAndStopsRequiredDevices()
    {
        Fixture fixture;
        fixture.prepare();
        QVERIFY(fixture.engine.start());

        // 无倒计时：start() 立即进入 Running
        QCOMPARE(fixture.engine.testState(), TestState::Running);
        QVERIFY(fixture.devices.allRequiredRunning());

        QTest::qWait(250);

        QVERIFY(fixture.engine.stop());
        QCOMPARE(fixture.engine.testState(), TestState::Finished);
        QVERIFY(!fixture.devices.anyRunning());
        const QList<TestRunSummary> runs = fixture.database.listRuns();
        QCOMPARE(runs.size(), 1);
        QCOMPARE(runs.first().status, QStringLiteral("FINISHED"));
        QVERIFY(fixture.database.sampleCount(runs.first().id) >= 1);
    }

    void persistenceFailureStopsTheTest()
    {
        Fixture fixture;
        fixture.prepare();
        QVERIFY(fixture.engine.start());
        QCOMPARE(fixture.engine.testState(), TestState::Running);

        fixture.database.close();
        QTRY_COMPARE_WITH_TIMEOUT(fixture.engine.testState(), TestState::Fault, 2000);
        QVERIFY(!fixture.devices.anyRunning());
        QVERIFY(!fixture.logger.active());
    }
};

QTEST_GUILESS_MAIN(TestStateMachine)
#include "TestStateMachine.moc"
