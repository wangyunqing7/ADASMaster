// =============================================================================
// TestTrajectoryGeometry.cpp
// 功能：验证直线/圆弧派生、连续段传播和整轨旋转。
// =============================================================================

#include <QtTest>

#include "src/scenario/TrajectoryGeometry.h"

class TestTrajectoryGeometry : public QObject
{
    Q_OBJECT

private slots:
    void derivesStraightKinematics()
    {
        TrajectorySegment s;
        s.kind = TrajectorySegmentKind::Straight;
        s.start = QPointF(0.0, 0.0);
        s.end = QPointF(100.0, 0.0);
        s.startSpeedMps = 0.0;
        s.endSpeedMps = 10.0 / 3.6;

        const DerivedTrajectorySegment d = TrajectoryGeometry::derive(s);
        QVERIFY2(d.valid, qPrintable(d.error));
        QCOMPARE(d.lengthM, 100.0);
        QVERIFY(qAbs(d.headingDeg) < 1e-9);
        QVERIFY(qAbs(d.tangentialAccelerationMps2 - 0.0385802469) < 1e-6);
        QVERIFY(qAbs(d.durationSec - 72.0) < 1e-6);
    }

    void derivesCircularArc()
    {
        TrajectorySegment s;
        s.kind = TrajectorySegmentKind::CircularArc;
        s.start = QPointF(0.0, 0.0);
        s.end = QPointF(10.0, 10.0);
        s.radiusM = 10.0;
        s.turnDirection = TrajectoryTurnDirection::Left;
        s.startSpeedMps = 2.0;
        s.endSpeedMps = 2.0;

        const DerivedTrajectorySegment d = TrajectoryGeometry::derive(s);
        QVERIFY2(d.valid, qPrintable(d.error));
        QVERIFY(qAbs(d.lengthM - (M_PI * 5.0)) < 1e-6);
        QVERIFY(qAbs(d.sweepDeg - 90.0) < 1e-6);
        QVERIFY(qAbs(d.maxLateralAccelerationMps2 - 0.4) < 1e-6);
    }

    void rejectsArcWhoseRadiusIsBelowHalfChord()
    {
        TrajectorySegment s;
        s.kind = TrajectorySegmentKind::CircularArc;
        s.start = QPointF(0.0, 0.0);
        s.end = QPointF(10.0, 0.0);
        s.radiusM = 4.9;
        s.endSpeedMps = 1.0;
        QVERIFY(!TrajectoryGeometry::derive(s).valid);
    }

    void propagatesAndRotatesSegments()
    {
        TrajectorySegment first;
        first.start = QPointF(0.0, 0.0);
        first.end = QPointF(10.0, 0.0);
        first.endSpeedMps = 2.0;
        TrajectorySegment second;
        second.start = QPointF(8.0, 8.0);
        second.end = QPointF(8.0, 18.0);
        second.startSpeedMps = 9.0;
        second.endSpeedMps = 3.0;

        const QList<TrajectorySegment> propagated = TrajectoryGeometry::propagate({first, second});
        QCOMPARE(propagated.at(1).start, QPointF(10.0, 0.0));
        QCOMPARE(propagated.at(1).end, QPointF(10.0, 10.0));
        QCOMPARE(propagated.at(1).startSpeedMps, 2.0);

        const QList<TrajectorySegment> rotated = TrajectoryGeometry::rotate(propagated, 90.0);
        QVERIFY(qAbs(rotated.at(0).end.x()) < 1e-9);
        QVERIFY(qAbs(rotated.at(0).end.y() - 10.0) < 1e-9);
        QVERIFY(qAbs(rotated.at(1).end.x() + 10.0) < 1e-9);
        QVERIFY(qAbs(rotated.at(1).end.y() - 10.0) < 1e-9);
    }
};

QTEST_GUILESS_MAIN(TestTrajectoryGeometry)
#include "TestTrajectoryGeometry.moc"
