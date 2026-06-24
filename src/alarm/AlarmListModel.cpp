// =============================================================================
// AlarmListModel.cpp —— 见头文件注释
// =============================================================================

#include "AlarmListModel.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStringConverter>

namespace {
// CSV 字段转义：内部双引号加倍，整体用双引号包裹
QString quoteCsv(const QString &s)
{
    QString out = s;
    out.replace('"', "\"\"");
    return '"' + out + '"';
}
// 等级枚举 -> 中文文字（与界面徽标一致）
QString levelText(AlarmLevel lv)
{
    switch (lv) {
        case AlarmLevel::Info:      return QStringLiteral("信息");
        case AlarmLevel::Warning:   return QStringLiteral("警告");
        case AlarmLevel::Error:     return QStringLiteral("错误");
        case AlarmLevel::Emergency: return QStringLiteral("急停");
    }
    return QStringLiteral("信息");
}
// 来源枚举 -> 中文文字
QString sourceText(AlarmSource src)
{
    switch (src) {
        case AlarmSource::System:        return QStringLiteral("系统");
        case AlarmSource::Safety:        return QStringLiteral("安全");
        case AlarmSource::Device:        return QStringLiteral("设备");
        case AlarmSource::Gnss:          return QStringLiteral("RTK");
        case AlarmSource::Scenario:      return QStringLiteral("工况");
        case AlarmSource::Communication: return QStringLiteral("通信");
    }
    return QStringLiteral("系统");
}
} // namespace

AlarmListModel::AlarmListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int AlarmListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_events.size();
}

QVariant AlarmListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_events.size())
        return {};
    const EventLog &e = m_events.at(index.row());
    switch (role) {
        case TimeRole:     return e.timestampMs;
        case LevelRole:    return static_cast<int>(e.level);
        case SourceRole:   return static_cast<int>(e.source);
        case TitleRole:    return e.title;
        case MessageRole:  return e.message;
        case AckedRole:    return e.acknowledged;
        case RecoveredRole:return e.recovered;
    }
    return {};
}

QHash<int, QByteArray> AlarmListModel::roleNames() const
{
    return {
        {TimeRole,      "alarmTime"},
        {LevelRole,     "alarmLevel"},
        {SourceRole,    "alarmSource"},
        {TitleRole,     "alarmTitle"},
        {MessageRole,   "alarmMessage"},
        {AckedRole,     "alarmAcked"},
        {RecoveredRole, "alarmRecovered"}
    };
}

void AlarmListModel::append(const EventLog &e)
{
    if (m_events.size() >= m_capacity) {
        const int last = m_events.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_events.removeLast();
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), 0, 0);
    m_events.prepend(e);
    endInsertRows();
    emit countChanged();
    recomputeStats();
}

void AlarmListModel::acknowledge(int row)
{
    if (row < 0 || row >= m_events.size()) return;
    m_events[row].acknowledged = true;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {AckedRole});
    recomputeStats();
}

void AlarmListModel::acknowledgeAll()
{
    for (int i = 0; i < m_events.size(); ++i)
        m_events[i].acknowledged = true;
    if (!m_events.isEmpty())
        emit dataChanged(index(0), index(m_events.size() - 1), {AckedRole});
    recomputeStats();
}

int AlarmListModel::count() const { return m_events.size(); }

const EventLog &AlarmListModel::eventAt(int row) const
{
    static const EventLog invalid;
    return row >= 0 && row < m_events.size() ? m_events.at(row) : invalid;
}

void AlarmListModel::clear()
{
    if (m_events.isEmpty()) {
        recomputeStats();   // 兜底：确保统计归零
        return;
    }
    beginResetModel();
    m_events.clear();
    endResetModel();
    emit countChanged();
    recomputeStats();
}

bool AlarmListModel::exportToCsv(const QUrl &fileUrl)
{
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);   // UTF-8 BOM，Excel 打开不乱码

    out << quoteCsv(QStringLiteral("时间")) << ","
        << quoteCsv(QStringLiteral("等级")) << ","
        << quoteCsv(QStringLiteral("来源")) << ","
        << quoteCsv(QStringLiteral("标题")) << ","
        << quoteCsv(QStringLiteral("内容")) << ","
        << quoteCsv(QStringLiteral("是否确认")) << ","
        << quoteCsv(QStringLiteral("是否恢复")) << "\n";

    for (const EventLog &e : m_events) {
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(e.timestampMs);
        out << quoteCsv(dt.toString("yyyy-MM-dd HH:mm:ss")) << ","
            << quoteCsv(levelText(e.level)) << ","
            << quoteCsv(sourceText(e.source)) << ","
            << quoteCsv(e.title) << ","
            << quoteCsv(e.message) << ","
            << (e.acknowledged ? QStringLiteral("是") : QStringLiteral("否")) << ","
            << (e.recovered ? QStringLiteral("是") : QStringLiteral("否")) << "\n";
    }
    return true;
}

void AlarmListModel::recomputeStats()
{
    int unacked = 0, info = 0, warn = 0, err = 0, emer = 0;
    for (const EventLog &e : m_events) {
        if (!e.acknowledged) ++unacked;
        switch (e.level) {
            case AlarmLevel::Info:      ++info; break;
            case AlarmLevel::Warning:   ++warn; break;
            case AlarmLevel::Error:     ++err;  break;
            case AlarmLevel::Emergency: ++emer; break;
        }
    }
    const bool changed = (unacked != m_unacked || info != m_info || warn != m_warn ||
                          err != m_error || emer != m_emergency);
    m_unacked = unacked;
    m_info = info;
    m_warn = warn;
    m_error = err;
    m_emergency = emer;
    if (changed) emit statsChanged();
}
