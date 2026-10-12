// =============================================================================
// 功能：轨迹输入校验、连续线段投影、角度跨 ±pi 插值。
// =============================================================================
#include "ReferencePath.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace lateral {
bool ReferencePath::setPoints(const std::vector<ReferencePoint> &points, QString *error) {
    auto fail = [&](const char *message) {
        if (error) *error = QString::fromLatin1(message);
        return false;
    };
    if (points.size() < 2) return fail("Path needs at least two points");
    if (std::abs(points.front().sM) > 1e-6) return fail("Path arc length must start at zero");
    for (size_t i = 0; i < points.size(); ++i) {
        const auto &p = points[i];
        for (double value : {p.sM, p.xM, p.yM, p.headingRad, p.curvaturePerM})
            if (!std::isfinite(value)) return fail("Non-finite path point");
        if (i == 0) continue;
        const auto &previous = points[i - 1];
        const double ds = p.sM - previous.sM;
        const double chord = std::hypot(p.xM - previous.xM, p.yM - previous.yM);
        if (ds <= 1e-6 || chord <= 1e-6 || ds < chord * 0.98 || ds > chord * 1.1)
            return fail("Invalid arc length or duplicate path point; resample the path");
        const double chordHeading = std::atan2(p.yM - previous.yM, p.xM - previous.xM);
        if (std::abs(wrapAngle(chordHeading - previous.headingRad)) > radians(20.0)
            || std::abs(wrapAngle(p.headingRad - previous.headingRad)) > radians(20.0))
            return fail("Path heading discontinuity; smooth and resample the path");
    }
    m_points = points;
    if (error) error->clear();
    return true;
}

ReferencePoint ReferencePath::sample(double s) const {
    if (empty() || !std::isfinite(s)) return {};
    if (s <= m_points.front().sM) return m_points.front();
    if (s >= lengthM()) return m_points.back();
    const auto upper = std::upper_bound(m_points.begin(), m_points.end(), s,
        [](double value, const ReferencePoint &p) { return value < p.sM; });
    const auto &b = *upper;
    const auto &a = *(upper - 1);
    const double t = (s - a.sM) / (b.sM - a.sM);
    return {s, a.xM + t * (b.xM - a.xM), a.yM + t * (b.yM - a.yM),
            wrapAngle(a.headingRad + t * wrapAngle(b.headingRad - a.headingRad)),
            a.curvaturePerM + t * (b.curvaturePerM - a.curvaturePerM)};
}

bool ReferencePath::project(const VehicleState &state, PathProjection *result,
                            double hintS, double window) const {
    if (empty() || !result || !finiteState(state)) return false;
    double best = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i + 1 < m_points.size(); ++i) {
        const auto &a = m_points[i];
        const auto &b = m_points[i + 1];
        if (hintS >= 0.0 && (b.sM < hintS - window || a.sM > hintS + window)) continue;
        const double dx = b.xM - a.xM, dy = b.yM - a.yM;
        const double t = std::clamp(((state.xM - a.xM) * dx + (state.yM - a.yM) * dy)
                                    / (dx * dx + dy * dy), 0.0, 1.0);
        const double x = a.xM + t * dx, y = a.yM + t * dy;
        const double distance = std::hypot(state.xM - x, state.yM - y);
        if (distance >= best) continue;
        best = distance;
        result->reference = sample(a.sM + t * (b.sM - a.sM));
        result->segment = i;
    }
    if (!std::isfinite(best)) return false;
    const auto &p = result->reference;
    result->lateralErrorM = -std::sin(p.headingRad) * (state.xM - p.xM)
                            + std::cos(p.headingRad) * (state.yM - p.yM);
    result->headingErrorRad = wrapAngle(state.yawRad - p.headingRad);
    // 首尾外侧的纵向偏差不能被投影到端点后隐藏。
    const double along = std::cos(p.headingRad) * (state.xM - p.xM)
                          + std::sin(p.headingRad) * (state.yM - p.yM);
    return !(p.sM <= 1e-6 && along < -1.0) && !(p.sM >= lengthM() - 1e-6 && along > 0.1);
}
} // namespace lateral
