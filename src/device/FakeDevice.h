#pragma once

// =============================================================================
// FakeDevice.h
// 功能：模拟设备实现。在无真实硬件时，用于验证界面、业务流程、状态机和安全
//       管理。通过 QTimer 周期性模拟心跳、电量、延迟、运行状态等。
// 输入：构造时指定设备名称、类型、IP、端口；上层调用 connect/arm/start 等。
// 输出：实时更新的 DeviceStatus，供 DeviceManager 与界面使用。
//
// 【替换为真实设备的步骤】
//   继承 IDevice 写一个真实客户端类（如 DrivingRobotClient），在构造里建立
//   socket，在槽函数里解析收到的报文并更新 m_status，FakeDevice 即可弃用。
//   上层完全无感。
// =============================================================================

#include "IDevice.h"
#include <QTimer>

class FakeDevice : public IDevice
{
    Q_OBJECT
public:
    FakeDevice(const QString &name,
               const QString &type,
               const QString &ip,
               int port,
               bool motionTelemetry = false,
               QObject *parent = nullptr);

    QString name() const override { return m_status.name; }
    QString type() const override { return m_status.type; }
    void setInfo(const QString &name, const QString &ip, int port) override;

    void connectDevice()    override;
    void disconnectDevice() override;
    void arm()              override;
    void start()            override;
    void stop()             override;
    void reset()            override;
    void emergencyStop()    override;

    DeviceStatus status() const override { return m_status; }

    // 由 DeviceManager 的统一时钟驱动，刷新模拟量（心跳时间、延迟、电量等）
    void tick(qint64 nowMs);

private:
    DeviceStatus m_status;
    QTimer m_heartbeatTimer;   // 模拟心跳产生
    bool   m_estopped = false; // 是否处于急停
    qint64 m_lastTickMs = 0;
};
