// =============================================================================
// 功能：Qt 角度控制调度、反馈新鲜度、故障/急停锁存、限幅目标下发。
// 输入：同一单调时钟的状态/反馈、20ms 调度；输出：ISteeringRobot 指令和诊断信号。
// 必须在同一个工作线程调用；QML 通过 AppController 暴露的属性读取诊断。
// =============================================================================
#pragma once
#include <QPointer>
#include "ISteeringRobot.h"
#include "LateralMpc.h"

namespace lateral {
class SteeringRobotController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int controlState READ controlStateInt NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString faultReason READ faultReason NOTIFY diagnosticsChanged)
    Q_PROPERTY(double lateralErrorM READ lateralErrorM NOTIFY diagnosticsChanged)
    Q_PROPERTY(double targetAngleDeg READ targetAngleDeg NOTIFY diagnosticsChanged)
    Q_PROPERTY(double solveTimeMs READ solveTimeMs NOTIFY diagnosticsChanged)
public:
    enum class State { Disabled, Armed, Running, Fault, Emergency };
    Q_ENUM(State)
    explicit SteeringRobotController(QObject *parent = nullptr);
    ~SteeringRobotController() override;
    bool configure(const VehicleParameters &vehicle, const MpcSettings &settings,
                   QString *error = nullptr);
    bool setReferencePath(const std::vector<ReferencePoint> &points, QString *error = nullptr);
    bool setRobot(ISteeringRobot *robot);
    bool arm(qint64 nowMs);
    bool start(qint64 nowMs);
    bool resetFault(qint64 nowMs);
    void processOnce(qint64 nowMs);
    State controlState() const { return m_controlState; }
    int controlStateInt() const { return static_cast<int>(m_controlState); }
    QString faultReason() const { return m_reason; }
    double lateralErrorM() const { return m_result.lateralErrorM; }
    double targetAngleDeg() const { return degrees(m_result.robotTargetRad); }
    double solveTimeMs() const { return m_result.solveTimeMs; }
    const MpcResult &lastResult() const { return m_result; }
public slots:
    void updateVehicleState(const lateral::VehicleState &state);
    void updateFeedback(const lateral::SteeringFeedback &feedback);
    void stop();
    void setExternalEmergencyStop(bool active);
signals:
    void diagnosticsChanged();
    void commandIssued(const lateral::SteeringCommand &command);
    void stopRequested(bool emergency, const QString &reason);
private:
    QString healthIssue(qint64 nowMs, bool requireEnabled, bool requirePathControl = true) const;
    void trip(const QString &reason, bool emergency = false);
    QPointer<ISteeringRobot> m_robot;
    LateralMpc m_mpc;
    ReferencePath m_path;
    VehicleState m_vehicleState;
    SteeringFeedback m_feedback;
    MpcResult m_result;
    State m_controlState = State::Disabled;
    QString m_reason;
    bool m_externalEmergency = false;
    double m_previousRoadTarget = 0.0;
    qint64 m_lastTickMs = -1;
    quint64 m_sequence = 0;
};
} // namespace lateral
