#pragma once

// =============================================================================
// SafetyManager.h
// 功能：集中管理 ADAS 场地测试的安全状态，是整个系统的安全大脑。
// 输入：接收硬件急停、软件急停、无线急停信号；由 ScenarioEngine 查询 allowArm/
//       allowStart；由通信层上报超时。
// 输出：QML 通过 safetyLevel/emergencyStopActive 等属性驱动顶部状态栏；
//       ScenarioEngine 在每次状态推进前检查是否允许操作。
//
// 重要原则：
//   - 软件急停只是软件层停止指令，不能替代独立硬件急停系统。
//   - 急停一旦激活，必须复位后才能恢复，且复位会清空软件急停标志。
//   - 任何急停都会立刻把 safetyLevel 置为 Emergency 并锁定操作。
// =============================================================================

#include <QObject>
#include "SafetyStatus.h"

class SafetyManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int  safetyLevel         READ safetyLevelInt        NOTIFY safetyChanged)
    Q_PROPERTY(bool emergencyStopActive READ emergencyStopActive   NOTIFY safetyChanged)
    Q_PROPERTY(QString emergencySource  READ emergencySource       NOTIFY safetyChanged)
    Q_PROPERTY(bool hardwareEStop       READ hardwareEStop         NOTIFY safetyChanged)
    Q_PROPERTY(bool softwareEStop       READ softwareEStop         NOTIFY safetyChanged)
    Q_PROPERTY(bool wirelessEStop       READ wirelessEStop         NOTIFY safetyChanged)
    Q_PROPERTY(bool resetRequired       READ resetRequired         NOTIFY safetyChanged)
    Q_PROPERTY(bool allowArm            READ allowArm              NOTIFY safetyChanged)
    Q_PROPERTY(bool allowStart          READ allowStart            NOTIFY safetyChanged)
    Q_PROPERTY(QString levelText        READ levelText             NOTIFY safetyChanged)
    Q_PROPERTY(QString resetError       READ resetError            NOTIFY safetyChanged)

public:
    explicit SafetyManager(QObject *parent = nullptr);

    SafetyLevel safetyLevel() const { return m_level; }
    int  safetyLevelInt() const { return static_cast<int>(m_level); }
    bool emergencyStopActive() const { return m_emergencyActive; }
    QString emergencySource() const { return m_emergencySource; }
    bool hardwareEStop() const { return m_hwEStop; }
    bool softwareEStop() const { return m_swEStop; }
    bool wirelessEStop() const { return m_wirelessEStop; }
    bool resetRequired() const { return m_resetRequired; }

    // 仅当安全正常时允许预备/开始
    bool allowArm()   const { return m_level == SafetyLevel::Normal; }
    bool allowStart() const { return m_level == SafetyLevel::Normal; }

    // 中文文案，供界面直接显示
    QString levelText() const;
    QString resetError() const { return m_resetError; }

public slots:
    // ---- 急停触发源 ----
    void triggerSoftwareEStop();   // 软件急停（界面按钮）
    void triggerHardwareEStop();   // 硬件急停（由安全系统上报）
    void triggerWirelessEStop();   // 无线急停（由无线接收器上报）
    void updateHardwareEStop(bool active);
    void updateWirelessEStop(bool active);

    // ---- 复位 ----
    // 复位前提：急停源已物理解除。第一阶段模拟为直接可复位。
    bool requestReset();

    // 由设备通信层上报：通信异常导致安全降级
    void onCommunicationDegraded(bool ok);

signals:
    void safetyChanged();
    void emergencyTriggered(const QString &source); // 供 AlarmManager 记录

private:
    void recompute();   // 根据各急停源重新计算 level/active

    SafetyLevel m_level = SafetyLevel::Normal;
    bool m_emergencyActive = false;
    QString m_emergencySource;
    bool m_hwEStop = false;
    bool m_swEStop = false;
    bool m_wirelessEStop = false;
    bool m_resetRequired = false;
    bool m_commOk = true;     // 通信整体是否正常
    QString m_resetError;
};
