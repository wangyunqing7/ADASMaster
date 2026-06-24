#pragma once

// =============================================================================
// DeviceStatus.h
// 功能：定义设备运行状态的统一数据结构，所有设备（驾驶机器人、目标车平台、
//       RTK 基站、安全系统、数据采集设备等）共用本结构。
// 输入：由各 IDevice 子类（如 FakeDevice、未来的真实协议客户端）填充。
// 输出：被 DeviceManager / DeviceListModel 读取，并通过 QAbstractListModel
//       的 role 暴露给 QML 显示。
// 说明：这里用 Q_GADGET + Q_PROPERTY，使该结构可作为 QVariant 在 QML 与
//       C++ 之间传递，也可直接被 ListModel 使用。
// =============================================================================

#include <QObject>
#include <QDateTime>

class DeviceStatus
{
    // Q_GADGET 使普通结构体获得 Q_PROPERTY / 元对象能力，无需 QObject 开销
    Q_GADGET

public:
    DeviceStatus() = default;

    // ---- 基本属性 ----
    QString name;            // 设备名称，如 "驾驶机器人 #1"
    QString type;            // 设备类型，如 "DrivingRobot"
    QString ip;              // IP 地址
    int     port = 0;        // 端口

    // ---- 通信状态 ----
    bool connected = false;        // 是否已连接
    bool heartbeatOk = false;      // 心跳是否正常
    qint64 lastUpdateTimeMs = 0;   // 最后更新时间（毫秒）
    int    latencyMs = 0;          // 通信延迟（毫秒）
    double packetLossPercent = 0.0;
    int    signalStrengthPercent = 0;

    // ---- 运行状态机 ----
    bool ready    = false;         // 自检就绪
    bool armed    = false;         // 已预备
    bool running  = false;         // 运行中
    bool fault    = false;         // 故障
    int  faultCode = 0;            // 故障码（0=无故障）

    // ---- 物理量 ----
    int batteryPercent = 100;      // 电池电量百分比
    bool motionCapable = false;
    double xM = 0.0;
    double yM = 0.0;
    double speedMps = 0.0;
    double yawDeg = 0.0;

    Q_PROPERTY(QString name             MEMBER name)
    Q_PROPERTY(QString type             MEMBER type)
    Q_PROPERTY(QString ip               MEMBER ip)
    Q_PROPERTY(int     port             MEMBER port)
    Q_PROPERTY(bool    connected        MEMBER connected)
    Q_PROPERTY(bool    heartbeatOk      MEMBER heartbeatOk)
    Q_PROPERTY(qint64  lastUpdateTimeMs MEMBER lastUpdateTimeMs)
    Q_PROPERTY(int     latencyMs        MEMBER latencyMs)
    Q_PROPERTY(double  packetLossPercent MEMBER packetLossPercent)
    Q_PROPERTY(int     signalStrengthPercent MEMBER signalStrengthPercent)
    Q_PROPERTY(bool    ready            MEMBER ready)
    Q_PROPERTY(bool    armed            MEMBER armed)
    Q_PROPERTY(bool    running          MEMBER running)
    Q_PROPERTY(bool    fault            MEMBER fault)
    Q_PROPERTY(int     faultCode        MEMBER faultCode)
    Q_PROPERTY(int     batteryPercent   MEMBER batteryPercent)
    Q_PROPERTY(bool    motionCapable MEMBER motionCapable)
    Q_PROPERTY(double  xM MEMBER xM)
    Q_PROPERTY(double  yM MEMBER yM)
    Q_PROPERTY(double  speedMps MEMBER speedMps)
    Q_PROPERTY(double  yawDeg MEMBER yawDeg)
};
Q_DECLARE_METATYPE(DeviceStatus)
