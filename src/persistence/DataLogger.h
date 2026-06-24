#pragma once

#include "PersistenceTypes.h"
#include "../scenario/ScenarioConfig.h"

#include <QObject>
#include <QList>

class DatabaseManager;
class StateManager;

class DataLogger : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY loggingChanged)
    Q_PROPERTY(QString currentRunId READ currentRunId NOTIFY loggingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY loggingChanged)

public:
    explicit DataLogger(QObject *parent = nullptr);

    void setDatabaseManager(DatabaseManager *database) { m_database = database; }
    void setStateManager(StateManager *state) { m_state = state; }

    bool active() const { return m_active; }
    QString currentRunId() const { return m_runId; }
    QString lastError() const { return m_lastError; }

    bool beginRun(const ScenarioConfig &scenario, qint64 startedAtMs = 0);
    bool capture(qint64 timestampMs = 0);
    bool finishRun(const QString &status, const QString &resultText,
                   qint64 finishedAtMs = 0);
    void abandonRun();

signals:
    void loggingChanged();
    void persistenceFailed(const QString &message);

private:
    bool flush();
    bool fail(const QString &message);

    DatabaseManager *m_database = nullptr;
    StateManager *m_state = nullptr;
    QList<SampleRecord> m_buffer;
    QString m_runId;
    QString m_lastError;
    bool m_active = false;
    bool m_failureReported = false;
    static constexpr int BufferSize = 10;
};
