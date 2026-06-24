// =============================================================================
// StateManager.cpp —— 见头文件注释
// =============================================================================

#include "StateManager.h"

StateManager::StateManager(QObject *parent) : QObject(parent) {}

QString StateManager::safetyText() const
{
    switch (m_safetyLevel) {
        case SafetyLevel::Normal:    return QStringLiteral("正常");
        case SafetyLevel::Warning:   return QStringLiteral("警告");
        case SafetyLevel::Error:     return QStringLiteral("故障");
        case SafetyLevel::Emergency: return QStringLiteral("急停");
    }
    return QStringLiteral("未知");
}

QString StateManager::testStateText() const
{
    switch (m_testState) {
        case TestState::Idle:      return QStringLiteral("空闲");
        case TestState::Checking:  return QStringLiteral("检查中");
        case TestState::Ready:     return QStringLiteral("就绪");
        case TestState::Armed:     return QStringLiteral("预备");
        case TestState::Running:   return QStringLiteral("运行中");
        case TestState::Finished:  return QStringLiteral("完成");
        case TestState::Fault:     return QStringLiteral("故障");
        case TestState::Emergency: return QStringLiteral("急停");
    }
    return QStringLiteral("未知");
}

void StateManager::setSafetyLevel(SafetyLevel l) { m_safetyLevel = l; emit stateChanged(); }
void StateManager::setTestState(TestState s)     { m_testState = s;   emit stateChanged(); }
void StateManager::setScenario(const QString &name, const QString &type)
{ m_scenarioName = name; m_scenarioType = type; emit stateChanged(); }
void StateManager::setProgress(double p) { m_progress = p; emit stateChanged(); }
void StateManager::setCountdown(int c)   { m_countdown = c; emit stateChanged(); }
void StateManager::setResult(const QString &r) { m_result = r; emit stateChanged(); }
void StateManager::setRtk(const QString &mode, int sats, double acc)
{ m_rtkMode = mode; m_satelliteCount = sats; m_positionAccuracy = acc; emit stateChanged(); }
void StateManager::setEgo(double x, double y, double yaw, double speed)
{ m_egoX = x; m_egoY = y; m_egoYaw = yaw; m_egoSpeed = speed; emit stateChanged(); }
void StateManager::setTarget(double x, double y, double yaw, double speed, const QString &type)
{ m_tgtX = x; m_tgtY = y; m_tgtYaw = yaw; m_tgtSpeed = speed; m_targetType = type; emit stateChanged(); }
void StateManager::setRelative(double distance, double relSpeed, double ttc, bool risk)
{ m_distance = distance; m_relativeSpeed = relSpeed; m_ttc = ttc; m_collisionRisk = risk; emit stateChanged(); }
