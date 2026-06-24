// =============================================================================
// SafetyManager.cpp —— 见头文件注释
// =============================================================================

#include "SafetyManager.h"

SafetyManager::SafetyManager(QObject *parent) : QObject(parent) {}

QString SafetyManager::levelText() const
{
    switch (m_level) {
        case SafetyLevel::Normal:    return QStringLiteral("正常");
        case SafetyLevel::Warning:   return QStringLiteral("警告");
        case SafetyLevel::Error:     return QStringLiteral("故障");
        case SafetyLevel::Emergency: return QStringLiteral("急停");
    }
    return QStringLiteral("未知");
}

void SafetyManager::triggerSoftwareEStop()
{
    if (m_swEStop) return;
    m_swEStop = true;
    recompute();
    if (m_emergencyActive)
        emit emergencyTriggered(QStringLiteral("软件急停"));
}

void SafetyManager::triggerHardwareEStop()
{
    updateHardwareEStop(true);
}

void SafetyManager::triggerWirelessEStop()
{
    updateWirelessEStop(true);
}

void SafetyManager::updateHardwareEStop(bool active)
{
    if (m_hwEStop == active) return;
    m_hwEStop = active;
    if (active) {
        m_emergencyActive = true;
        m_resetRequired = true;
        m_emergencySource = QStringLiteral("硬件急停");
    }
    recompute();
    if (active) emit emergencyTriggered(QStringLiteral("硬件急停"));
}

void SafetyManager::updateWirelessEStop(bool active)
{
    if (m_wirelessEStop == active) return;
    m_wirelessEStop = active;
    if (active) {
        m_emergencyActive = true;
        m_resetRequired = true;
        m_emergencySource = QStringLiteral("无线急停");
    }
    recompute();
    if (active) emit emergencyTriggered(QStringLiteral("无线急停"));
}

bool SafetyManager::requestReset()
{
    m_resetError.clear();
    if (m_hwEStop || m_wirelessEStop) {
        m_resetError = QStringLiteral("物理急停输入尚未解除");
        emit safetyChanged();
        return false;
    }
    if (!m_commOk) {
        m_resetError = QStringLiteral("必需设备通信尚未恢复");
        emit safetyChanged();
        return false;
    }

    m_swEStop = false;
    m_resetRequired = false;
    m_emergencyActive = false;
    m_emergencySource.clear();
    recompute();
    return true;
}

void SafetyManager::onCommunicationDegraded(bool ok)
{
    m_commOk = ok;
    recompute();
}

void SafetyManager::recompute()
{
    // 急停优先级最高
    if (m_emergencyActive || m_hwEStop || m_swEStop || m_wirelessEStop) {
        m_level = SafetyLevel::Emergency;
        m_emergencyActive = true;
        m_resetRequired = true;
        // 记录首个触发的来源
        if (m_emergencySource.isEmpty()) {
            if (m_hwEStop)      m_emergencySource = QStringLiteral("硬件急停");
            else if (m_swEStop) m_emergencySource = QStringLiteral("软件急停");
            else                m_emergencySource = QStringLiteral("无线急停");
        }
    } else if (!m_commOk) {
        m_level = SafetyLevel::Error;
    } else {
        m_level = SafetyLevel::Normal;
    }
    emit safetyChanged();
}
