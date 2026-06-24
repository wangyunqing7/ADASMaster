// =============================================================================
// ScenarioEngine.cpp —— 见头文件注释
// =============================================================================

#include "ScenarioEngine.h"
#include "../device/DeviceManager.h"
#include "../safety/SafetyManager.h"
#include "../state/StateManager.h"
#include "../alarm/AlarmManager.h"
#include "../persistence/DataLogger.h"
#include "../ncap/NcapEvaluator.h"

#include <QDateTime>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

ScenarioEngine::ScenarioEngine(QObject *parent)
    : QObject(parent)
{
    // 10Hz 模拟时钟：驱动 Running 时的运动学与状态刷新
    m_tickTimer.setInterval(100);
    connect(&m_tickTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastTickMs == 0) m_lastTickMs = now;
        const double dt = (now - m_lastTickMs) / 1000.0;
        m_lastTickMs = now;

        // 倒计时阶段
        if (m_state_machine == TestState::Armed && m_countdown > 0) {
            // 倒计时由独立逻辑处理（见 start() 中的 QTimer）
        }

        // 运行中：推进运动学
        if (m_state_machine == TestState::Running)
            stepSimulation(dt);
    });

    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this]() {
        if (!m_countdownActive || m_state_machine != TestState::Armed) {
            cancelCountdown();
            return;
        }

        --m_countdown;
        if (m_state) m_state->setCountdown(m_countdown);
        if (m_countdown > 0) return;

        m_countdownTimer.stop();
        m_countdownActive = false;
        emit testStateChanged();

        if (!canEnterRunning()) {
            if (m_devices) m_devices->stopAll();
            setState(TestState::Fault);
            if (m_alarm)
                m_alarm->error(QStringLiteral("启动失败"),
                               QStringLiteral("倒计时结束时安全或必需设备状态不满足"));
            return;
        }

        m_devices->startAll();
        if (!m_devices->allRequiredRunning()) {
            m_devices->stopAll();
            setState(TestState::Fault);
            if (m_alarm)
                m_alarm->error(QStringLiteral("启动失败"), QStringLiteral("必需设备未全部进入运行状态"));
            return;
        }

        if (m_logger && !m_logger->beginRun(m_config)) {
            m_devices->stopAll();
            setState(TestState::Fault);
            if (m_alarm)
                m_alarm->error(QStringLiteral("记录启动失败"), m_logger->lastError());
            return;
        }
        if (m_alarm && m_logger)
            m_alarm->setCurrentRunId(m_logger->currentRunId());

        setState(TestState::Running);
        beginSimulation();
        m_tickTimer.start();
        if (m_alarm)
            m_alarm->info(QStringLiteral("开始测试"), QStringLiteral("测试已开始运行"));
    });
}

bool ScenarioEngine::loadScenario(const QString &name)
{
    if (m_state_machine != TestState::Idle || m_countdownActive) return false;

    const auto it = std::find_if(m_scenarios.cbegin(), m_scenarios.cend(),
                                 [&name](const ScenarioConfig &scenario) {
        return scenario.name.compare(name, Qt::CaseInsensitive) == 0;
    });
    if (it == m_scenarios.cend()) return false;
    m_config = *it;

    if (m_state)
        m_state->setScenario(m_config.name, m_config.type);

    if (m_alarm)
        m_alarm->info(QStringLiteral("加载工况"),
                      QStringLiteral("已加载工况：%1（%2）").arg(m_config.name, m_config.type));

    setState(TestState::Idle);
    return true;
}

bool ScenarioEngine::checkDevices()
{
    if (!m_safety || !m_devices || m_config.name.isEmpty()) return false;
    if (m_state_machine != TestState::Idle || m_countdownActive) return false;

    setState(TestState::Checking);
    if (m_alarm)
        m_alarm->info(QStringLiteral("检查设备"), QStringLiteral("正在检查设备、RTK、安全状态..."));

    // 检查安全状态
    if (m_safety->safetyLevel() != SafetyLevel::Normal) {
        if (m_alarm)
            m_alarm->error(QStringLiteral("检查失败"), QStringLiteral("安全状态非正常，无法就绪"));
        setState(TestState::Fault);
        return false;
    }
    // 检查设备在线数（至少 4/5 在线）
    if (!m_devices->allRequiredOnline() || !m_devices->allRequiredReady()) {
        if (m_alarm)
            m_alarm->error(QStringLiteral("检查失败"),
                           QStringLiteral("必需设备异常：%1").arg(m_devices->requiredHealthSummary()));
        setState(TestState::Fault);
        return false;
    }
    // 通过 -> 就绪
    setState(TestState::Ready);
    if (m_alarm)
        m_alarm->info(QStringLiteral("就绪"), QStringLiteral("检查通过，可进入预备"));
    return true;
}

bool ScenarioEngine::arm()
{
    if (m_state_machine != TestState::Ready || m_countdownActive) return false;
    if (!m_safety || !m_safety->allowArm() || !m_devices ||
        !m_devices->allRequiredOnline() || !m_devices->allRequiredReady()) return false;
    setState(TestState::Armed);
    // 预备所有设备
    if (m_devices) m_devices->armAll();
    if (!m_devices->allRequiredArmed()) {
        setState(TestState::Fault);
        return false;
    }
    if (m_alarm)
        m_alarm->info(QStringLiteral("预备"), QStringLiteral("设备已预备，等待开始"));
    return true;
}

bool ScenarioEngine::start()
{
    if (m_state_machine != TestState::Armed || m_countdownActive) return false;
    if (!canEnterRunning()) return false;

    // 无倒计时：直接启动设备并进入 Running
    m_devices->startAll();
    if (!m_devices->allRequiredRunning()) {
        m_devices->stopAll();
        setState(TestState::Fault);
        if (m_alarm)
            m_alarm->error(QStringLiteral("启动失败"), QStringLiteral("必需设备未全部进入运行状态"));
        return false;
    }

    if (m_logger && !m_logger->beginRun(m_config)) {
        m_devices->stopAll();
        setState(TestState::Fault);
        if (m_alarm)
            m_alarm->error(QStringLiteral("记录启动失败"), m_logger->lastError());
        return false;
    }
    if (m_alarm && m_logger)
        m_alarm->setCurrentRunId(m_logger->currentRunId());

    setState(TestState::Running);
    beginSimulation();
    m_tickTimer.start();
    if (m_alarm)
        m_alarm->info(QStringLiteral("开始测试"), QStringLiteral("测试已开始运行"));
    return true;
}

bool ScenarioEngine::stop()
{
    if (m_state_machine != TestState::Running && !m_countdownActive) return false;
    finishTest(QStringLiteral("正常停止"));
    return true;
}

void ScenarioEngine::softwareEmergencyStop()
{
    if (!m_safety) return;
    m_safety->triggerSoftwareEStop();   // 这会触发 onSafetyEmergency
}

void ScenarioEngine::onSafetyEmergency(const QString &source)
{
    cancelCountdown();
    m_tickTimer.stop();
    if (m_devices) m_devices->emergencyStopAll();
    setState(TestState::Emergency);
    // 急停 = 异常中断，NCAP 裁决为未通过（completedNormally=false）
    m_result.completedNormally = false;
    const NcapVerdict verdict = NcapEvaluator::evaluate(m_config, m_result);
    if (m_state)
        m_state->setVerdict(verdict.outcomeText(),
                            static_cast<int>(verdict.verdict),
                            verdict.scorePercent, verdict.detail);
    if (m_alarm)
        m_alarm->emergency(QStringLiteral("急停触发"),
                           QStringLiteral("来源：%1，所有测试流程已停止").arg(source));
    if (m_logger && m_logger->active()) {
        const QString result = QStringLiteral("急停：%1").arg(source);
        if (!m_logger->finishRun(QStringLiteral("EMERGENCY"), result)) {
            handlePersistenceFailure(m_logger->lastError());
            return;
        }
        if (m_alarm) m_alarm->setCurrentRunId(QString());
    }
}

void ScenarioEngine::reset()
{
    if (m_state_machine != TestState::Emergency &&
        m_state_machine != TestState::Fault &&
        m_state_machine != TestState::Finished)
        return;
    // 急停状态复位前先复位安全
    if (m_safety && m_safety->resetRequired())
        if (!m_safety->requestReset()) {
            if (m_alarm) m_alarm->warning(QStringLiteral("复位失败"), m_safety->resetError());
            return;
        }
    if (m_devices) m_devices->resetAll();
    setState(TestState::Idle);
    if (m_state) {
        m_state->setCountdown(0);
        m_state->setProgress(0.0);
        m_state->setResult(QString());
    }
    if (m_alarm)
        m_alarm->info(QStringLiteral("复位"), QStringLiteral("系统已复位至空闲"));
}

void ScenarioEngine::tick(qint64 nowMs)
{
    // 外部统一时钟目前不直接使用；模拟由内部 m_tickTimer 驱动
    Q_UNUSED(nowMs)
}

void ScenarioEngine::setState(TestState s)
{
    m_state_machine = s;
    if (m_state) m_state->setTestState(s);
    emit testStateChanged();
}

void ScenarioEngine::beginSimulation()
{
    // 初始化主车与目标物位姿（沿 X 轴方向行驶的简化模型）
    m_runStartMs = QDateTime::currentMSecsSinceEpoch();
    m_lastTickMs = m_runStartMs;
    m_egoX = m_config.egoStartX; m_egoY = m_config.egoStartY;
    m_egoYaw = m_config.egoStartYaw; m_egoSpeed = 0.0;
    // 目标物起点：优先取第一个航点，无航点时退回 targetStart*
    if (!m_config.targetPath.isEmpty()) {
        const Waypoint &wp0 = m_config.targetPath.first();
        m_tgtX = wp0.x; m_tgtY = wp0.y; m_tgtSpeed = wp0.speed;
        m_tgtYaw = m_config.targetStartYaw;
        m_currentWaypoint = 0;
    } else {
        m_tgtX = m_config.targetStartX; m_tgtY = m_config.targetStartY;
        m_tgtYaw = m_config.targetStartYaw; m_tgtSpeed = m_config.targetSpeed;
        m_currentWaypoint = -1;
    }
    m_simInited = true;

    // 重置 NCAP 评估跟踪状态
    m_collisionDetected = false;
    m_aebDecelMps2 = 0.0;
    m_result = TestResult{};
    m_result.egoTargetSpeed = m_config.egoTargetSpeed;
    m_result.minTtcSec = 999.0;
    m_result.minDistanceM = 999.0;

    if (m_state) {
        m_state->setTarget(m_tgtX, m_tgtY, m_tgtYaw, m_tgtSpeed, m_config.targetType);
        m_state->setVerdict(QString(), 2, 0.0, QString());   // 清空上轮裁决
    }
}

void ScenarioEngine::stepSimulation(double dt)
{
    if (!m_simInited) return;

    // 主车加速至目标速度；若 AEB 已触发则改为制动减速
    const double accel = 3.0; // m/s^2 正常加速
    const bool isAeb = m_config.type.compare(QStringLiteral("AEB"), Qt::CaseInsensitive) == 0;
    if (m_aebDecelMps2 > 0.0) {
        // AEB 制动中：速度递减，最低到 0
        m_egoSpeed = std::max(0.0, m_egoSpeed - m_aebDecelMps2 * dt);
    } else if (m_config.egoTargetSpeed >= 0.0) {
        m_egoSpeed = std::min(m_config.egoTargetSpeed, m_egoSpeed + accel * dt);
    } else {
        m_egoSpeed = std::max(m_config.egoTargetSpeed, m_egoSpeed - accel * dt);
    }
    const double egoRadians = m_egoYaw * 3.14159265358979323846 / 180.0;
    m_egoX += m_egoSpeed * std::cos(egoRadians) * dt;
    m_egoY += m_egoSpeed * std::sin(egoRadians) * dt;

    // 目标物运动：有航点时按航点插值，无航点时保持单点恒速/静止
    if (m_currentWaypoint >= 0 && m_currentWaypoint < m_config.targetPath.size()) {
        const Waypoint &goal = m_config.targetPath.at(m_currentWaypoint);
        const double dxw = goal.x - m_tgtX;
        const double dyw = goal.y - m_tgtY;
        const double distToGoal = std::hypot(dxw, dyw);
        const double move = m_tgtSpeed * dt;

        if (distToGoal <= 0.5 || move >= distToGoal) {
            // 到达当前航点，切换到下一个（或停留）
            m_tgtX = goal.x; m_tgtY = goal.y;
            if (m_currentWaypoint + 1 < m_config.targetPath.size()) {
                ++m_currentWaypoint;
                m_tgtSpeed = m_config.targetPath.at(m_currentWaypoint).speed;
            } else {
                // 最后航点：保持其速度（可能为 0，即静止）
                m_tgtSpeed = goal.speed;
                m_currentWaypoint = m_config.targetPath.size();  // 标记已走完
            }
        } else {
            // 朝航点方向移动
            m_tgtX += dxw / distToGoal * move;
            m_tgtY += dyw / distToGoal * move;
        }
    } else if (m_currentWaypoint < 0) {
        // 单点模式：沿 yaw 方向恒速
        const double targetRadians = m_tgtYaw * 3.14159265358979323846 / 180.0;
        m_tgtX += m_tgtSpeed * std::cos(targetRadians) * dt;
        m_tgtY += m_tgtSpeed * std::sin(targetRadians) * dt;
    }
    // m_currentWaypoint >= size()：已走完所有航点，目标物静止不动

    // ---- NCAP 精确指标计算（替代旧的简化 dist<5 判定）----
    const double dx = m_tgtX - m_egoX;
    const double dy = m_tgtY - m_egoY;
    const double dist = std::hypot(dx, dy);
    // 纵向距离（沿行驶方向 X 轴投影，取主车前方为正）
    const double longitudinal = m_tgtX - m_egoX;
    const double ttc = NcapEvaluator::computeTtc(longitudinal, m_egoSpeed, m_tgtSpeed);
    m_result.minDistanceM = std::min(m_result.minDistanceM, dist);
    if (ttc < m_result.minTtcSec)
        m_result.minTtcSec = ttc;

    // 碰撞检测（NCAP 几何重叠判定）
    const ActorPose egoPose{m_egoX, m_egoY, m_egoYaw};
    const ActorPose tgtPose{m_tgtX, m_tgtY, m_tgtYaw};
    const bool collision = NcapEvaluator::detectsCollision(egoPose, 2.25, tgtPose, 1.8, 1.8);

    // AEB 触发：接近中且 TTC 低于阈值时启动制动（模拟被测车辆的 AEB 响应）
    if (isAeb && m_aebDecelMps2 == 0.0 && ttc < 2.6 && ttc > 0.0 &&
        std::isfinite(ttc) && m_egoSpeed > m_tgtSpeed) {
        m_aebDecelMps2 = 6.0;   // 典型 AEB 制动减速度 ~0.6g
        m_result.aebTriggerSpeedMps = m_egoSpeed;
    }

    if (collision && !m_collisionDetected) {
        m_collisionDetected = true;
        m_result.collisionOccurred = true;
        m_result.speedAtImpactMps = m_egoSpeed;
    }

    const bool isLka = m_config.type.compare(QStringLiteral("LKA"), Qt::CaseInsensitive) == 0;
    const bool risk = !isLka && (collision || (ttc < 1.0 && std::isfinite(ttc)));

    if (m_state) {
        m_state->setEgo(m_egoX, m_egoY, m_egoYaw, m_egoSpeed);
        m_state->setTarget(m_tgtX, m_tgtY, m_tgtYaw, m_tgtSpeed, m_config.targetType);
        m_state->setRelative(dist, m_egoSpeed - m_tgtSpeed, ttc, risk);
    }

    if (m_logger && m_logger->active() &&
        !m_logger->capture(QDateTime::currentMSecsSinceEpoch())) {
        handlePersistenceFailure(m_logger->lastError());
        return;
    }

    // 进度 = 已用时间 / 最大测试时间
    const double elapsed = (QDateTime::currentMSecsSinceEpoch() - m_runStartMs) / 1000.0;
    m_result.durationSec = elapsed;
    if (m_state) m_state->setProgress(std::min(1.0, elapsed / m_config.maxTestTimeSec));

    // 自动完成条件：碰撞、主车完全刹停、或超时
    if (m_collisionDetected) {
        m_tickTimer.stop();
        finishTest(QStringLiteral("碰撞触发：%1 工况").arg(m_config.type));
    } else if (isAeb && m_aebDecelMps2 > 0.0 && m_egoSpeed < 0.1) {
        // AEB 成功刹停，避免碰撞
        m_tickTimer.stop();
        finishTest(QStringLiteral("AEB 成功刹停：%1 工况").arg(m_config.type));
    } else if (elapsed >= m_config.maxTestTimeSec) {
        m_tickTimer.stop();
        finishTest(QStringLiteral("%1 测试完成").arg(m_config.type));
    }
}

void ScenarioEngine::finishTest(const QString &result)
{
    cancelCountdown();
    m_tickTimer.stop();
    if (m_devices) m_devices->stopAll();

    // 正常结束（停止/超时/刹停/碰撞触发）→ 调用 NCAP 评估器给出裁决
    m_result.completedNormally = true;
    const NcapVerdict verdict = NcapEvaluator::evaluate(m_config, m_result);
    const QString fullResult = QStringLiteral("[%1 评分%2%] %2")
        .arg(verdict.outcomeText())
        .arg(verdict.scorePercent, 0, 'f', 0)
        .arg(result);

    if (m_state) {
        m_state->setResult(fullResult);
        m_state->setProgress(1.0);
        m_state->setVerdict(verdict.outcomeText(),
                            static_cast<int>(verdict.verdict),
                            verdict.scorePercent, verdict.detail);
    }
    if (m_alarm) {
        const QString level = verdict.verdict == NcapOutcome::Pass
            ? QStringLiteral("通过") : QStringLiteral("未通过");
        m_alarm->info(QStringLiteral("测试完成（%1）").arg(level),
                      QStringLiteral("%1 · %2").arg(fullResult, verdict.detail));
    }
    if (m_logger && m_logger->active() &&
        !m_logger->finishRun(QStringLiteral("FINISHED"), fullResult)) {
        handlePersistenceFailure(m_logger->lastError());
        return;
    }
    if (m_alarm) m_alarm->setCurrentRunId(QString());
    setState(TestState::Finished);
}

bool ScenarioEngine::canEnterRunning() const
{
    return m_safety && m_safety->allowStart() && m_devices &&
           m_devices->allRequiredOnline() && m_devices->allRequiredReady() &&
           m_devices->allRequiredArmed();
}

void ScenarioEngine::cancelCountdown()
{
    m_countdownTimer.stop();
    if (!m_countdownActive && m_countdown == 0) return;
    m_countdownActive = false;
    m_countdown = 0;
    if (m_state) m_state->setCountdown(0);
    emit testStateChanged();
}

void ScenarioEngine::handlePersistenceFailure(const QString &message)
{
    cancelCountdown();
    m_tickTimer.stop();
    if (m_devices) m_devices->stopAll();
    if (m_logger) m_logger->abandonRun();
    if (m_alarm) m_alarm->setCurrentRunId(QString());
    if (m_state) m_state->setResult(QStringLiteral("数据落盘失败：%1").arg(message));
    if (m_state_machine != TestState::Emergency)
        setState(TestState::Fault);
    if (m_alarm)
        m_alarm->error(QStringLiteral("数据落盘失败"), message);
}
