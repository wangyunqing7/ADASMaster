#pragma once

#include "../alarm/AlarmManager.h"
#include "../auth/AuthManager.h"
#include "../config/ConfigManager.h"
#include "../device/DeviceManager.h"
#include "../persistence/DataLogger.h"
#include "../persistence/DatabaseManager.h"
#include "../persistence/ReplayListModel.h"
#include "../safety/SafetyManager.h"
#include "../scenario/ScenarioEngine.h"
#include "../scenario/ScenarioCatalog.h"
#include "../state/StateManager.h"

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConfigManager* configManager READ configManager CONSTANT)
    Q_PROPERTY(AuthManager* authManager READ authManager CONSTANT)
    Q_PROPERTY(DatabaseManager* databaseManager READ databaseManager CONSTANT)
    Q_PROPERTY(DeviceManager* deviceManager READ deviceManager CONSTANT)
    Q_PROPERTY(SafetyManager* safetyManager READ safetyManager CONSTANT)
    Q_PROPERTY(ScenarioEngine* scenarioEngine READ scenarioEngine CONSTANT)
    Q_PROPERTY(AlarmManager* alarmManager READ alarmManager CONSTANT)
    Q_PROPERTY(StateManager* stateManager READ stateManager CONSTANT)
    Q_PROPERTY(DataLogger* dataLogger READ dataLogger CONSTANT)
    Q_PROPERTY(ReplayListModel* replayListModel READ replayListModel CONSTANT)
    Q_PROPERTY(bool operationalReady READ operationalReady NOTIFY readinessChanged)
    Q_PROPERTY(QString startupError READ startupError NOTIFY readinessChanged)
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY clockTick)

public:
    explicit AppController(const QString &dataRoot = QString(),
                           const QString &storageRoot = QString(),
                           QObject *parent = nullptr);

    ConfigManager *configManager() const { return m_config; }
    AuthManager *authManager() const { return m_auth; }
    DatabaseManager *databaseManager() const { return m_database; }
    DeviceManager *deviceManager() const { return m_device; }
    SafetyManager *safetyManager() const { return m_safety; }
    ScenarioEngine *scenarioEngine() const { return m_scenario; }
    AlarmManager *alarmManager() const { return m_alarm; }
    StateManager *stateManager() const { return m_state; }
    DataLogger *dataLogger() const { return m_logger; }
    ReplayListModel *replayListModel() const { return m_replay; }

    bool operationalReady() const;
    QString startupError() const { return m_startupError; }
    QString currentTime() const { return m_currentTime; }

    Q_INVOKABLE void refreshClock();
    Q_INVOKABLE void startup();

    // 工况列表（名称数组，供 QML 工况选择下拉与工况管理列表）
    Q_INVOKABLE QStringList scenarioNames() const;
    // 工况三级树（组→子组→工况），供工况管理页树形展示。
    // 返回 QVariantList of {group, subgroups: [{subgroup, scenarios: [name,...]}]}
    Q_INVOKABLE QVariantList scenarioTree() const;
    Q_INVOKABLE QString createScenarioNode(const QString &kind,
                                           const QString &parentId,
                                           const QString &name);
    Q_INVOKABLE QVariantList scenarioTrajectory(const QString &name,
                                                const QString &actor) const;
    Q_INVOKABLE QVariantList previewTrajectory(const QVariantList &segments) const;
    Q_INVOKABLE QVariantList normalizeTrajectory(const QVariantList &segments) const;
    Q_INVOKABLE bool saveScenarioTrajectory(const QString &name,
                                            const QString &actor,
                                            const QVariantList &segments);
    Q_INVOKABLE QString monitorDeviceSelection(int slot) const;
    Q_INVOKABLE void setMonitorDeviceSelection(int slot, const QString &name);
    // 取某工况的目标物（平板）航点路径（QVariantList of QVariantMap{x,y,speed}）；无则空
    Q_INVOKABLE QVariantList scenarioWaypoints(const QString &name) const;
    // 取某工况的主车航点路径；无则空
    Q_INVOKABLE QVariantList scenarioEgoWaypoints(const QString &name) const;
    // 运行时新建工况（内存，不持久化）。type 决定默认参数模板。
    // 返回新工况名（可能加序号避免重名），失败返回空串。
    Q_INVOKABLE QString createScenario(const QString &name, const QString &type,
                                       const QString &group, const QString &subgroup);
    // 加载某 run 的采样数据，供轨迹回放（QVariantList of QVariantMap）
    Q_INVOKABLE QVariantList runSamples(const QString &runId) const;

signals:
    void clockTick();
    void readinessChanged();
    void scenarioTreeChanged();

private:
    QString resolveStorageRoot(const QString &requestedRoot) const;
    void loadTrajectoryOverrides();
    bool persistTrajectoryOverrides();
    void ensureCatalogScenarios();

    ConfigManager *m_config = nullptr;
    AuthManager *m_auth = nullptr;
    DatabaseManager *m_database = nullptr;
    DeviceManager *m_device = nullptr;
    SafetyManager *m_safety = nullptr;
    ScenarioEngine *m_scenario = nullptr;
    ScenarioCatalog *m_scenarioCatalog = nullptr;
    AlarmManager *m_alarm = nullptr;
    StateManager *m_state = nullptr;
    DataLogger *m_logger = nullptr;
    ReplayListModel *m_replay = nullptr;

    QTimer m_deviceTimer;
    QString m_currentTime;
    QString m_startupError;
    QString m_trajectoryOverridesPath;
    QString m_monitorSettingsPath;
    QVariantMap m_trajectoryOverrides;
    bool m_started = false;
};
