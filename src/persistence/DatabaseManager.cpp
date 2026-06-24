// =============================================================================
// DatabaseManager.cpp —— 见头文件注释
// =============================================================================

#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString &path)
{
    close();
    clearError();

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return setError(QStringLiteral("Cannot create database directory: %1").arg(info.absolutePath()));

    m_connectionName = QStringLiteral("ADASMaster-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_path = info.absoluteFilePath();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_path);
        if (!db.open())
            return setError(QStringLiteral("Cannot open SQLite database: %1").arg(db.lastError().text()));
    }

    if (!execPragma(QStringLiteral("PRAGMA foreign_keys=ON")) ||
        !execPragma(QStringLiteral("PRAGMA journal_mode=WAL")) ||
        !execPragma(QStringLiteral("PRAGMA busy_timeout=3000")) ||
        !migrate()) {
        return false;
    }

    m_ready = true;
    m_writable = true;
    emit databaseChanged();
    return true;
}

void DatabaseManager::close()
{
    const QString name = m_connectionName;
    if (!name.isEmpty() && QSqlDatabase::contains(name)) {
        {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            if (db.isValid()) db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
    m_connectionName.clear();
    m_ready = false;
    m_writable = false;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool DatabaseManager::execPragma(const QString &statement)
{
    QSqlQuery query(database());
    if (!query.exec(statement))
        return setError(QStringLiteral("SQLite pragma failed: %1").arg(query.lastError().text()));
    return true;
}

bool DatabaseManager::migrate()
{
    QSqlDatabase db = database();
    if (!db.transaction()) return setError(db.lastError().text());

    QSqlQuery query(db);
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS test_runs ("
                       "id TEXT PRIMARY KEY, scenario_name TEXT NOT NULL, scenario_type TEXT NOT NULL, "
                       "started_at_ms INTEGER NOT NULL, finished_at_ms INTEGER, status TEXT NOT NULL, "
                       "result_text TEXT, operator_name TEXT NOT NULL, app_version TEXT NOT NULL, "
                       "config_snapshot TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS samples ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, run_id TEXT NOT NULL REFERENCES test_runs(id) ON DELETE CASCADE, "
                       "timestamp_ms INTEGER NOT NULL, ego_x REAL NOT NULL, ego_y REAL NOT NULL, ego_yaw REAL NOT NULL, "
                       "ego_speed REAL NOT NULL, target_x REAL NOT NULL, target_y REAL NOT NULL, target_yaw REAL NOT NULL, "
                       "target_speed REAL NOT NULL, distance REAL NOT NULL, relative_speed REAL NOT NULL, "
                       "ttc REAL NOT NULL, collision_risk INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_run_time ON samples(run_id, timestamp_ms)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS events ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, run_id TEXT REFERENCES test_runs(id) ON DELETE SET NULL, "
                       "timestamp_ms INTEGER NOT NULL, level INTEGER NOT NULL, source INTEGER NOT NULL, "
                       "title TEXT NOT NULL, message TEXT NOT NULL, acknowledged INTEGER NOT NULL DEFAULT 0, "
                       "recovered INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_time ON events(timestamp_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_run_time ON events(run_id, timestamp_ms)")
    };

    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            db.rollback();
            return setError(QStringLiteral("Schema migration failed: %1").arg(query.lastError().text()));
        }
    }

    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM schema_version")) || !query.next()) {
        db.rollback();
        return setError(query.lastError().text());
    }
    if (query.value(0).toInt() == 0 &&
        !query.exec(QStringLiteral("INSERT INTO schema_version(version) VALUES(1)"))) {
        db.rollback();
        return setError(query.lastError().text());
    }

    if (!db.commit()) return setError(db.lastError().text());
    return true;
}

int DatabaseManager::schemaVersion() const
{
    if (!m_ready) return 0;
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) || !query.next()) return 0;
    return query.value(0).toInt();
}

bool DatabaseManager::createRun(const TestRunRecord &run)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("INSERT INTO test_runs("
                                 "id,scenario_name,scenario_type,started_at_ms,status,operator_name,app_version,config_snapshot) "
                                 "VALUES(?,?,?,?,?,?,?,?)"));
    query.addBindValue(run.id);
    query.addBindValue(run.scenarioName);
    query.addBindValue(run.scenarioType);
    query.addBindValue(run.startedAtMs);
    query.addBindValue(run.status);
    query.addBindValue(run.operatorName);
    query.addBindValue(run.appVersion);
    query.addBindValue(run.configSnapshot);
    if (!query.exec()) return setError(query.lastError().text());
    clearError();
    return true;
}

bool DatabaseManager::appendSamples(const QList<SampleRecord> &samples)
{
    if (samples.isEmpty()) return true;
    QSqlDatabase db = database();
    if (!db.transaction()) return setError(db.lastError().text());

    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO samples("
                                 "run_id,timestamp_ms,ego_x,ego_y,ego_yaw,ego_speed,target_x,target_y,target_yaw,target_speed,"
                                 "distance,relative_speed,ttc,collision_risk) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    for (const SampleRecord &sample : samples) {
        query.bindValue(0, sample.runId);
        query.bindValue(1, sample.timestampMs);
        query.bindValue(2, sample.egoX);
        query.bindValue(3, sample.egoY);
        query.bindValue(4, sample.egoYaw);
        query.bindValue(5, sample.egoSpeed);
        query.bindValue(6, sample.targetX);
        query.bindValue(7, sample.targetY);
        query.bindValue(8, sample.targetYaw);
        query.bindValue(9, sample.targetSpeed);
        query.bindValue(10, sample.distance);
        query.bindValue(11, sample.relativeSpeed);
        query.bindValue(12, sample.ttc);
        query.bindValue(13, sample.collisionRisk ? 1 : 0);
        if (!query.exec()) {
            db.rollback();
            return setError(query.lastError().text());
        }
    }
    if (!db.commit()) return setError(db.lastError().text());
    clearError();
    return true;
}

bool DatabaseManager::finishRun(const QString &runId, const QString &status,
                                const QString &resultText, qint64 finishedAtMs)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE test_runs SET status=?, result_text=?, finished_at_ms=? WHERE id=?"));
    query.addBindValue(status);
    query.addBindValue(resultText);
    query.addBindValue(finishedAtMs);
    query.addBindValue(runId);
    if (!query.exec() || query.numRowsAffected() != 1) return setError(query.lastError().text());
    clearError();
    return true;
}

qint64 DatabaseManager::insertEvent(const QString &runId, qint64 timestampMs, int level, int source,
                                    const QString &title, const QString &message)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("INSERT INTO events(run_id,timestamp_ms,level,source,title,message) "
                                 "VALUES(?,?,?,?,?,?)"));
    if (runId.isEmpty()) query.addBindValue(QVariant()); else query.addBindValue(runId);
    query.addBindValue(timestampMs);
    query.addBindValue(level);
    query.addBindValue(source);
    query.addBindValue(title);
    query.addBindValue(message);
    if (!query.exec()) {
        setError(query.lastError().text());
        return -1;
    }
    clearError();
    return query.lastInsertId().toLongLong();
}

bool DatabaseManager::acknowledgeEvent(qint64 eventId)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE events SET acknowledged=1 WHERE id=?"));
    query.addBindValue(eventId);
    if (!query.exec() || query.numRowsAffected() != 1) return setError(query.lastError().text());
    clearError();
    return true;
}

QList<TestRunSummary> DatabaseManager::listRuns(int limit) const
{
    QList<TestRunSummary> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT id,scenario_name,scenario_type,started_at_ms,"
                                 "COALESCE(finished_at_ms,0),status,COALESCE(result_text,''),operator_name "
                                 "FROM test_runs ORDER BY started_at_ms DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec()) return result;
    while (query.next()) {
        TestRunSummary summary;
        summary.id = query.value(0).toString();
        summary.scenarioName = query.value(1).toString();
        summary.scenarioType = query.value(2).toString();
        summary.startedAtMs = query.value(3).toLongLong();
        summary.finishedAtMs = query.value(4).toLongLong();
        summary.status = query.value(5).toString();
        summary.resultText = query.value(6).toString();
        summary.operatorName = query.value(7).toString();
        result.append(summary);
    }
    return result;
}

int DatabaseManager::sampleCount(const QString &runId) const
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM samples WHERE run_id=?"));
    query.addBindValue(runId);
    if (!query.exec() || !query.next()) return -1;
    return query.value(0).toInt();
}

QList<TestRunSummary> DatabaseManager::listRunsByScenario(const QString &scenarioName, int limit) const
{
    QList<TestRunSummary> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT id,scenario_name,scenario_type,started_at_ms,"
                                 "COALESCE(finished_at_ms,0),status,COALESCE(result_text,''),operator_name "
                                 "FROM test_runs WHERE scenario_name=? ORDER BY started_at_ms DESC LIMIT ?"));
    query.addBindValue(scenarioName);
    query.addBindValue(limit);
    if (!query.exec()) return result;
    while (query.next()) {
        TestRunSummary summary;
        summary.id = query.value(0).toString();
        summary.scenarioName = query.value(1).toString();
        summary.scenarioType = query.value(2).toString();
        summary.startedAtMs = query.value(3).toLongLong();
        summary.finishedAtMs = query.value(4).toLongLong();
        summary.status = query.value(5).toString();
        summary.resultText = query.value(6).toString();
        summary.operatorName = query.value(7).toString();
        result.append(summary);
    }
    return result;
}

QList<SampleRecord> DatabaseManager::listSamples(const QString &runId) const
{
    QList<SampleRecord> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT run_id,timestamp_ms,ego_x,ego_y,ego_yaw,ego_speed,"
                                 "target_x,target_y,target_yaw,target_speed,distance,relative_speed,ttc,collision_risk "
                                 "FROM samples WHERE run_id=? ORDER BY timestamp_ms ASC"));
    query.addBindValue(runId);
    if (!query.exec()) return result;
    while (query.next()) {
        SampleRecord s;
        s.runId = query.value(0).toString();
        s.timestampMs = query.value(1).toLongLong();
        s.egoX = query.value(2).toDouble();
        s.egoY = query.value(3).toDouble();
        s.egoYaw = query.value(4).toDouble();
        s.egoSpeed = query.value(5).toDouble();
        s.targetX = query.value(6).toDouble();
        s.targetY = query.value(7).toDouble();
        s.targetYaw = query.value(8).toDouble();
        s.targetSpeed = query.value(9).toDouble();
        s.distance = query.value(10).toDouble();
        s.relativeSpeed = query.value(11).toDouble();
        s.ttc = query.value(12).toDouble();
        s.collisionRisk = query.value(13).toInt() != 0;
        result.append(s);
    }
    return result;
}

int DatabaseManager::eventCount(const QString &runId) const
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM events WHERE run_id=?"));
    query.addBindValue(runId);
    if (!query.exec() || !query.next()) return -1;
    return query.value(0).toInt();
}

bool DatabaseManager::eventAcknowledged(qint64 eventId) const
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT acknowledged FROM events WHERE id=?"));
    query.addBindValue(eventId);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool DatabaseManager::setError(const QString &message)
{
    m_lastError = message.isEmpty() ? QStringLiteral("SQLite operation failed") : message;
    m_writable = false;
    emit databaseChanged();
    return false;
}

void DatabaseManager::clearError()
{
    m_lastError.clear();
    if (m_ready) m_writable = true;
}
