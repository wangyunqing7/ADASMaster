// =============================================================================
// FakeDevice.cpp —— 见 FakeDevice.h 头注释。模拟设备的具体实现。
// =============================================================================

#include "FakeDevice.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <QtMath>
#include <cmath>

FakeDevice::FakeDevice(const QString &name, const QString &type,
                       const QString &ip, int port, bool motionTelemetry, QObject *parent)
    : IDevice(parent)
{
    m_status.name = name;
    m_status.type = type;
    m_status.ip   = ip;
    m_status.port = port;
    m_status.motionCapable = motionTelemetry;
    m_status.speedMps = motionTelemetry ? 2.0 : 0.0;
    m_status.batteryPercent = 80 + QRandomGenerator::global()->bounded(20); // 80~100
}

void FakeDevice::setInfo(const QString &name, const QString &ip, int port)
{
    m_status.name = name;
    m_status.ip   = ip;
    m_status.port = port;
    emit statusChanged();
}

void FakeDevice::connectDevice()
{
    if (m_status.connected || m_estopped)
        return;
    m_status.connected   = true;
    m_status.heartbeatOk = true;
    m_status.ready       = true;   // 模拟设备自检即就绪
    m_status.lastUpdateTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_status.packetLossPercent = 0.0;
    m_status.signalStrengthPercent = 90;
    emit statusChanged();
}

void FakeDevice::disconnectDevice()
{
    m_status.connected = false;
    m_status.heartbeatOk = false;
    m_status.ready   = false;
    m_status.armed   = false;
    m_status.running = false;
    m_status.packetLossPercent = 0.0;
    m_status.signalStrengthPercent = 0;
    emit statusChanged();
}

void FakeDevice::arm()
{
    if (!m_status.connected || m_status.fault || m_estopped)
        return;
    m_status.armed = true;
    emit statusChanged();
}

void FakeDevice::start()
{
    if (!m_status.armed || m_estopped)
        return;
    m_status.running = true;
    emit statusChanged();
}

void FakeDevice::stop()
{
    m_status.running = false;
    emit statusChanged();
}

void FakeDevice::reset()
{
    m_status.fault     = false;
    m_status.faultCode = 0;
    m_status.armed     = false;
    m_status.running   = false;
    m_estopped         = false;
    if (m_status.connected) {
        m_status.heartbeatOk = true;
        m_status.ready       = true;
    }
    emit statusChanged();
}

void FakeDevice::emergencyStop()
{
    m_estopped           = true;
    m_status.running     = false;
    m_status.armed       = false;
    m_status.fault       = true;
    m_status.faultCode   = 9999;   // 约定 9999=急停
    m_status.heartbeatOk = true;   // 急停下仍保持心跳，便于上报状态
    emit statusChanged();
}

void FakeDevice::tick(qint64 nowMs)
{
    if (!m_status.connected)
        return;

    // 更新心跳时间与模拟通信延迟（5~30ms 抖动）
    m_status.lastUpdateTimeMs = nowMs;
    m_status.latencyMs = 5 + QRandomGenerator::global()->bounded(26);
    m_status.packetLossPercent = QRandomGenerator::global()->bounded(21) / 10.0;
    m_status.signalStrengthPercent = 75 + QRandomGenerator::global()->bounded(26);

    if (m_status.motionCapable && m_status.running && m_lastTickMs > 0 && nowMs > m_lastTickMs) {
        const double dt = (nowMs - m_lastTickMs) / 1000.0;
        const double yawRad = qDegreesToRadians(m_status.yawDeg);
        m_status.xM += std::cos(yawRad) * m_status.speedMps * dt;
        m_status.yM += std::sin(yawRad) * m_status.speedMps * dt;
        m_status.yawDeg = std::fmod(m_status.yawDeg + 360.0, 360.0);
    }
    m_lastTickMs = nowMs;

    // 电量缓慢下降
    if (QRandomGenerator::global()->bounded(100) < 3 && m_status.batteryPercent > 20)
        m_status.batteryPercent -= 1;

    emit statusChanged();
}
