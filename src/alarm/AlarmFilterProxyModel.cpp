// =============================================================================
// AlarmFilterProxyModel.cpp —— 见头文件注释
// =============================================================================

#include "AlarmFilterProxyModel.h"
#include "AlarmListModel.h"

AlarmFilterProxyModel::AlarmFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

void AlarmFilterProxyModel::setActiveLevel(int level)
{
    if (m_activeLevel == level) return;
    m_activeLevel = level;
    emit activeLevelChanged();
    invalidateFilter();
}

void AlarmFilterProxyModel::setActiveSource(int source)
{
    if (m_activeSource == source) return;
    m_activeSource = source;
    emit activeSourceChanged();
    invalidateFilter();
}

void AlarmFilterProxyModel::setOnlyUnacked(bool on)
{
    if (m_onlyUnacked == on) return;
    m_onlyUnacked = on;
    emit onlyUnackedChanged();
    invalidateFilter();
}

void AlarmFilterProxyModel::setKeyword(const QString &kw)
{
    const QString trimmed = kw.trimmed();
    if (m_keyword == trimmed) return;
    m_keyword = trimmed;
    emit keywordChanged();
    invalidateFilter();
}

int AlarmFilterProxyModel::mapRowToSource(int proxyRow) const
{
    const QModelIndex src = mapToSource(index(proxyRow, 0));
    return src.isValid() ? src.row() : -1;
}

bool AlarmFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *src = sourceModel();
    if (!src) return true;
    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (m_activeLevel >= 0) {
        if (src->data(idx, AlarmListModel::LevelRole).toInt() != m_activeLevel)
            return false;
    }
    if (m_activeSource >= 0) {
        if (src->data(idx, AlarmListModel::SourceRole).toInt() != m_activeSource)
            return false;
    }
    if (m_onlyUnacked) {
        if (src->data(idx, AlarmListModel::AckedRole).toBool())
            return false;
    }
    if (!m_keyword.isEmpty()) {
        const QString title = src->data(idx, AlarmListModel::TitleRole).toString();
        const QString msg   = src->data(idx, AlarmListModel::MessageRole).toString();
        if (!title.contains(m_keyword, Qt::CaseInsensitive) &&
            !msg.contains(m_keyword, Qt::CaseInsensitive))
            return false;
    }
    return true;
}
