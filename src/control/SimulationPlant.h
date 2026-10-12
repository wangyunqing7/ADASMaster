// =============================================================================
// 功能：示例/回归用非线性自行车车辆和角度伺服仿真，非硬件驱动。
// 输入：MPC 方向盘目标；输出：质心位姿、侧向速度、横摆角速度及编码器角度。
// 小步长 RK4 与非线性轮胎滑角用于独立于 MPC 离散模型的验证。
// =============================================================================
#pragma once
#include "src/control/ISteeringRobot.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lateral::simulation {
class Robot : public ISteeringRobot {
public:
    bool rejectCommands = false;
    bool rejectEnable = false;
    int commandCount = 0;
    int stopCount = 0;
    SteeringCommand lastCommand;
    SteeringFeedback feedback;
    bool requestEnable() override {
        if (rejectEnable) return false;
        feedback.enabled = true;
        emit feedbackReceived(feedback);
        return true;
    }
    bool sendAngleCommand(const SteeringCommand &c) override {
        if (rejectCommands) return false;
        lastCommand = c;
        ++commandCount;
        return true;
    }
    void requestStop(bool, const QString &) override {
        ++stopCount;
        feedback.enabled = false;
        emit feedbackReceived(feedback);
    }
    void publish(qint64 nowMs, double angleRad, double rateRadS = 0.0) {
        feedback.timestampMs = nowMs;
        feedback.angleRad = angleRad;
        feedback.rateRadS = rateRadS;
        feedback.online = true;
        emit feedbackReceived(feedback);
    }
};

class Plant {
public:
    VehicleState state;
    VehicleParameters vehicle;
    double roadAngleRad = 0.0;
    double robotAngleRad() const { return vehicle.robotAngle(roadAngleRad); }
    void advance(double roadTargetRad, double dt, double maximumRobotRateRadS,
                 double dynamicThresholdMps = 3.0) {
        using State = std::array<double, 6>; // x,y,yaw,vy,r,delta
        State x{{state.xM, state.yM, state.yawRad, state.lateralSpeedMps,
                 state.yawRateRadS, roadAngleRad}};
        const double speed = state.longitudinalSpeedMps;
        const auto derivative = [&](const State &p) {
            State d{};
            d[5] = std::clamp((roadTargetRad - p[5]) / vehicle.actuatorTimeConstantS,
                             -maximumRobotRateRadS / vehicle.steeringRatio,
                              maximumRobotRateRadS / vehicle.steeringRatio);
            if (speed < dynamicThresholdMps) {
                const double beta = std::atan(vehicle.rearAxleToCgM / vehicle.wheelbaseM() * std::tan(p[5]));
                const double vy = speed * std::tan(beta);
                d[0] = speed * std::cos(p[2]) - vy * std::sin(p[2]);
                d[1] = speed * std::sin(p[2]) + vy * std::cos(p[2]);
                d[2] = speed / vehicle.wheelbaseM() * std::tan(p[5]);
            } else {
                const double frontForce = vehicle.frontCorneringStiffnessNPerRad
                    * (p[5] - std::atan2(p[3] + vehicle.frontAxleToCgM * p[4], speed));
                const double rearForce = -vehicle.rearCorneringStiffnessNPerRad
                    * std::atan2(p[3] - vehicle.rearAxleToCgM * p[4], speed);
                d[0] = speed * std::cos(p[2]) - p[3] * std::sin(p[2]);
                d[1] = speed * std::sin(p[2]) + p[3] * std::cos(p[2]);
                d[2] = p[4];
                d[3] = (frontForce * std::cos(p[5]) + rearForce) / vehicle.massKg - speed * p[4];
                d[4] = (vehicle.frontAxleToCgM * frontForce * std::cos(p[5])
                        - vehicle.rearAxleToCgM * rearForce) / vehicle.yawInertiaKgM2;
            }
            return d;
        };
        const int steps = std::max(1, static_cast<int>(std::ceil(dt / 0.002)));
        const double h = dt / steps;
        for (int step = 0; step < steps; ++step) {
            const auto k1 = derivative(x);
            State t;
            for (int i = 0; i < 6; ++i) t[i] = x[i] + 0.5 * h * k1[i];
            const auto k2 = derivative(t);
            for (int i = 0; i < 6; ++i) t[i] = x[i] + 0.5 * h * k2[i];
            const auto k3 = derivative(t);
            for (int i = 0; i < 6; ++i) t[i] = x[i] + h * k3[i];
            const auto k4 = derivative(t);
            for (int i = 0; i < 6; ++i) x[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
        }
        state.xM = x[0]; state.yM = x[1]; state.yawRad = wrapAngle(x[2]);
        state.lateralSpeedMps = x[3]; state.yawRateRadS = x[4]; roadAngleRad = x[5];
        if (speed < dynamicThresholdMps) {
            state.lateralSpeedMps = speed * vehicle.rearAxleToCgM / vehicle.wheelbaseM() * std::tan(roadAngleRad);
            state.yawRateRadS = speed / vehicle.wheelbaseM() * std::tan(roadAngleRad);
        }
    }
};

inline std::vector<ReferencePoint> makePath(const QString &kind, double radiusM = 80.0,
                                          double laneLengthM = 60.0, double laneWidthM = 3.5,
                                          double lengthM = 600.0) {
    std::vector<ReferencePoint> path;
    double arcLength = 0.0, lastX = 0.0, lastY = 0.0;
    const int count = static_cast<int>(std::ceil(std::min(lengthM, 600.0) / 0.25));
    for (int i = 0; i <= count; ++i) {
        const double t = i * 0.25;
        ReferencePoint p;
        if (kind == QLatin1String("circle")) {
            const double radius = radiusM;
            // 少于一整圈，避免仿真初次投影的重叠歧义。
            if (t > std::min(350.0, radius * 1.8 * pi)) break;
            p = {t, radius * std::sin(t / radius), radius * (1.0 - std::cos(t / radius)),
                 wrapAngle(t / radius), 1.0 / radius};
        } else {
            double y = 0.0, slope = 0.0, second = 0.0;
            if (kind == QLatin1String("lane-change")) {
                if (t <= laneLengthM) {
                    y = laneWidthM / 2.0 * (1.0 - std::cos(pi * t / laneLengthM));
                    slope = laneWidthM / 2.0 * pi / laneLengthM * std::sin(pi * t / laneLengthM);
                    second = laneWidthM / 2.0 * pi * pi / (laneLengthM * laneLengthM)
                        * std::cos(pi * t / laneLengthM);
                } else y = laneWidthM;
            }
            if (i > 0) arcLength += std::hypot(t - lastX, y - lastY);
            p = {arcLength, t, y, std::atan(slope), second / std::pow(1.0 + slope * slope, 1.5)};
        }
        lastX = p.xM; lastY = p.yM;
        path.push_back(p);
    }
    return path;
}
} // namespace lateral::simulation
