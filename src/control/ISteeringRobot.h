// =============================================================================
// 功能：角度模式转向机器人的通信适配边界；输入编码器反馈，输出带期限的目标。
// 实现必须非阻塞、校验协议、在驱动器端实施角速度/转矩限制及独立通信看门狗。
// 本接口不定义 ABD 私有协议。stop 必须停止输出，不能自动回正。
// =============================================================================
#pragma once
#include <QObject>
#include "LateralControlTypes.h"

namespace lateral {
struct SteeringFeedback {
    qint64 timestampMs = -1;
    double angleRad = 0.0;
    double rateRadS = 0.0;
    double torqueNm = 0.0;
    bool online = false;
    bool enabled = false;
    bool fault = false;
    bool emergencyStop = false;
};
struct SteeringCommand {
    quint64 sequence = 0;
    qint64 timestampMs = -1;
    qint64 validUntilMs = -1;
    double targetAngleRad = 0.0;
    double maximumRateRadS = 0.0;
    double torqueLimitNm = 0.0;
};

class ISteeringRobot : public QObject {
    Q_OBJECT
public:
    explicit ISteeringRobot(QObject *parent = nullptr) : QObject(parent) {}
    virtual bool requestEnable() = 0;
    virtual bool sendAngleCommand(const SteeringCommand &command) = 0;
    virtual void requestStop(bool emergency, const QString &reason) = 0;
signals:
    void feedbackReceived(const lateral::SteeringFeedback &feedback);
};
} // namespace lateral
Q_DECLARE_METATYPE(lateral::SteeringFeedback)
Q_DECLARE_METATYPE(lateral::SteeringCommand)
