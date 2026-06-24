#include <QtTest>
#include <QTemporaryDir>

#include "src/persistence/DatabaseManager.h"
#include "src/persistence/ReplayListModel.h"

class TestReplayListModel : public QObject
{
    Q_OBJECT

private slots:
    void loadsNewestRunsFromSQLite()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("replay.db"))));

        TestRunRecord first;
        first.id = QStringLiteral("first");
        first.scenarioName = QStringLiteral("AEB CCRs 50");
        first.scenarioType = QStringLiteral("AEB");
        first.startedAtMs = 1000;
        first.status = QStringLiteral("RUNNING");
        first.operatorName = QStringLiteral("operator");
        first.appVersion = QStringLiteral("0.1");
        first.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(first));
        QVERIFY(database.finishRun(first.id, QStringLiteral("FINISHED"),
                                   QStringLiteral("正常停止"), 1500));

        TestRunRecord second = first;
        second.id = QStringLiteral("second");
        second.scenarioName = QStringLiteral("LKA 01");
        second.scenarioType = QStringLiteral("LKA");
        second.startedAtMs = 2000;
        second.status = QStringLiteral("RUNNING");
        QVERIFY(database.createRun(second));

        ReplayListModel model;
        model.setDatabaseManager(&database);
        model.refresh();

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), ReplayListModel::RunIdRole).toString(),
                 QStringLiteral("second"));
        QCOMPARE(model.data(model.index(0), ReplayListModel::ScenarioNameRole).toString(),
                 QStringLiteral("LKA 01"));
        QCOMPARE(model.data(model.index(1), ReplayListModel::StatusRole).toString(),
                 QStringLiteral("FINISHED"));
        QCOMPARE(model.data(model.index(1), ReplayListModel::ResultTextRole).toString(),
                 QStringLiteral("正常停止"));
    }

    void filtersByScenario()
    {
        QTemporaryDir directory;
        DatabaseManager database;
        QVERIFY(database.open(directory.filePath(QStringLiteral("filter.db"))));

        TestRunRecord aeb;
        aeb.id = QStringLiteral("aeb-1");
        aeb.scenarioName = QStringLiteral("AEB CCRs 50");
        aeb.scenarioType = QStringLiteral("AEB");
        aeb.startedAtMs = 1000;
        aeb.status = QStringLiteral("FINISHED");
        aeb.operatorName = QStringLiteral("operator");
        aeb.appVersion = QStringLiteral("0.1");
        aeb.configSnapshot = QStringLiteral("{}");
        QVERIFY(database.createRun(aeb));

        TestRunRecord lka = aeb;
        lka.id = QStringLiteral("lka-1");
        lka.scenarioName = QStringLiteral("LKA 01");
        lka.scenarioType = QStringLiteral("LKA");
        lka.startedAtMs = 2000;
        QVERIFY(database.createRun(lka));

        ReplayListModel model;
        model.setDatabaseManager(&database);
        model.filterByScenario(QStringLiteral("AEB CCRs 50"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), ReplayListModel::ScenarioNameRole).toString(),
                 QStringLiteral("AEB CCRs 50"));

        // 空字符串恢复全部
        model.filterByScenario(QString());
        QCOMPARE(model.rowCount(), 2);
    }
};

QTEST_GUILESS_MAIN(TestReplayListModel)
#include "TestReplayListModel.moc"
