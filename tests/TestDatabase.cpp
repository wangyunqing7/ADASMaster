#include <QtTest>
#include <QTemporaryDir>

#include "src/persistence/DatabaseManager.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void migratesAndPersistsRunData()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        DatabaseManager database;
        QVERIFY2(database.open(directory.filePath(QStringLiteral("test.db"))),
                 qPrintable(database.lastError()));
        QVERIFY(database.ready());
        QCOMPARE(database.schemaVersion(), 1);

        TestRunRecord run;
        run.id = QStringLiteral("run-1");
        run.scenarioName = QStringLiteral("AEB CCRs 50");
        run.scenarioType = QStringLiteral("AEB");
        run.startedAtMs = 1000;
        run.status = QStringLiteral("RUNNING");
        run.operatorName = QStringLiteral("operator");
        run.appVersion = QStringLiteral("0.1.0");
        run.configSnapshot = QStringLiteral("{\"name\":\"AEB CCRs 50\"}");
        QVERIFY2(database.createRun(run), qPrintable(database.lastError()));

        SampleRecord first;
        first.runId = run.id;
        first.timestampMs = 1100;
        first.egoSpeed = 10.0;
        first.distance = 40.0;
        first.ttc = 4.0;
        SampleRecord second = first;
        second.timestampMs = 1200;
        second.distance = 39.0;
        QVERIFY(database.appendSamples({first, second}));
        QCOMPARE(database.sampleCount(run.id), 2);

        const qint64 eventId = database.insertEvent(run.id, 1150, 1, 4,
                                                    QStringLiteral("设备警告"),
                                                    QStringLiteral("延迟升高"));
        QVERIFY(eventId > 0);
        QVERIFY(database.acknowledgeEvent(eventId));

        QVERIFY(database.finishRun(run.id, QStringLiteral("FINISHED"),
                                   QStringLiteral("模拟完成"), 2000));
        const QList<TestRunSummary> runs = database.listRuns();
        QCOMPARE(runs.size(), 1);
        QCOMPARE(runs.first().id, run.id);
        QCOMPARE(runs.first().status, QStringLiteral("FINISHED"));
        QCOMPARE(runs.first().resultText, QStringLiteral("模拟完成"));
        QCOMPARE(runs.first().finishedAtMs, 2000);
    }

    void rollsBackInvalidSampleBatch()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("rollback.db"))));

        TestRunRecord run;
        run.id = QStringLiteral("run-valid");
        run.scenarioName = QStringLiteral("LKA 01");
        run.scenarioType = QStringLiteral("LKA");
        run.startedAtMs = 1;
        run.status = QStringLiteral("RUNNING");
        run.operatorName = QStringLiteral("operator");
        run.appVersion = QStringLiteral("0.1.0");
        run.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(run));

        SampleRecord valid;
        valid.runId = run.id;
        valid.timestampMs = 2;
        SampleRecord invalid = valid;
        invalid.runId = QStringLiteral("missing-run");

        QVERIFY(!database.appendSamples({valid, invalid}));
        QCOMPARE(database.sampleCount(run.id), 0);
    }

    void listsRunsFilteredByScenario()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("filter.db"))));

        TestRunRecord aebRun;
        aebRun.id = QStringLiteral("aeb-filter-1");
        aebRun.scenarioName = QStringLiteral("AEB CCRs 50");
        aebRun.scenarioType = QStringLiteral("AEB");
        aebRun.startedAtMs = 1000;
        aebRun.status = QStringLiteral("FINISHED");
        aebRun.operatorName = QStringLiteral("tester");
        aebRun.appVersion = QStringLiteral("test");
        aebRun.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(aebRun));

        TestRunRecord rctaRun;
        rctaRun.id = QStringLiteral("rcta-filter-1");
        rctaRun.scenarioName = QStringLiteral("RCTA 01");
        rctaRun.scenarioType = QStringLiteral("RCTA");
        rctaRun.startedAtMs = 2000;
        rctaRun.status = QStringLiteral("FINISHED");
        rctaRun.operatorName = QStringLiteral("tester");
        rctaRun.appVersion = QStringLiteral("test");
        rctaRun.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(rctaRun));

        const QList<TestRunSummary> aebOnly = database.listRunsByScenario(QStringLiteral("AEB CCRs 50"));
        QVERIFY(aebOnly.size() >= 1);
        QVERIFY(std::all_of(aebOnly.cbegin(), aebOnly.cend(),
                            [](const TestRunSummary &s) { return s.scenarioName == QStringLiteral("AEB CCRs 50"); }));
        const QList<TestRunSummary> rctaOnly = database.listRunsByScenario(QStringLiteral("RCTA 01"));
        QCOMPARE(rctaOnly.size(), 1);
        QCOMPARE(rctaOnly.first().id, QStringLiteral("rcta-filter-1"));
    }

    void listsSamplesForRun()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("samples.db"))));

        TestRunRecord run;
        run.id = QStringLiteral("sample-run-1");
        run.scenarioName = QStringLiteral("AEB CCRs 50");
        run.scenarioType = QStringLiteral("AEB");
        run.startedAtMs = 3000;
        run.status = QStringLiteral("FINISHED");
        run.operatorName = QStringLiteral("tester");
        run.appVersion = QStringLiteral("test");
        run.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(run));

        const QString runId = run.id;
        SampleRecord s1; s1.runId = runId; s1.timestampMs = 100; s1.egoX = 1.0; s1.egoY = 0.0;
        SampleRecord s2; s2.runId = runId; s2.timestampMs = 200; s2.egoX = 2.0; s2.egoY = 0.0;
        QVERIFY(database.appendSamples({s1, s2}));
        const QList<SampleRecord> samples = database.listSamples(runId);
        QCOMPARE(samples.size(), 2);
        QCOMPARE(samples.at(0).timestampMs, 100);
        QCOMPARE(samples.at(1).timestampMs, 200);
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "TestDatabase.moc"
