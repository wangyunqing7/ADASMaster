// =============================================================================
// 功能：验证界面数据模型的异步闭环、暂停/停止/急停、输入验证和 CSV 数据。
// =============================================================================
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <limits>
#include "src/control/MpcSimulationModel.h"

using namespace lateral;
class TestMpcSimulation : public QObject {
    Q_OBJECT
private:
    QVariantMap quickParameters(MpcSimulationModel &model) {
        auto p = model.defaults();
        p["scenario"] = "straight"; p["durationS"] = 0.5;
        p["playbackRate"] = 5.0; p["speedKph"] = 36.0; p["initialOffsetM"] = 0.15;
        return p;
    }
private slots:
    void runsAsynchronouslyAndExportsAllSamples() {
        MpcSimulationModel model;
        QCOMPARE(model.phase(), int(MpcSimulationModel::Ready));
        QVERIFY(!model.referencePath().isEmpty());
        const auto p = quickParameters(model);
        QVERIFY(model.start(p));
        QVERIFY(!model.start(p));
        QVERIFY(!model.preview(p));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 5000);
        QCOMPARE(model.phase(), int(MpcSimulationModel::Completed));
        QCOMPARE(model.sampleCount(), 10);
        QCOMPARE(model.chartSamples().size(), 10);
        QVERIFY(model.metrics().value("rmsM").toDouble() > 0.0);
        QCOMPARE(model.metrics().value("progress").toDouble(), 1.0);
        QVERIFY(model.chartSamples().last().toMap().value("xM").toDouble() > 4.0);
        QTemporaryDir storage;
        const auto path = storage.filePath(QStringLiteral("result.csv"));
        QVERIFY(model.exportCsv(QUrl::fromLocalFile(path)));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(rows.size(), 11);
        QVERIFY(rows[0].startsWith("time_s,x_m,y_m"));
        QCOMPARE(rows[1].split(',').size(), 9);
        QVERIFY(!model.exportCsv(QUrl(QStringLiteral("https://example.com/output.csv"))));
        QVERIFY(model.preview(p));
        QCOMPARE(model.sampleCount(), 0);
    }
    void pauseResumeAndStop() {
        MpcSimulationModel model;
        auto p = quickParameters(model); p["durationS"] = 3.0;
        QVERIFY(model.start(p));
        QTRY_VERIFY_WITH_TIMEOUT(model.sampleCount() >= 3, 5000);
        model.pauseResume();
        QTRY_COMPARE_WITH_TIMEOUT(model.phase(), int(MpcSimulationModel::Paused), 5000);
        const int pausedCount = model.sampleCount();
        QTest::qWait(70);
        QCOMPARE(model.sampleCount(), pausedCount);
        model.pauseResume();
        QTRY_VERIFY_WITH_TIMEOUT(model.sampleCount() > pausedCount, 5000);
        model.stop();
        QTRY_COMPARE_WITH_TIMEOUT(model.phase(), int(MpcSimulationModel::Stopped), 5000);
        const int stoppedCount = model.sampleCount();
        QTest::qWait(70);
        QCOMPARE(model.sampleCount(), stoppedCount);
        QVERIFY(!model.busy());
    }
    void systemEmergencyStopsAndBlocksRestart() {
        MpcSimulationModel model;
        auto p = quickParameters(model); p["durationS"] = 3.0;
        QVERIFY(model.start(p));
        QTRY_VERIFY_WITH_TIMEOUT(model.sampleCount() >= 2, 5000);
        model.setSafetyEmergencyStop(true);
        QTRY_COMPARE_WITH_TIMEOUT(model.phase(), int(MpcSimulationModel::Fault), 5000);
        QVERIFY(model.safetyBlocked());
        QVERIFY(!model.start(p));
        const int count = model.sampleCount(); QTest::qWait(70);
        QCOMPARE(model.sampleCount(), count);
        model.setSafetyEmergencyStop(false);
        p["durationS"] = 0.5;
        QVERIFY(model.start(p));
        QTRY_COMPARE_WITH_TIMEOUT(model.phase(), int(MpcSimulationModel::Completed), 5000);
        QCOMPARE(model.sampleCount(), 10);
    }
    void rejectsBadParametersWithoutLaunching() {
        MpcSimulationModel model;
        auto p = model.defaults(); p["horizon"] = 5.5;
        QVERIFY(!model.start(p)); QVERIFY(!model.message().isEmpty());
        p = model.defaults(); p["speedKph"] = "invalid";
        QVERIFY(!model.start(p));
        p = model.defaults(); p["actuatorLagMs"] = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!model.start(p));
        p = model.defaults(); p["scenario"] = "circle"; p["speedKph"] = 100;
        QVERIFY(!model.start(p));
        QCOMPARE(model.phase(), int(MpcSimulationModel::Ready));
        QCOMPARE(model.sampleCount(), 0);
    }
};
QTEST_GUILESS_MAIN(TestMpcSimulation)
#include "TestMpcSimulation.moc"
