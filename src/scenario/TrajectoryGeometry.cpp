// =============================================================================
// TrajectoryGeometry.cpp —— 见头文件注释
// =============================================================================

#include "TrajectoryGeometry.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr double Epsilon = 1e-9;

double normalizePositive(double radians)
{
    double value = std::fmod(radians, 2.0 * Pi);
    if (value < 0.0) value += 2.0 * Pi;
    return value;
}

double normalizedDegrees(double degrees)
{
    double value = std::fmod(degrees, 360.0);
    if (value < 0.0) value += 360.0;
    return value;
}

QPointF rotatePoint(const QPointF &point, const QPointF &origin, double radians)
{
    const double dx = point.x() - origin.x();
    const double dy = point.y() - origin.y();
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return QPointF(origin.x() + dx * c - dy * s,
                   origin.y() + dx * s + dy * c);
}
}

DerivedTrajectorySegment TrajectoryGeometry::derive(const TrajectorySegment &segment)
{
    DerivedTrajectorySegment result;
    if (segment.startSpeedMps < 0.0 || segment.endSpeedMps < 0.0) {
        result.error = QStringLiteral("速度不能为负数");
        return result;
    }
    const double dx = segment.end.x() - segment.start.x();
    const double dy = segment.end.y() - segment.start.y();
    const double chord = std::hypot(dx, dy);
    if (chord <= Epsilon) {
        result.error = QStringLiteral("起点与终点不能重合");
        return result;
    }

    result.headingDeg = normalizedDegrees(std::atan2(dy, dx) * 180.0 / Pi);
    if (segment.kind == TrajectorySegmentKind::Straight) {
        result.lengthM = chord;
    } else {
        if (segment.radiusM <= 0.0 || segment.radiusM + Epsilon < chord / 2.0) {
            result.error = QStringLiteral("圆弧半径不能小于弦长的一半");
            return result;
        }
        const QPointF midpoint((segment.start.x() + segment.end.x()) / 2.0,
                               (segment.start.y() + segment.end.y()) / 2.0);
        const double h = std::sqrt(std::max(0.0,
            segment.radiusM * segment.radiusM - chord * chord / 4.0));
        const QPointF perpendicular(-dy / chord, dx / chord);
        const QPointF centers[2] = {
            midpoint + perpendicular * h,
            midpoint - perpendicular * h
        };

        double bestSweep = 1e100;
        QPointF bestCenter;
        double bestStart = 0.0;
        for (const QPointF &center : centers) {
            const double startAngle = std::atan2(segment.start.y() - center.y(),
                                                 segment.start.x() - center.x());
            const double endAngle = std::atan2(segment.end.y() - center.y(),
                                               segment.end.x() - center.x());
            const double ccw = normalizePositive(endAngle - startAngle);
            const double magnitude = segment.turnDirection == TrajectoryTurnDirection::Left
                ? ccw : normalizePositive(startAngle - endAngle);
            if (magnitude < bestSweep) {
                bestSweep = magnitude;
                bestCenter = center;
                bestStart = startAngle;
            }
        }
        result.center = bestCenter;
        result.startAngleDeg = normalizedDegrees(bestStart * 180.0 / Pi);
        result.sweepDeg = bestSweep * 180.0 / Pi;
        result.lengthM = segment.radiusM * bestSweep;
        const double maxSpeed = std::max(segment.startSpeedMps, segment.endSpeedMps);
        result.maxLateralAccelerationMps2 = maxSpeed * maxSpeed / segment.radiusM;
    }

    if (segment.startSpeedMps + segment.endSpeedMps <= Epsilon) {
        result.error = QStringLiteral("非零轨迹段至少需要一个非零端点速度");
        return result;
    }
    result.tangentialAccelerationMps2 =
        (segment.endSpeedMps * segment.endSpeedMps -
         segment.startSpeedMps * segment.startSpeedMps) / (2.0 * result.lengthM);
    result.durationSec = 2.0 * result.lengthM /
        (segment.startSpeedMps + segment.endSpeedMps);
    result.valid = true;
    return result;
}

QList<TrajectorySegment> TrajectoryGeometry::propagate(const QList<TrajectorySegment> &segments)
{
    QList<TrajectorySegment> output = segments;
    for (int i = 1; i < output.size(); ++i) {
        const QPointF offset = output.at(i - 1).end - output.at(i).start;
        output[i].start += offset;
        output[i].end += offset;
        output[i].startSpeedMps = output.at(i - 1).endSpeedMps;
    }
    return output;
}

QList<TrajectorySegment> TrajectoryGeometry::rotate(const QList<TrajectorySegment> &segments,
                                                    double degrees)
{
    if (segments.isEmpty()) return {};
    QList<TrajectorySegment> output = segments;
    const QPointF origin = segments.first().start;
    const double radians = degrees * Pi / 180.0;
    for (TrajectorySegment &segment : output) {
        segment.start = rotatePoint(segment.start, origin, radians);
        segment.end = rotatePoint(segment.end, origin, radians);
    }
    return output;
}

QList<Waypoint> TrajectoryGeometry::sample(const QList<TrajectorySegment> &segments,
                                           double spacingM)
{
    QList<Waypoint> points;
    const QList<TrajectorySegment> continuous = propagate(segments);
    for (int segmentIndex = 0; segmentIndex < continuous.size(); ++segmentIndex) {
        const TrajectorySegment &segment = continuous.at(segmentIndex);
        const DerivedTrajectorySegment d = derive(segment);
        if (!d.valid) return {};
        const int steps = std::max(1, static_cast<int>(std::ceil(d.lengthM /
            std::max(0.1, spacingM))));
        const int firstStep = segmentIndex == 0 ? 0 : 1;
        for (int step = firstStep; step <= steps; ++step) {
            const double t = static_cast<double>(step) / steps;
            QPointF point;
            if (segment.kind == TrajectorySegmentKind::Straight) {
                point = segment.start + (segment.end - segment.start) * t;
            } else {
                const double direction = segment.turnDirection == TrajectoryTurnDirection::Left ? 1.0 : -1.0;
                const double angle = d.startAngleDeg * Pi / 180.0 +
                                     direction * d.sweepDeg * Pi / 180.0 * t;
                point = d.center + QPointF(std::cos(angle) * segment.radiusM,
                                           std::sin(angle) * segment.radiusM);
            }
            const double speed = segment.startSpeedMps +
                                 (segment.endSpeedMps - segment.startSpeedMps) * t;
            points.append(Waypoint{point.x(), point.y(), speed});
        }
    }
    return points;
}

