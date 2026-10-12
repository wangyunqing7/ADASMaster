// =============================================================================
// 功能：校验车辆/控制器参数与有限数值，避免无效参数进入矩阵及执行器映射。
// =============================================================================
#include "LateralControlTypes.h"
#include <cmath>

namespace lateral {
double wrapAngle(double a) { return std::remainder(a, 2.0 * pi); }
bool finiteState(const VehicleState &s) {
    return std::isfinite(s.xM) && std::isfinite(s.yM) && std::isfinite(s.yawRad)
        && std::isfinite(s.longitudinalSpeedMps) && std::isfinite(s.lateralSpeedMps)
        && std::isfinite(s.yawRateRadS);
}
QString validateConfiguration(const VehicleParameters &v, const MpcSettings &s) {
    const auto positive = [](double x) { return std::isfinite(x) && x > 0.0; };
    for (double x : {v.massKg, v.yawInertiaKgM2, v.frontAxleToCgM,
                    v.rearAxleToCgM, v.frontCorneringStiffnessNPerRad,
                    v.rearCorneringStiffnessNPerRad, v.steeringRatio,
                    v.actuatorTimeConstantS, s.sampleTimeS, s.minimumSpeedMps,
                    s.maximumSpeedMps, s.dynamicModelMinSpeedMps,
                    s.terminalWeightMultiplier, s.inputWeight, s.inputRateWeight,
                    s.maximumRoadWheelAngleRad, s.maximumRobotAngleRad,
                    s.maximumRobotRateRadS, s.maximumLateralAccelerationMps2,
                    s.maximumLateralErrorM, s.maximumHeadingErrorRad,
                    s.maximumRobotTorqueNm, s.solverTimeLimitMs,
                    s.solverAbsoluteTolerance, s.solverRelativeTolerance}) {
        if (!positive(x)) return QStringLiteral("Parameters must be finite and positive");
    }
    for (double w : s.stateWeights)
        if (!std::isfinite(w) || w < 0.0) return QStringLiteral("Invalid state weight");
    if (s.stateWeights[0] == 0.0 || s.stateWeights[1] == 0.0)
        return QStringLiteral("Tracking weights must be positive");
    if ((v.steeringSign != 1.0 && v.steeringSign != -1.0)
        || !std::isfinite(v.steeringZeroRad)
        || std::abs(v.steeringZeroRad) >= s.maximumRobotAngleRad)
        return QStringLiteral("Invalid steering sign or encoder zero");
    if (s.horizon < 5 || s.horizon > 80 || s.sampleTimeS < 0.005 || s.sampleTimeS > 0.1
        || s.minimumSpeedMps >= s.dynamicModelMinSpeedMps
        || s.dynamicModelMinSpeedMps >= s.maximumSpeedMps
        || s.maximumRoadWheelAngleRad >= radians(45.0)
        || s.maximumHeadingErrorRad >= pi / 2.0
        || s.feedbackTimeoutMs <= 0 || s.stateTimeoutMs <= 0
        || s.maximumSolverIterations < 1 || s.maximumSolverIterations > 10000
        || s.solverTimeLimitMs >= s.sampleTimeS * 1000.0)
        return QStringLiteral("Configuration outside supported operating range");
    return {};
}
} // namespace lateral
