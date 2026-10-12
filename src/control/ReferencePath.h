// =============================================================================
// 功能：保存带切线/曲率的质心轨迹，投影车辆位置并生成等时间预瞄参考。
// 输入：已平滑的弧长采样点；输出：参考点、带符号误差；不负责规划或平滑。
// =============================================================================
#pragma once
#include "LateralControlTypes.h"

namespace lateral {
struct PathProjection {
    ReferencePoint reference;
    double lateralErrorM = 0.0;
    double headingErrorRad = 0.0;
    size_t segment = 0;
};

class ReferencePath {
public:
    bool setPoints(const std::vector<ReferencePoint> &points, QString *error = nullptr);
    bool empty() const { return m_points.empty(); }
    double lengthM() const { return empty() ? 0.0 : m_points.back().sM; }
    ReferencePoint sample(double sM) const;
    // 初次全局搜索；此后限制在弧长窗口内，防止交叉路径跳支。
    bool project(const VehicleState &state, PathProjection *result,
                 double hintSM = -1.0, double searchWindowM = 20.0) const;
private:
    std::vector<ReferencePoint> m_points;
};
} // namespace lateral
