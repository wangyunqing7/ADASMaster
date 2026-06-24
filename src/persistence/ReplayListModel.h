#pragma once

#include "PersistenceTypes.h"

#include <QAbstractListModel>

class DatabaseManager;

class ReplayListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        RunIdRole = Qt::UserRole + 1,
        StartedAtRole,
        ScenarioNameRole,
        ScenarioTypeRole,
        StatusRole,
        ResultTextRole,
        OperatorRole
    };
    Q_ENUM(Roles)

    explicit ReplayListModel(QObject *parent = nullptr);

    void setDatabaseManager(DatabaseManager *database) { m_database = database; }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_runs.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void filterByScenario(const QString &scenarioName);

signals:
    void countChanged();

private:
    DatabaseManager *m_database = nullptr;
    QList<TestRunSummary> m_runs;
    QString m_scenarioFilter;   // 非空时仅显示该工况的记录
};
