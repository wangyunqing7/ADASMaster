// =============================================================================
// AlarmManager.cpp —— 见头文件注释
// =============================================================================

#include "AlarmManager.h"
#include "../persistence/DatabaseManager.h"
#include <QDateTime>
#include <QLoggingCategory>
#include <QUrl>

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent),
      m_model(new AlarmListModel(this)),
      m_filterModel(new AlarmFilterProxyModel(this))
{
    m_filterModel->setSourceModel(m_model);
}

void AlarmManager::raise(AlarmLevel level, AlarmSource source,
                         const QString &title, const QString &message)
{
    EventLog e;
    e.timestampMs = QDateTime::currentMSecsSinceEpoch();
    e.level  = level;
    e.source = source;
    e.title  = title;
    e.message = message;

    // 安全优先：先入内存模型 + 发信号，确保 UI 立即响应、急停告警不被 DB 阻塞。
    // busy_timeout=3s 意味着先落盘会让 Emergency 告警延迟最多 3 秒，对工控安全不可接受。
    m_model->append(e);
    m_latestEvent = QStringLiteral("[%1] %2").arg(e.title).arg(e.message);
    emit latestEventChanged();
    emit eventRaised(e);

    // 同步输出到 Qt 日志（Emergency 用 qCritical 以区别于普通错误）
    switch (level) {
        case AlarmLevel::Info:      qInfo()   << "[信息]" << title << message; break;
        case AlarmLevel::Warning:   qWarning() << "[警告]" << title << message; break;
        case AlarmLevel::Error:     qWarning() << "[错误]" << title << message; break;
        case AlarmLevel::Emergency: qCritical() << "[急停]" << title << message; break;
    }

    // 之后落盘 DB（瞬时失败不影响内存告警的呈现）
    if (m_database && m_database->writable()) {
        const qint64 id = m_database->insertEvent(m_currentRunId, e.timestampMs,
                                                  static_cast<int>(level),
                                                  static_cast<int>(source),
                                                  title, message);
        if (id > 0)
            m_model->setEventDatabaseId(0, id);   // 最新事件在头部 index 0
    }
}

void AlarmManager::info(const QString &title, const QString &msg)
{ raise(AlarmLevel::Info, AlarmSource::System, title, msg); }

void AlarmManager::warning(const QString &title, const QString &msg)
{ raise(AlarmLevel::Warning, AlarmSource::System, title, msg); }

void AlarmManager::error(const QString &title, const QString &msg)
{ raise(AlarmLevel::Error, AlarmSource::System, title, msg); }

void AlarmManager::emergency(const QString &title, const QString &msg)
{ raise(AlarmLevel::Emergency, AlarmSource::Safety, title, msg); }

void AlarmManager::acknowledge(int row)
{
    const EventLog &event = m_model->eventAt(row);
    if (event.databaseId > 0 && m_database &&
        !m_database->acknowledgeEvent(event.databaseId))
        return;
    m_model->acknowledge(row);
}

void AlarmManager::acknowledgeAll()
{
    for (int row = 0; row < m_model->count(); ++row)
        acknowledge(row);
}

void AlarmManager::clear()
{
    m_model->clear();
}

void AlarmManager::exportCsv(const QUrl &url)
{
    m_model->exportToCsv(url);
}
