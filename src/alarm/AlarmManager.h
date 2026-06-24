#pragma once

// =============================================================================
// AlarmManager.h
// 功能：报警/事件的统一入口。系统内任何模块产生的事件都通过 raise() 上报，
//       由本管理器统一存入 AlarmListModel 并广播给界面（底部日志栏、报警中心）。
// 输入：各模块调用 raise(level, source, title, message)。
// 输出：alarmListModel 供报警中心显示；filterModel 提供筛选代理；latestEvent
//       属性驱动底部日志栏；同时通过 qInfo/qWarning 输出到控制台与日志文件。
// =============================================================================

#include <QObject>
#include "EventLog.h"
#include "AlarmListModel.h"
#include "AlarmFilterProxyModel.h"

class DatabaseManager;

class AlarmManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AlarmListModel* alarmListModel READ alarmListModel CONSTANT)
    Q_PROPERTY(AlarmFilterProxyModel* filterModel READ filterModel CONSTANT)
    Q_PROPERTY(QString latestEvent READ latestEvent NOTIFY latestEventChanged)

public:
    explicit AlarmManager(QObject *parent = nullptr);

    AlarmListModel *alarmListModel() const { return m_model; }
    AlarmFilterProxyModel *filterModel() const { return m_filterModel; }
    QString latestEvent() const { return m_latestEvent; }
    void setDatabaseManager(DatabaseManager *database) { m_database = database; }
    void setCurrentRunId(const QString &runId) { m_currentRunId = runId; }

    // 抛出一个事件
    Q_INVOKABLE void raise(AlarmLevel level, AlarmSource source,
                           const QString &title, const QString &message);
    // 便捷重载
    void info(const QString &title, const QString &msg);
    void warning(const QString &title, const QString &msg);
    void error(const QString &title, const QString &msg);
    void emergency(const QString &title, const QString &msg);

    Q_INVOKABLE void acknowledge(int row);
    Q_INVOKABLE void acknowledgeAll();

    // 清空当前内存列表（保留数据库历史）
    Q_INVOKABLE void clear();
    // 导出全部记录为 CSV
    Q_INVOKABLE void exportCsv(const QUrl &url);

signals:
    void latestEventChanged();
    void eventRaised(const EventLog &e);

private:
    AlarmListModel *m_model;
    AlarmFilterProxyModel *m_filterModel;
    QString m_latestEvent;
    DatabaseManager *m_database = nullptr;
    QString m_currentRunId;
};
