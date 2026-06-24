#pragma once

// =============================================================================
// DatabaseManager.h
// 功能：管理 QSQLITE 连接、schema 迁移及测试/采样/事件的事务读写。
// =============================================================================

#include "PersistenceTypes.h"

#include <QObject>
#include <QList>

class QSqlDatabase;

class DatabaseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY databaseChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY databaseChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY databaseChanged)

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool open(const QString &path);
    void close();

    bool ready() const { return m_ready; }
    bool writable() const { return m_ready && m_writable; }
    QString lastError() const { return m_lastError; }
    QString databasePath() const { return m_path; }
    int schemaVersion() const;

    bool createRun(const TestRunRecord &run);
    bool appendSamples(const QList<SampleRecord> &samples);
    bool finishRun(const QString &runId, const QString &status,
                   const QString &resultText, qint64 finishedAtMs);
    qint64 insertEvent(const QString &runId, qint64 timestampMs, int level, int source,
                       const QString &title, const QString &message);
    bool acknowledgeEvent(qint64 eventId);

    QList<TestRunSummary> listRuns(int limit = 200) const;
    QList<TestRunSummary> listRunsByScenario(const QString &scenarioName, int limit = 200) const;
    QList<SampleRecord> listSamples(const QString &runId) const;
    int sampleCount(const QString &runId) const;
    int eventCount(const QString &runId) const;
    bool eventAcknowledged(qint64 eventId) const;

signals:
    void databaseChanged();

private:
    QSqlDatabase database() const;
    bool migrate();
    bool execPragma(const QString &statement);
    bool setError(const QString &message);        // 致命错误：锁死写链路
    bool transientError(const QString &message);  // 瞬时错误：仅记录，不影响 writable
    void clearError();

    QString m_connectionName;
    QString m_path;
    QString m_lastError;
    bool m_ready = false;
    bool m_writable = false;
};
