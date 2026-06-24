#pragma once

// =============================================================================
// EventLog.h
// 功能：定义报警/事件记录的数据结构，覆盖信息、警告、错误、急停四个等级。
// 输入：由各模块（SafetyManager/ScenarioEngine/DeviceManager 等）通过
//       AlarmManager::raise() 产生。
// 输出：由 AlarmListModel 暴露给报警中心页面，并写入事件日志文件。
// =============================================================================

#include <QObject>
#include <QDateTime>

// 报警等级，颜色约定：Info=蓝/灰, Warning=黄, Error=橙, Emergency=红
enum class AlarmLevel {
    Info      = 0,
    Warning   = 1,
    Error     = 2,
    Emergency = 3
};

// 报警来源分类，便于筛选与统计
enum class AlarmSource {
    System,            // 系统
    Safety,            // 安全系统
    Device,            // 设备
    Gnss,              // RTK/GNSS
    Scenario,          // 工况/测试流程
    Communication      // 通信
};

class EventLog
{
    Q_GADGET
public:
    EventLog() = default;

    qint64       databaseId = -1;    // SQLite events.id; -1 表示尚未持久化
    qint64       timestampMs = 0;   // 发生时间（毫秒）
    AlarmLevel   level   = AlarmLevel::Info;   // 等级
    AlarmSource  source  = AlarmSource::System; // 来源
    QString      title;             // 简短标题
    QString      message;           // 详细内容
    bool         acknowledged = false; // 是否已确认
    bool         recovered    = false; // 是否已恢复

    Q_PROPERTY(qint64      timestampMs  MEMBER timestampMs)
    Q_PROPERTY(int         level        READ levelInt)
    Q_PROPERTY(QString     title        MEMBER title)
    Q_PROPERTY(QString     message      MEMBER message)
    Q_PROPERTY(bool        acknowledged MEMBER acknowledged)
    Q_PROPERTY(bool        recovered    MEMBER recovered)

    int levelInt() const { return static_cast<int>(level); }
};
Q_DECLARE_METATYPE(EventLog)
