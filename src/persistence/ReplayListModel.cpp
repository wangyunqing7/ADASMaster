#include "ReplayListModel.h"

#include "DatabaseManager.h"

#include <QDateTime>

ReplayListModel::ReplayListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ReplayListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_runs.size();
}

QVariant ReplayListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_runs.size())
        return {};
    const TestRunSummary &run = m_runs.at(index.row());
    switch (role) {
    case RunIdRole: return run.id;
    case StartedAtRole:
        return QDateTime::fromMSecsSinceEpoch(run.startedAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case ScenarioNameRole: return run.scenarioName;
    case ScenarioTypeRole: return run.scenarioType;
    case StatusRole: return run.status;
    case ResultTextRole: return run.resultText;
    case OperatorRole: return run.operatorName;
    }
    return {};
}

QHash<int, QByteArray> ReplayListModel::roleNames() const
{
    return {
        {RunIdRole, "runId"},
        {StartedAtRole, "startedAt"},
        {ScenarioNameRole, "scenarioName"},
        {ScenarioTypeRole, "scenarioType"},
        {StatusRole, "runStatus"},
        {ResultTextRole, "resultText"},
        {OperatorRole, "operatorName"}
    };
}

void ReplayListModel::refresh()
{
    beginResetModel();
    if (m_database && m_database->ready()) {
        m_runs = m_scenarioFilter.isEmpty()
                     ? m_database->listRuns()
                     : m_database->listRunsByScenario(m_scenarioFilter);
    } else {
        m_runs.clear();
    }
    endResetModel();
    emit countChanged();
}

void ReplayListModel::filterByScenario(const QString &scenarioName)
{
    m_scenarioFilter = scenarioName;
    refresh();
}
