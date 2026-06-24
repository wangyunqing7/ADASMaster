#include <QtTest>
#include <QAbstractItemModelTester>
#include <QTemporaryDir>

#include "src/alarm/AlarmManager.h"
#include "src/persistence/DatabaseManager.h"

class TestAlarmPersistence : public QObject
{
    Q_OBJECT

private slots:
    void persistsAndAcknowledgesAlarm()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("alarms.db"))));

        TestRunRecord run;
        run.id = QStringLiteral("run-alarm");
        run.scenarioName = QStringLiteral("AEB");
        run.scenarioType = QStringLiteral("AEB");
        run.startedAtMs = 1;
        run.status = QStringLiteral("RUNNING");
        run.operatorName = QStringLiteral("operator");
        run.appVersion = QStringLiteral("0.1");
        run.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(run));

        AlarmManager alarms;
        alarms.setDatabaseManager(&database);
        alarms.setCurrentRunId(run.id);
        alarms.raise(AlarmLevel::Warning, AlarmSource::Device,
                     QStringLiteral("设备警告"), QStringLiteral("延迟升高"));

        QCOMPARE(database.eventCount(run.id), 1);
        QCOMPARE(alarms.alarmListModel()->count(), 1);
        QVERIFY(alarms.alarmListModel()->eventAt(0).databaseId > 0);

        alarms.acknowledge(0);
        QVERIFY(alarms.alarmListModel()->eventAt(0).acknowledged);
        QVERIFY(database.eventAcknowledged(
            alarms.alarmListModel()->eventAt(0).databaseId));
    }

    void remainsAValidModelAtCapacity()
    {
        AlarmListModel model;
        QAbstractItemModelTester tester(&model,
            QAbstractItemModelTester::FailureReportingMode::QtTest);
        for (int i = 0; i < 1001; ++i) {
            EventLog event;
            event.timestampMs = i;
            event.title = QString::number(i);
            model.append(event);
        }
        QCOMPARE(model.count(), 1000);
        QCOMPARE(model.eventAt(0).title, QStringLiteral("1000"));
    }
};

QTEST_GUILESS_MAIN(TestAlarmPersistence)
#include "TestAlarmPersistence.moc"
