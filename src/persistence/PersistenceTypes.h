#pragma once

// =============================================================================
// PersistenceTypes.h
// 功能：SQLite 持久化边界使用的测试记录、采样和回放摘要结构。
// =============================================================================

#include <QString>

struct TestRunRecord
{
    QString id;
    QString scenarioName;
    QString scenarioType;
    qint64 startedAtMs = 0;
    QString status;
    QString operatorName;
    QString appVersion;
    QString configSnapshot;
};

struct SampleRecord
{
    QString runId;
    qint64 timestampMs = 0;
    double egoX = 0.0;
    double egoY = 0.0;
    double egoYaw = 0.0;
    double egoSpeed = 0.0;
    double targetX = 0.0;
    double targetY = 0.0;
    double targetYaw = 0.0;
    double targetSpeed = 0.0;
    double distance = 0.0;
    double relativeSpeed = 0.0;
    double ttc = 999.0;
    bool collisionRisk = false;
};

struct TestRunSummary
{
    QString id;
    QString scenarioName;
    QString scenarioType;
    qint64 startedAtMs = 0;
    qint64 finishedAtMs = 0;
    QString status;
    QString resultText;
    QString operatorName;
};
