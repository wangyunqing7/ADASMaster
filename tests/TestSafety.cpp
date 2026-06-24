#include <QtTest>

#include "src/safety/SafetyManager.h"

class TestSafety : public QObject
{
    Q_OBJECT

private slots:
    void rejectsResetWhileHardwareInputIsActive()
    {
        SafetyManager safety;
        safety.updateHardwareEStop(true);

        QVERIFY(safety.emergencyStopActive());
        QVERIFY(!safety.requestReset());
        QVERIFY(safety.resetRequired());

        safety.updateHardwareEStop(false);
        QVERIFY(safety.requestReset());
        QCOMPARE(safety.safetyLevel(), SafetyLevel::Normal);
    }

    void rejectsResetWhenCommunicationIsDegraded()
    {
        SafetyManager safety;
        safety.triggerSoftwareEStop();
        safety.onCommunicationDegraded(false);

        QVERIFY(!safety.requestReset());
        safety.onCommunicationDegraded(true);
        QVERIFY(safety.requestReset());
    }
};

QTEST_GUILESS_MAIN(TestSafety)
#include "TestSafety.moc"
