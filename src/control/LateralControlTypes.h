// =============================================================================
// 功能：横向控制的参数、测量和结果；输入统一为 SI 单位，输出方向盘角度为 rad。
// 坐标：ENU 平面，逆时针/左转为正；车辆位置必须是质心，yaw 是车身朝向。
// =============================================================================
#pragma once

#include <array>
#include <QString>
#include <QMetaType>
#include <vector>

namespace lateral {
constexpr double pi = 3.14159265358979323846;
constexpr double radians(double degrees) { return degrees * pi / 180.0; }
constexpr double degrees(double radiansValue) { return radiansValue * 180.0 / pi; }

struct VehicleParameters {
    double massKg = 1600.0;
    double yawInertiaKgM2 = 2700.0;
    double frontAxleToCgM = 1.2;
    double rearAxleToCgM = 1.6;
    double frontCorneringStiffnessNPerRad = 80000.0; // 整个前轴，非单轮
    double rearCorneringStiffnessNPerRad = 80000.0;
    double steeringRatio = 16.0; // 方向盘角 / 等效前轮角
    double steeringSign = 1.0; // 机器人正方向与左转相同为 +1，否则 -1
    double steeringZeroRad = 0.0; // 机器人编码器直行零位
    double actuatorTimeConstantS = 0.08;
    double wheelbaseM() const { return frontAxleToCgM + rearAxleToCgM; }
    double roadWheelAngle(double robotAngleRad) const {
        return steeringSign * (robotAngleRad - steeringZeroRad) / steeringRatio;
    }
    double robotAngle(double roadAngleRad) const {
        return steeringZeroRad + steeringSign * steeringRatio * roadAngleRad;
    }
};

struct MpcSettings {
    double sampleTimeS = 0.02;
    int horizon = 30;
    double dynamicModelMinSpeedMps = 3.0;
    double minimumSpeedMps = 0.3; // 低于此值停止路径控制，禁止原地抢转
    double maximumSpeedMps = 35.0;
    std::array<double, 5> stateWeights{{40.0, 12.0, 0.2, 0.4, 0.5}};
    double terminalWeightMultiplier = 3.0;
    double inputWeight = 1.0;
    double inputRateWeight = 20.0;
    double maximumRoadWheelAngleRad = radians(25.0);
    double maximumRobotAngleRad = radians(540.0);
    double maximumRobotRateRadS = radians(600.0);
    double maximumLateralAccelerationMps2 = 3.0;
    double maximumLateralErrorM = 2.0;
    double maximumHeadingErrorRad = radians(35.0);
    double maximumRobotTorqueNm = 20.0;
    qint64 feedbackTimeoutMs = 100;
    qint64 stateTimeoutMs = 100;
    int maximumSolverIterations = 3000;
    double solverTimeLimitMs = 15.0;
    double solverAbsoluteTolerance = 1e-5;
    double solverRelativeTolerance = 1e-4;
};

struct VehicleState {
    qint64 timestampMs = -1; // 同一单调时钟，禁止使用 GNSS UTC 直接混入
    double xM = 0.0;
    double yM = 0.0;
    double yawRad = 0.0;
    double longitudinalSpeedMps = 0.0;
    double lateralSpeedMps = 0.0; // 车身坐标系，左为正；来自融合估计器
    double yawRateRadS = 0.0;
    bool localizationValid = false;
};

struct ReferencePoint {
    double sM = 0.0; // 严格递增的弧长
    double xM = 0.0;
    double yM = 0.0;
    double headingRad = 0.0; // 路径切线方向
    double curvaturePerM = 0.0;
};

enum class ControlStatus {
    Solved, InvalidConfiguration, InvalidInput, SpeedOutOfRange,
    TrackingEnvelopeExceeded, ReferenceUnavailable, SolverFailed
};

struct MpcResult {
    ControlStatus status = ControlStatus::InvalidInput;
    QString message;
    double roadWheelTargetRad = 0.0;
    double robotTargetRad = 0.0;
    double lateralErrorM = 0.0;
    double headingErrorRad = 0.0;
    double solveTimeMs = 0.0;
    int iterations = 0;
    bool dynamicModel = false;
    std::vector<double> predictedRoadWheelTargetsRad;
    std::vector<double> predictedLateralErrorsM;
    std::vector<double> predictedLateralAccelerationsMps2;
    bool ok() const { return status == ControlStatus::Solved; }
};

QString validateConfiguration(const VehicleParameters &vehicle, const MpcSettings &settings);
bool finiteState(const VehicleState &state);
double wrapAngle(double angleRad);
} // namespace lateral

Q_DECLARE_METATYPE(lateral::VehicleState)
