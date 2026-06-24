#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "src/persistence/DataLogger.h"
#include "src/persistence/DatabaseManager.h"
#include "src/scenario/ScenarioConfig.h"
#include "src/state/StateManager.h"

class TestDataLogger : public QObject
{
    Q_OBJECT

private slots:
    void buffersAndPersistsSamples()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("logger.db"))));

        StateManager state;
        state.setEgo(1.0, 2.0, 3.0, 4.0);
        state.setTarget(11.0, 12.0, 13.0, 14.0, QStringLiteral("car"));
        state.setRelative(15.0, 10.0, 1.5, false);

        DataLogger logger;
        logger.setDatabaseManager(&database);
        logger.setStateManager(&state);

        ScenarioConfig scenario;
        scenario.name = QStringLiteral("AEB CCRs 50");
        scenario.type = QStringLiteral("AEB");
        scenario.rawJson = QStringLiteral("{\"name\":\"AEB CCRs 50\"}");

        QVERIFY(logger.beginRun(scenario, 1000));
        QVERIFY(logger.active());
        const QString runId = logger.currentRunId();
        QVERIFY(!runId.isEmpty());

        for (int i = 0; i < 12; ++i)
            QVERIFY(logger.capture(1100 + i * 100));

        QCOMPARE(database.sampleCount(runId), 10);
        QVERIFY(logger.finishRun(QStringLiteral("FINISHED"), QStringLiteral("完成"), 3000));
        QVERIFY(!logger.active());
        QCOMPARE(database.sampleCount(runId), 12);

        const QList<TestRunSummary> runs = database.listRuns();
        QCOMPARE(runs.size(), 1);
        QCOMPARE(runs.first().id, runId);
        QCOMPARE(runs.first().status, QStringLiteral("FINISHED"));
        QCOMPARE(runs.first().resultText, QStringLiteral("完成"));
    }

    void reportsPersistenceFailure()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("failure.db"))));

        StateManager state;
        DataLogger logger;
        logger.setDatabaseManager(&database);
        logger.setStateManager(&state);

        ScenarioConfig scenario;
        scenario.name = QStringLiteral("LKA 01");
        scenario.type = QStringLiteral("LKA");
        scenario.rawJson = QStringLiteral("{}");
        QVERIFY(logger.beginRun(scenario, 1));

        QSignalSpy failureSpy(&logger, &DataLogger::persistenceFailed);
        database.close();
        for (int i = 0; i < 9; ++i)
            QVERIFY(logger.capture(2 + i));
        QVERIFY(!logger.capture(11));
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!logger.lastError().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDataLogger)
#include "TestDataLogger.moc"
