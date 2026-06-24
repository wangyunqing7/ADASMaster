#pragma once

// =============================================================================
// ScenarioEngine.h
// 功能：场景执行引擎，ADAS 场地测试的核心调度器。负责：
//   1. 加载工况配置
//   2. 推进测试状态机（Idle->Checking->Ready->Armed->Running->Finished）
//   3. 在 Running 时模拟主车/目标物运动学并更新 StateManager
//   4. 处理故障与急停（由 SafetyManager 触发）
//   5. 生成测试结果
// 输入：测试执行页面的按钮（loadScenario/checkDevices/arm/start/stop/softwareEStop）
//       通过 AppController 调用本引擎的 Q_INVOKABLE 方法。
// 输出：通过 StateManager 实时反映状态；通过 AlarmManager 记录关键事件。
//
// 依赖：DeviceManager / SafetyManager / StateManager / AlarmManager
// =============================================================================

#include <QObject>
#include <QTimer>
#include "ScenarioStatus.h"
#include "ScenarioConfig.h"
#include "../ncap/TestResult.h"

class DeviceManager;
class SafetyManager;
class StateManager;
class AlarmManager;
class DataLogger;

class ScenarioEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int     testState   READ testStateInt  NOTIFY testStateChanged)
    Q_PROPERTY(QString scenarioName READ scenarioName NOTIFY testStateChanged)
    Q_PROPERTY(bool    running     READ isRunning     NOTIFY testStateChanged)
    Q_PROPERTY(bool    countdownActive READ countdownActive NOTIFY testStateChanged)

public:
    explicit ScenarioEngine(QObject *parent = nullptr);

    void setDeviceManager(DeviceManager *m) { m_devices = m; }
    void setSafetyManager(SafetyManager *m) { m_safety = m; }
    void setStateManager(StateManager *m)   { m_state  = m; }
    void setAlarmManager(AlarmManager *m)   { m_alarm  = m; }
    void setDataLogger(DataLogger *logger)  { m_logger = logger; }
    void setScenarios(const QList<ScenarioConfig> &scenarios) { m_scenarios = scenarios; }

    TestState testState() const { return m_state_machine; }
    int testStateInt() const { return static_cast<int>(m_state_machine); }
    QString scenarioName() const { return m_config.name; }
    bool isRunning() const { return m_state_machine == TestState::Running; }
    bool countdownActive() const { return m_countdownActive; }

    // 当前加载工况的目标物航点路径（供 QML 预览与测试读取；未加载时返回空）
    int currentTargetWaypointCount() const { return m_config.targetPath.size(); }
    Waypoint currentWaypoint(int index) const { return m_config.targetPath.value(index); }
    const QList<Waypoint> &currentTargetPath() const { return m_config.targetPath; }

public slots:
    // ---- QML 可调用操作（通过 AppController 转发）----
    Q_INVOKABLE bool loadScenario(const QString &name);   // 加载工况
    Q_INVOKABLE bool checkDevices();                      // 检查设备/RTK/安全 -> Ready
    Q_INVOKABLE bool arm();                               // 预备 -> Armed
    Q_INVOKABLE bool start();                             // 开始 -> 倒计时 -> Running
    Q_INVOKABLE bool stop();                              // 停止 -> Finished
    Q_INVOKABLE void softwareEmergencyStop();             // 软件急停 -> Emergency
    Q_INVOKABLE void reset();                             // 复位 -> Idle

    // 由 SafetyManager 急停触发时调用
    void onSafetyEmergency(const QString &source);
    // 统一时钟驱动（由 AppController 调用，10Hz）
    void tick(qint64 nowMs);

signals:
    void testStateChanged();

private:
    void setState(TestState s);
    void beginSimulation();        // 进入 Running 时初始化运动学
    void stepSimulation(double dt);// 每帧推进主车/目标物
    void finishTest(const QString &result);
    bool canEnterRunning() const;
    void cancelCountdown();
    void handlePersistenceFailure(const QString &message);

    DeviceManager *m_devices = nullptr;
    SafetyManager *m_safety  = nullptr;
    StateManager  *m_state   = nullptr;
    AlarmManager  *m_alarm   = nullptr;
    DataLogger    *m_logger  = nullptr;

    TestState      m_state_machine = TestState::Idle;
    ScenarioConfig m_config;
    QList<ScenarioConfig> m_scenarios;

    // 模拟运动学
    QTimer m_tickTimer;           // 10Hz 模拟时钟
    QTimer m_countdownTimer;
    qint64 m_lastTickMs = 0;
    qint64 m_runStartMs = 0;
    int    m_countdown = 0;       // 倒计时秒数
    bool   m_countdownActive = false;

    // 主车/目标物当前位姿与速度（模拟）
    double m_egoX = 0, m_egoY = 0, m_egoYaw = 0, m_egoSpeed = 0;
    double m_tgtX = 0, m_tgtY = 0, m_tgtYaw = 0, m_tgtSpeed = 0;
    bool   m_simInited = false;
    int    m_currentWaypoint = -1;     // 当前正在前往的航点索引；-1 表示单点模式

    // NCAP 评估跟踪（运行中累积，结束时交 NcapEvaluator 评定）
    TestResult m_result;
    bool   m_collisionDetected = false;     // 本轮是否已检测到碰撞（避免重复触发）
    double m_aebDecelMps2 = 0.0;            // AEB 触发后的制动减速度（>0 表示已触发）
};
