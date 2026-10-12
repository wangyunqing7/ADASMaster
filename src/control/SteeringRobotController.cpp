// =============================================================================
// 功能：串行执行安全校验、MPC、传输接受检查；故障时停止并等待显式复位。
// =============================================================================
#include "SteeringRobotController.h"
#include <QThread>
#include <algorithm>
#include <cmath>

namespace lateral {
SteeringRobotController::SteeringRobotController(QObject *parent) : QObject(parent) {
    qRegisterMetaType<VehicleState>();
    qRegisterMetaType<SteeringFeedback>();
    qRegisterMetaType<SteeringCommand>();
}
SteeringRobotController::~SteeringRobotController() {
    if (m_robot && (m_controlState == State::Running || m_controlState == State::Armed)) {
        // 先断开同步反馈回调，再请求停机；通信适配器仍须有独立看门狗。
        disconnect(m_robot, nullptr, this, nullptr);
        m_robot->requestStop(false, QStringLiteral("Controller shutdown"));
    }
}
bool SteeringRobotController::configure(const VehicleParameters &v, const MpcSettings &s, QString *error) {
    if (m_controlState != State::Disabled) {
        if (error) *error = QStringLiteral("Stop and reset before changing configuration");
        return false;
    }
    return m_mpc.configure(v, s, error);
}
bool SteeringRobotController::setReferencePath(const std::vector<ReferencePoint> &p, QString *error) {
    if (m_controlState != State::Disabled) {
        if (error) *error = QStringLiteral("Stop and reset before changing path");
        return false;
    }
    if (!m_path.setPoints(p, error)) return false;
    m_mpc.reset();
    return true;
}
bool SteeringRobotController::setRobot(ISteeringRobot *robot) {
    if (m_controlState != State::Disabled) return false;
    if (m_robot) disconnect(m_robot, nullptr, this, nullptr);
    m_robot = robot;
    m_feedback = {};
    if (robot) {
        connect(robot, &ISteeringRobot::feedbackReceived, this, &SteeringRobotController::updateFeedback);
        connect(robot, &QObject::destroyed, this, [this] {
            if (m_controlState == State::Running || m_controlState == State::Armed)
                trip(QStringLiteral("Steering transport destroyed"));
        });
    }
    return true;
}
QString SteeringRobotController::healthIssue(qint64 now, bool enabled, bool pathControl) const {
    const auto &s = m_mpc.settings();
    const auto &v = m_mpc.vehicle();
    if (m_externalEmergency || m_feedback.emergencyStop) return QStringLiteral("Emergency stop active");
    if (!m_robot) return QStringLiteral("No steering transport");
    if (now < 0 || m_feedback.timestampMs < 0 || m_feedback.timestampMs > now
        || now - m_feedback.timestampMs > s.feedbackTimeoutMs)
        return QStringLiteral("Steering feedback stale or clock invalid");
    if (pathControl && (m_vehicleState.timestampMs < 0 || m_vehicleState.timestampMs > now
        || now - m_vehicleState.timestampMs > s.stateTimeoutMs))
        return QStringLiteral("Vehicle state stale or clock invalid");
    if (!m_feedback.online || m_feedback.fault || (enabled && !m_feedback.enabled))
        return QStringLiteral("Steering robot offline, faulted or disabled");
    if (!std::isfinite(m_feedback.angleRad) || !std::isfinite(m_feedback.rateRadS)
        || !std::isfinite(m_feedback.torqueNm)
        || (pathControl && (!finiteState(m_vehicleState) || !m_vehicleState.localizationValid)))
        return QStringLiteral("Invalid localization or steering measurement");
    if (std::abs(m_feedback.torqueNm) > s.maximumRobotTorqueNm
        || std::abs(m_feedback.angleRad) > s.maximumRobotAngleRad
        || std::abs(v.roadWheelAngle(m_feedback.angleRad)) > s.maximumRoadWheelAngleRad
        || std::abs(m_feedback.rateRadS) > s.maximumRobotRateRadS * 1.1)
        return QStringLiteral("Steering feedback exceeds limits");
    if (pathControl && m_path.empty()) return QStringLiteral("Reference path missing");
    return {};
}
bool SteeringRobotController::arm(qint64 now) {
    if (m_controlState != State::Disabled) return false;
    const QString issue = healthIssue(now, false);
    if (!issue.isEmpty()) { trip(issue, m_externalEmergency || m_feedback.emergencyStop); return false; }
    m_mpc.reset();
    m_previousRoadTarget = m_mpc.vehicle().roadWheelAngle(m_feedback.angleRad);
    m_lastTickMs = -1;
    m_controlState = State::Armed;
    if (!m_robot->requestEnable()) { trip(QStringLiteral("Robot rejected enable")); return false; }
    emit diagnosticsChanged();
    return m_controlState == State::Armed;
}
bool SteeringRobotController::start(qint64 now) {
    if (m_controlState != State::Armed) return false;
    const QString issue = healthIssue(now, true);
    if (!issue.isEmpty()) { trip(issue, m_externalEmergency || m_feedback.emergencyStop); return false; }
    // 等待使能确认期间编码器可能移动，重新以实测角度初始化。
    m_previousRoadTarget = m_mpc.vehicle().roadWheelAngle(m_feedback.angleRad);
    m_controlState = State::Running;
    emit diagnosticsChanged();
    return true;
}
void SteeringRobotController::updateVehicleState(const VehicleState &state) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (state.timestampMs < m_vehicleState.timestampMs) {
        if (m_controlState == State::Running || m_controlState == State::Armed)
            trip(QStringLiteral("Vehicle state timestamp moved backwards"));
        return;
    }
    m_vehicleState = state;
}
void SteeringRobotController::updateFeedback(const SteeringFeedback &feedback) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (feedback.emergencyStop) {
        m_feedback = feedback;
        trip(QStringLiteral("Robot emergency stop"), true);
        return;
    }
    if (feedback.timestampMs < m_feedback.timestampMs) {
        if (m_controlState == State::Running || m_controlState == State::Armed)
            trip(QStringLiteral("Robot feedback timestamp moved backwards"));
        return;
    }
    m_feedback = feedback;
    if ((m_controlState == State::Running || m_controlState == State::Armed)
        && (!feedback.online || feedback.fault || (m_controlState == State::Running && !feedback.enabled)))
        trip(QStringLiteral("Robot reported a fault or disconnected"));
}
void SteeringRobotController::trip(const QString &reason, bool emergency) {
    if (m_controlState == State::Emergency || (m_controlState == State::Fault && !emergency)) return;
    m_controlState = emergency ? State::Emergency : State::Fault;
    m_reason = reason;
    m_mpc.reset();
    if (m_robot) m_robot->requestStop(emergency, reason);
    emit stopRequested(emergency, reason);
    emit diagnosticsChanged();
}
void SteeringRobotController::stop() {
    if (m_controlState == State::Fault || m_controlState == State::Emergency) return;
    m_controlState = State::Disabled;
    m_mpc.reset();
    if (m_robot) m_robot->requestStop(false, QStringLiteral("Operator stop"));
    emit stopRequested(false, QStringLiteral("Operator stop"));
    emit diagnosticsChanged();
}
void SteeringRobotController::setExternalEmergencyStop(bool active) {
    m_externalEmergency = active;
    if (active) trip(QStringLiteral("External safety emergency stop"), true);
}
bool SteeringRobotController::resetFault(qint64 now) {
    if (m_controlState != State::Fault && m_controlState != State::Emergency) return false;
    if (now < 0 || m_externalEmergency || m_feedback.emergencyStop) return false;
    // 复位只是退回 Disabled；路径/定位的缺失不能阻止停用后的重新配置。
    // 有适配器时必须确认机器人反馈安全且已经解除使能。
    if (m_robot && (!healthIssue(now, false, false).isEmpty() || m_feedback.enabled)) return false;
    m_reason.clear();
    m_controlState = State::Disabled;
    m_lastTickMs = -1;
    m_mpc.reset();
    emit diagnosticsChanged();
    return true;
}
void SteeringRobotController::processOnce(qint64 now) {
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_controlState != State::Running && m_controlState != State::Armed) return;
    const QString issue = healthIssue(now, m_controlState == State::Running);
    if (!issue.isEmpty()) { trip(issue, m_externalEmergency || m_feedback.emergencyStop); return; }
    if (m_controlState != State::Running) return;
    const auto &settings = m_mpc.settings();
    const double periodMs = settings.sampleTimeS * 1000.0;
    if (m_lastTickMs >= 0) {
        const qint64 interval = now - m_lastTickMs;
        if (interval < 0 || interval > periodMs * 1.5) {
            trip(QStringLiteral("Control scheduler missed deadline or clock moved backwards"));
            return;
        }
        if (interval < periodMs * 0.9) return; // 过早调用不重复下发
    }
    m_lastTickMs = now;
    m_result = m_mpc.compute(m_vehicleState,
        m_mpc.vehicle().roadWheelAngle(m_feedback.angleRad), m_previousRoadTarget, m_path);
    if (!m_result.ok()) { trip(m_result.message); return; }
    const qint64 commandTime = now + static_cast<qint64>(std::ceil(m_result.solveTimeMs));
    const QString postIssue = healthIssue(commandTime, true);
    if (!postIssue.isEmpty()) { trip(postIssue); return; }
    const SteeringCommand command{++m_sequence, commandTime,
        now + static_cast<qint64>(std::floor(periodMs * 1.5)), m_result.robotTargetRad,
        settings.maximumRobotRateRadS, settings.maximumRobotTorqueNm};
    if (!m_robot->sendAngleCommand(command)) { trip(QStringLiteral("Robot rejected angle command")); return; }
    if (m_controlState != State::Running) return; // 适配器同步回调可能报告急停
    m_previousRoadTarget = m_result.roadWheelTargetRad;
    emit commandIssued(command);
    emit diagnosticsChanged();
}
} // namespace lateral
