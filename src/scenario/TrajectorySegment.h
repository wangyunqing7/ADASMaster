#pragma once

// =============================================================================
// TrajectorySegment.h
// 功能：定义参数化轨迹段输入结构，支持直线和恒半径圆弧。
// =============================================================================

#include <QPointF>

enum class TrajectorySegmentKind {
    Straight,
    CircularArc
};

enum class TrajectoryTurnDirection {
    Left,
    Right
};

struct TrajectorySegment
{
    TrajectorySegmentKind kind = TrajectorySegmentKind::Straight;
    QPointF start;
    QPointF end;
    double startSpeedMps = 0.0;
    double endSpeedMps = 0.0;
    double radiusM = 0.0;
    TrajectoryTurnDirection turnDirection = TrajectoryTurnDirection::Left;
};

