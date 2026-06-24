#pragma once

// =============================================================================
// TrajectoryGeometry.h
// 功能：轨迹段几何/运动学派生、连续传播、整轨旋转及执行航点采样。
// =============================================================================

#include "TrajectorySegment.h"
#include "Waypoint.h"

#include <QList>
#include <QString>

struct DerivedTrajectorySegment
{
    bool valid = false;
    QString error;
    double lengthM = 0.0;
    double headingDeg = 0.0;
    double sweepDeg = 0.0;
    QPointF center;
    double startAngleDeg = 0.0;
    double durationSec = 0.0;
    double tangentialAccelerationMps2 = 0.0;
    double maxLateralAccelerationMps2 = 0.0;
};

class TrajectoryGeometry
{
public:
    static DerivedTrajectorySegment derive(const TrajectorySegment &segment);
    static QList<TrajectorySegment> propagate(const QList<TrajectorySegment> &segments);
    static QList<TrajectorySegment> rotate(const QList<TrajectorySegment> &segments,
                                           double degrees);
    static QList<Waypoint> sample(const QList<TrajectorySegment> &segments,
                                  double spacingM = 1.0);
};

