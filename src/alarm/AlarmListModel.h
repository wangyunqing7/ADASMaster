#pragma once

// =============================================================================
// AlarmListModel.h
// 功能：以 QAbstractListModel 形式把报警/事件列表暴露给 QML（报警中心页面）。
//       并提供按等级统计（全量，不受筛选影响）、清空内存列表、导出 CSV。
// 输入：AlarmManager::raise() 追加新事件。
// 输出：QML 通过 role 访问时间/等级/来源/内容/确认/恢复；
//       统计属性（count/unackedCount/各等级数）供概览卡绑定；
//       clear()/exportToCsv() 供"清空"与"导出"按钮调用。
// 说明：采用预分配容量 + 循环覆盖策略，避免列表无限增长（默认保留最近 1000 条）。
// =============================================================================

#include <QAbstractListModel>
#include <QUrl>
#include "EventLog.h"

class AlarmListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int unackedCount READ unackedCount NOTIFY statsChanged)
    Q_PROPERTY(int infoCount READ infoCount NOTIFY statsChanged)
    Q_PROPERTY(int warnCount READ warnCount NOTIFY statsChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY statsChanged)
    Q_PROPERTY(int emergencyCount READ emergencyCount NOTIFY statsChanged)

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        LevelRole,
        SourceRole,
        TitleRole,
        MessageRole,
        AckedRole,
        RecoveredRole
    };
    Q_ENUM(Roles)

    explicit AlarmListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const EventLog &e);          // 追加（新事件插在头部）
    void acknowledge(int row);               // 确认指定行
    void acknowledgeAll();                   // 确认全部
    const EventLog &eventAt(int row) const;
    int  count() const;

    // 清空内存列表（不触碰数据库历史），供"清空"按钮调用
    Q_INVOKABLE void clear();
    // 导出全部记录为 CSV（UTF-8 BOM，Excel 友好），成功返回 true
    Q_INVOKABLE bool exportToCsv(const QUrl &fileUrl);

    // 统计访问器（全量，不受代理筛选影响）
    int unackedCount() const   { return m_unacked; }
    int infoCount() const      { return m_info; }
    int warnCount() const      { return m_warn; }
    int errorCount() const     { return m_error; }
    int emergencyCount() const { return m_emergency; }

signals:
    void countChanged();
    void statsChanged();

private:
    void recomputeStats();   // 依据 m_events 重算各统计，变化则 emit statsChanged

    QList<EventLog> m_events;   // 头部为最新
    int m_capacity = 1000;

    int m_unacked = 0;
    int m_info = 0;
    int m_warn = 0;
    int m_error = 0;
    int m_emergency = 0;
};
