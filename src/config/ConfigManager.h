#pragma once

// =============================================================================
// ConfigManager.h
// 功能：统一解析应用的设备与工况 JSON，向业务层提供只读强类型配置。
// =============================================================================

#include "DeviceConfig.h"
#include "../scenario/ScenarioConfig.h"

#include <QObject>
#include <QList>

class ConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY configChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY configChanged)

public:
    explicit ConfigManager(const QString &dataRoot = QString(), QObject *parent = nullptr);

    bool load(QString *error = nullptr);
    bool ready() const { return m_ready; }
    QString lastError() const { return m_lastError; }
    QString dataRoot() const { return m_dataRoot; }

    const QList<DeviceConfig> &devices() const { return m_devices; }
    const QList<ScenarioConfig> &scenarios() const { return m_scenarios; }
    const ScenarioConfig *scenarioByName(const QString &name) const;
    bool setScenarioPath(const QString &name, bool egoActor, const QList<Waypoint> &path);
    // 运行时添加工况（内存，不持久化到 JSON）
    void addScenario(const ScenarioConfig &scenario) { m_scenarios.append(scenario); emit configChanged(); }

signals:
    void configChanged();

private:
    QString chooseRoot() const;
    bool loadDevices(const QString &path, QString *error);
    bool loadScenarios(const QString &directory, QString *error);
    void fail(const QString &message, QString *error);

    QString m_requestedRoot;
    QString m_dataRoot;
    QList<DeviceConfig> m_devices;
    QList<ScenarioConfig> m_scenarios;
    bool m_ready = false;
    QString m_lastError;
};
