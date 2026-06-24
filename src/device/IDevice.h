#pragma once

// =============================================================================
// IDevice.h
// 功能：定义所有设备的统一抽象接口。无论是模拟设备 FakeDevice 还是未来的
//       真实设备（TCP/UDP/串口/CAN/厂商 SDK 客户端），都实现本接口。
// 输入：由 DeviceManager / ScenarioEngine / SafetyManager 调用控制方法。
// 输出：通过 status() 返回 DeviceStatus，通过 statusChanged() 通知状态变化。
//
// 【后续替换真实设备通信协议的方法】
//   1. 新建如 DrivingRobotClient : IDevice，在内部持有 QTcpSocket / QUdpSocket /
//      QSerialPort 或厂商 SDK 句柄。
//   2. connectDevice() 中建立连接；收到数据帧时解析协议并更新内部 DeviceStatus。
//   3. 用 QTimer 做心跳检测与通信超时判断。
//   4. DeviceManager 只需把 addDevice(new DrivingRobotClient(...)) 替换即可，
//      其余上层（场景引擎、安全管理、界面）完全不用改动。
// =============================================================================

#include <QObject>
#include "DeviceStatus.h"

class IDevice : public QObject
{
    Q_OBJECT
public:
    explicit IDevice(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IDevice() = default;

    // ---- 设备信息 ----
    virtual QString name() const = 0;
    virtual QString type() const = 0;
    // 更新设备基本信息（名称/IP/端口），用于设备管理页"编辑设备"
    virtual void setInfo(const QString &name, const QString &ip, int port) = 0;

    // ---- 控制接口（由上层 Manager 调用，不直接由 QML 调用）----
    virtual void connectDevice()    = 0;   // 建立连接
    virtual void disconnectDevice() = 0;   // 断开连接
    virtual void arm()              = 0;   // 预备
    virtual void start()            = 0;   // 开始
    virtual void stop()             = 0;   // 停止
    virtual void reset()            = 0;   // 复位
    virtual void emergencyStop()    = 0;   // 急停（软停）

    // ---- 状态查询 ----
    virtual DeviceStatus status() const = 0;

    // ---- 模拟时钟驱动（仅用于 FakeDevice 的周期刷新）----
    // 真实设备由网络回调更新状态，可直接保留此空实现不 override。
    virtual void tick(qint64 nowMs) { Q_UNUSED(nowMs) }

signals:
    // 状态发生变化时发出，DeviceManager 聚合后统一通知界面
    void statusChanged();
};
