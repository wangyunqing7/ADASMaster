#include "DataLogger.h"

#include "DatabaseManager.h"
#include "../state/StateManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>

DataLogger::DataLogger(QObject *parent)
    : QObject(parent)
{
}

bool DataLogger::beginRun(const ScenarioConfig &scenario, qint64 startedAtMs)
{
    if (m_active)
        return fail(QStringLiteral("A test run is already being recorded"));
    if (!m_database || !m_database->writable() || !m_state)
        return fail(QStringLiteral("SQLite database or state source is unavailable"));

    const qint64 started = startedAtMs > 0 ? startedAtMs : QDateTime::currentMSecsSinceEpoch();
    TestRunRecord run;
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.scenarioName = scenario.name;
    run.scenarioType = scenario.type;
    run.startedAtMs = started;
    run.status = QStringLiteral("RUNNING");
    // 审计修复：使用当前登录操作员而非硬编码 "default"
    run.operatorName = m_operatorName.isEmpty() ? QStringLiteral("default") : m_operatorName;
    run.appVersion = QCoreApplication::applicationVersion().isEmpty()
                         ? QStringLiteral("0.1.0")
                         : QCoreApplication::applicationVersion();
    run.configSnapshot = scenario.rawJson.isEmpty() ? QStringLiteral("{}") : scenario.rawJson;

    if (!m_database->createRun(run))
        return fail(m_database->lastError());

    m_buffer.clear();
    m_runId = run.id;
    m_lastError.clear();
    m_failureReported = false;
    m_active = true;
    emit loggingChanged();
    return true;
}

bool DataLogger::capture(qint64 timestampMs)
{
    if (!m_active || !m_state)
        return false;

    SampleRecord sample;
    sample.runId = m_runId;
    sample.timestampMs = timestampMs > 0 ? timestampMs : QDateTime::currentMSecsSinceEpoch();
    sample.egoX = m_state->egoX();
    sample.egoY = m_state->egoY();
    sample.egoYaw = m_state->egoYaw();
    sample.egoSpeed = m_state->egoSpeed();
    sample.targetX = m_state->tgtX();
    sample.targetY = m_state->tgtY();
    sample.targetYaw = m_state->tgtYaw();
    sample.targetSpeed = m_state->tgtSpeed();
    sample.distance = m_state->distance();
    sample.relativeSpeed = m_state->relativeSpeed();
    sample.ttc = m_state->ttc();
    sample.collisionRisk = m_state->collisionRisk();
    m_buffer.append(sample);

    return m_buffer.size() < BufferSize || flush();
}

bool DataLogger::finishRun(const QString &status, const QString &resultText, qint64 finishedAtMs)
{
    if (!m_active)
        return false;
    if (!flush())
        return false;

    const qint64 finished = finishedAtMs > 0 ? finishedAtMs : QDateTime::currentMSecsSinceEpoch();
    if (!m_database->finishRun(m_runId, status, resultText, finished))
        return fail(m_database->lastError());

    m_active = false;
    m_runId.clear();
    emit loggingChanged();
    return true;
}

void DataLogger::abandonRun()
{
    m_buffer.clear();
    // 修复"孤儿 run"：将 DB 中仍为 RUNNING 的记录标记为 ABANDONED，避免永久残留
    if (m_database && m_database->writable() && !m_runId.isEmpty())
        m_database->finishRun(m_runId, QStringLiteral("ABANDONED"),
                              QStringLiteral("运行被放弃（异常终止）"),
                              QDateTime::currentMSecsSinceEpoch());
    m_active = false;
    m_runId.clear();
    emit loggingChanged();
}

bool DataLogger::flush()
{
    if (m_buffer.isEmpty())
        return true;
    if (!m_database || !m_database->appendSamples(m_buffer)) {
        // 修复缓冲区无限增长 OOM：落盘失败时丢弃缓冲，避免随测试时长内存爆涨。
        // 数据丢失已通过 fail() 上报，宁可丢采样也不能让进程 OOM。
        m_buffer.clear();
        return fail(m_database ? m_database->lastError()
                               : QStringLiteral("SQLite database is unavailable"));
    }
    m_buffer.clear();
    return true;
}

bool DataLogger::fail(const QString &message)
{
    m_lastError = message.isEmpty() ? QStringLiteral("Data persistence failed") : message;
    emit loggingChanged();
    if (!m_failureReported) {
        m_failureReported = true;
        emit persistenceFailed(m_lastError);
    }
    return false;
}
