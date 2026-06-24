#include "AppController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QtMath>
#include <cmath>

#include "../scenario/TrajectoryGeometry.h"

namespace {
QList<TrajectorySegment> segmentsFromVariant(const QVariantList &input)
{
    QList<TrajectorySegment> output;
    for (const QVariant &value : input) {
        const QVariantMap map = value.toMap();
        TrajectorySegment segment;
        segment.kind = map.value(QStringLiteral("kind")).toString() == QLatin1String("arc")
            ? TrajectorySegmentKind::CircularArc : TrajectorySegmentKind::Straight;
        segment.start = QPointF(map.value(QStringLiteral("startX")).toDouble(),
                                map.value(QStringLiteral("startY")).toDouble());
        segment.end = QPointF(map.value(QStringLiteral("endX")).toDouble(),
                              map.value(QStringLiteral("endY")).toDouble());
        segment.startSpeedMps = map.value(QStringLiteral("startSpeedKph")).toDouble() / 3.6;
        segment.endSpeedMps = map.value(QStringLiteral("endSpeedKph")).toDouble() / 3.6;
        segment.radiusM = map.value(QStringLiteral("radiusM")).toDouble();
        segment.turnDirection = map.value(QStringLiteral("turnDirection")).toString() == QLatin1String("right")
            ? TrajectoryTurnDirection::Right : TrajectoryTurnDirection::Left;
        output.append(segment);
    }
    return TrajectoryGeometry::propagate(output);
}

QVariantMap segmentMap(const TrajectorySegment &segment)
{
    const DerivedTrajectorySegment derived = TrajectoryGeometry::derive(segment);
    return {
        {QStringLiteral("kind"), segment.kind == TrajectorySegmentKind::CircularArc ? QStringLiteral("arc") : QStringLiteral("straight")},
        {QStringLiteral("startX"), segment.start.x()},
        {QStringLiteral("startY"), segment.start.y()},
        {QStringLiteral("endX"), segment.end.x()},
        {QStringLiteral("endY"), segment.end.y()},
        {QStringLiteral("startSpeedKph"), segment.startSpeedMps * 3.6},
        {QStringLiteral("endSpeedKph"), segment.endSpeedMps * 3.6},
        {QStringLiteral("radiusM"), segment.radiusM},
        {QStringLiteral("turnDirection"), segment.turnDirection == TrajectoryTurnDirection::Right ? QStringLiteral("right") : QStringLiteral("left")},
        {QStringLiteral("valid"), derived.valid},
        {QStringLiteral("error"), derived.error},
        {QStringLiteral("lengthM"), derived.lengthM},
        {QStringLiteral("headingDeg"), derived.headingDeg},
        {QStringLiteral("sweepDeg"), derived.sweepDeg},
        {QStringLiteral("accelerationMps2"), derived.tangentialAccelerationMps2},
        {QStringLiteral("lateralAccelerationMps2"), derived.maxLateralAccelerationMps2},
        {QStringLiteral("durationSec"), derived.durationSec}
    };
}

QVariantList mapsFromSegments(const QList<TrajectorySegment> &segments)
{
    QVariantList list;
    for (const TrajectorySegment &segment : segments) list.append(segmentMap(segment));
    return list;
}
}

AppController::AppController(const QString &dataRoot,
                             const QString &storageRoot,
                             QObject *parent)
    : QObject(parent)
{
    m_config = new ConfigManager(dataRoot, this);
    m_database = new DatabaseManager(this);
    m_device = new DeviceManager(this);
    m_safety = new SafetyManager(this);
    m_alarm = new AlarmManager(this);
    m_auth = new AuthManager(this);
    m_state = new StateManager(this);
    m_logger = new DataLogger(this);
    m_replay = new ReplayListModel(this);
    m_scenario = new ScenarioEngine(this);

    QString configError;
    if (!m_config->load(&configError))
        m_startupError = QStringLiteral("配置加载失败：%1").arg(configError);

    const QString resolvedStorageRoot = resolveStorageRoot(storageRoot);
    m_trajectoryOverridesPath = QDir(resolvedStorageRoot)
                                    .filePath(QStringLiteral("trajectory_overrides.json"));
    m_monitorSettingsPath = QDir(resolvedStorageRoot)
                                .filePath(QStringLiteral("monitor_settings.ini"));
    loadTrajectoryOverrides();
    const QString databasePath = QDir(resolvedStorageRoot)
                                     .filePath(QStringLiteral("adasmaster.sqlite"));
    if (!m_database->open(databasePath)) {
        const QString error = QStringLiteral("数据库打开失败：%1").arg(m_database->lastError());
        m_startupError = m_startupError.isEmpty() ? error : m_startupError + QStringLiteral("；") + error;
    }

    if (m_config->ready()) {
        m_device->configure(m_config->devices());
        m_scenario->setScenarios(m_config->scenarios());
        m_auth->loadUsers(QDir(m_config->dataRoot()).filePath(QStringLiteral("config")));
    }

    m_scenarioCatalog = new ScenarioCatalog(
        QDir(resolvedStorageRoot).filePath(QStringLiteral("scenario_catalog.json")), this);
    QString catalogError;
    if (!m_scenarioCatalog->load(m_config->ready() ? m_config->scenarios() : QList<ScenarioConfig>{},
                                 &catalogError)) {
        const QString error = QStringLiteral("场景目录加载失败：%1").arg(catalogError);
        m_startupError = m_startupError.isEmpty() ? error : m_startupError + QStringLiteral("；") + error;
    }
    connect(m_scenarioCatalog, &ScenarioCatalog::changed,
            this, &AppController::scenarioTreeChanged);
    ensureCatalogScenarios();

    m_logger->setDatabaseManager(m_database);
    m_logger->setStateManager(m_state);
    m_replay->setDatabaseManager(m_database);
    m_replay->refresh();
    if (m_database->ready())
        m_alarm->setDatabaseManager(m_database);

    m_auth->setAlarmManager(m_alarm);

    // 审计链：登录状态变化时把当前操作员注入数据记录器，修复 operatorName 硬编码
    m_logger->setOperatorName(m_auth->isLoggedIn() ? m_auth->currentUser()
                                                   : QStringLiteral("default"));
    connect(m_auth, &AuthManager::loginStateChanged, this, [this]() {
        m_logger->setOperatorName(m_auth->isLoggedIn() ? m_auth->currentUser()
                                                       : QStringLiteral("default"));
    });

    m_scenario->setDeviceManager(m_device);
    m_scenario->setSafetyManager(m_safety);
    m_scenario->setStateManager(m_state);
    m_scenario->setAlarmManager(m_alarm);
    m_scenario->setDataLogger(m_logger);

    connect(m_safety, &SafetyManager::emergencyTriggered,
            m_scenario, &ScenarioEngine::onSafetyEmergency);
    connect(m_database, &DatabaseManager::databaseChanged,
            this, &AppController::readinessChanged);
    connect(m_config, &ConfigManager::configChanged,
            this, &AppController::readinessChanged);
    connect(m_safety, &SafetyManager::safetyChanged, this, [this]() {
        m_state->setSafetyLevel(m_safety->safetyLevel());
    });
    connect(m_device, &DeviceManager::requiredHealthChanged, this, [this]() {
        m_safety->onCommunicationDegraded(m_device->requiredDevicesHealthy());
    });
    connect(m_scenario, &ScenarioEngine::testStateChanged, this, [this]() {
        const TestState state = m_scenario->testState();
        if (state == TestState::Finished || state == TestState::Fault ||
            state == TestState::Emergency)
            m_replay->refresh();
    });
    connect(m_logger, &DataLogger::loggingChanged, this, [this]() {
        if (!m_logger->active())
            m_replay->refresh();
    });

    m_deviceTimer.setInterval(1000);
    connect(&m_deviceTimer, &QTimer::timeout, this, [this]() {
        m_device->tick(QDateTime::currentMSecsSinceEpoch());
    });

    auto *clockTimer = new QTimer(this);
    clockTimer->setInterval(1000);
    connect(clockTimer, &QTimer::timeout, this, &AppController::refreshClock);
    refreshClock();
    clockTimer->start();

    m_safety->onCommunicationDegraded(m_device->requiredDevicesHealthy());
    m_state->setSafetyLevel(m_safety->safetyLevel());

    if (operationalReady())
        m_alarm->info(QStringLiteral("系统启动"), QStringLiteral("配置与 SQLite 数据库已就绪"));
    else
        m_alarm->error(QStringLiteral("启动失败"), m_startupError);
}

bool AppController::operationalReady() const
{
    return m_config && m_config->ready() && m_database && m_database->writable();
}

QString AppController::resolveStorageRoot(const QString &requestedRoot) const
{
    if (!requestedRoot.isEmpty())
        return QDir(requestedRoot).absolutePath();

    const QString environmentRoot = qEnvironmentVariable("ADASMASTER_DATA_DIR");
    if (!environmentRoot.isEmpty())
        return QDir(environmentRoot).filePath(QStringLiteral("data"));

    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void AppController::startup()
{
    if (m_started || !operationalReady())
        return;

    m_device->connectAll();
    m_deviceTimer.start();
    m_started = true;
    m_alarm->info(QStringLiteral("设备连接"),
                  QStringLiteral("已连接 %1 台设备").arg(m_device->onlineCount()));
}

void AppController::refreshClock()
{
    m_currentTime = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    emit clockTick();
}

QStringList AppController::scenarioNames() const
{
    QStringList names;
    if (m_config) {
        for (const ScenarioConfig &s : m_config->scenarios())
            names.append(s.name);
    }
    return names;
}

QVariantList AppController::scenarioTree() const
{
    if (m_scenarioCatalog)
        return m_scenarioCatalog->tree();

    // 按 group → subgroup → 工况名 三级聚合
    QVariantList tree;
    if (!m_config) return tree;

    // 用有序的 group 列表 + 嵌套 map 保持稳定顺序
    QStringList groupOrder;
    QMap<QString, QStringList> subgroupOrder;          // group -> 子组顺序
    QMap<QString, QMap<QString, QStringList>> data;    // group -> subgroup -> [names]

    for (const ScenarioConfig &s : m_config->scenarios()) {
        const QString &g = s.group.isEmpty() ? QStringLiteral("未分组") : s.group;
        const QString &sg = s.subgroup;
        if (!groupOrder.contains(g)) groupOrder.append(g);
        if (!sg.isEmpty() && !subgroupOrder[g].contains(sg)) subgroupOrder[g].append(sg);
        data[g][sg].append(s.name);
    }

    for (const QString &g : groupOrder) {
        QVariantMap groupNode;
        groupNode.insert(QStringLiteral("group"), g);
        QVariantList subNodes;
        const QStringList subs = subgroupOrder.value(g);
        if (subs.isEmpty()) {
            // 无子组，工况直接挂在组下（用空子组名）
            QVariantMap sgNode;
            sgNode.insert(QStringLiteral("subgroup"), QString());
            sgNode.insert(QStringLiteral("scenarios"), data.value(g).value(QString()));
            subNodes.append(sgNode);
        } else {
            for (const QString &sg : subs) {
                QVariantMap sgNode;
                sgNode.insert(QStringLiteral("subgroup"), sg);
                sgNode.insert(QStringLiteral("scenarios"), data.value(g).value(sg));
                subNodes.append(sgNode);
            }
        }
        groupNode.insert(QStringLiteral("subgroups"), subNodes);
        tree.append(groupNode);
    }
    return tree;
}

QString AppController::createScenarioNode(const QString &kind,
                                          const QString &parentId,
                                          const QString &name)
{
    if (!m_scenarioCatalog) return {};
    ScenarioNodeKind nodeKind;
    if (kind.compare(QStringLiteral("group"), Qt::CaseInsensitive) == 0)
        nodeKind = ScenarioNodeKind::Group;
    else if (kind.compare(QStringLiteral("subgroup"), Qt::CaseInsensitive) == 0)
        nodeKind = ScenarioNodeKind::Subgroup;
    else if (kind.compare(QStringLiteral("scenario"), Qt::CaseInsensitive) == 0)
        nodeKind = ScenarioNodeKind::Scenario;
    else
        return {};

    QString error;
    const QString id = m_scenarioCatalog->createNode(nodeKind, parentId, name, &error);
    if (id.isEmpty() && m_alarm)
        m_alarm->warning(QStringLiteral("新建场景节点失败"), error);
    if (!id.isEmpty() && nodeKind == ScenarioNodeKind::Scenario)
        ensureCatalogScenarios();
    return id;
}

QVariantList AppController::scenarioTrajectory(const QString &name,
                                               const QString &actor) const
{
    const QString key = name + QStringLiteral("::") + actor.toLower();
    if (m_trajectoryOverrides.contains(key))
        return m_trajectoryOverrides.value(key).toList();
    if (!m_config) return {};
    const ScenarioConfig *scenario = m_config->scenarioByName(name);
    if (!scenario) return {};
    const QList<Waypoint> &path = actor.compare(QStringLiteral("ego"), Qt::CaseInsensitive) == 0
        ? scenario->egoPath : scenario->targetPath;
    QList<TrajectorySegment> segments;
    for (int i = 1; i < path.size(); ++i) {
        TrajectorySegment segment;
        segment.start = QPointF(path.at(i - 1).x, path.at(i - 1).y);
        segment.end = QPointF(path.at(i).x, path.at(i).y);
        segment.startSpeedMps = path.at(i - 1).speed;
        segment.endSpeedMps = path.at(i).speed;
        segments.append(segment);
    }
    if (segments.isEmpty() && actor.compare(QStringLiteral("ego"), Qt::CaseInsensitive) == 0) {
        TrajectorySegment segment;
        segment.start = QPointF(scenario->egoStartX, scenario->egoStartY);
        const double yaw = qDegreesToRadians(scenario->egoStartYaw);
        segment.end = segment.start + QPointF(std::cos(yaw) * 100.0, std::sin(yaw) * 100.0);
        segment.startSpeedMps = 0.0;
        segment.endSpeedMps = qMax(0.1, qAbs(scenario->egoTargetSpeed));
        segments.append(segment);
    }
    return mapsFromSegments(segments);
}

QVariantList AppController::previewTrajectory(const QVariantList &segments) const
{
    const QList<Waypoint> samples = TrajectoryGeometry::sample(segmentsFromVariant(segments));
    QVariantList output;
    for (const Waypoint &point : samples) {
        output.append(QVariantMap{{QStringLiteral("x"), point.x},
                                  {QStringLiteral("y"), point.y},
                                  {QStringLiteral("speed"), point.speed}});
    }
    return output;
}

QVariantList AppController::normalizeTrajectory(const QVariantList &segments) const
{
    return mapsFromSegments(segmentsFromVariant(segments));
}

bool AppController::saveScenarioTrajectory(const QString &name,
                                           const QString &actor,
                                           const QVariantList &segments)
{
    if (name.trimmed().isEmpty() || (actor != QLatin1String("ego") && actor != QLatin1String("target")))
        return false;
    if (!m_config || !m_config->scenarioByName(name)) return false;
    const QList<TrajectorySegment> normalized = segmentsFromVariant(segments);
    if (normalized.isEmpty()) return false;
    for (const TrajectorySegment &segment : normalized)
        if (!TrajectoryGeometry::derive(segment).valid) return false;
    const QString key = name + QStringLiteral("::") + actor;
    const QVariant old = m_trajectoryOverrides.value(key);
    m_trajectoryOverrides.insert(key, mapsFromSegments(normalized));
    if (!persistTrajectoryOverrides()) {
        if (old.isValid()) m_trajectoryOverrides.insert(key, old);
        else m_trajectoryOverrides.remove(key);
        return false;
    }
    m_config->setScenarioPath(name, actor == QLatin1String("ego"),
                              TrajectoryGeometry::sample(normalized));
    if (m_scenario) m_scenario->setScenarios(m_config->scenarios());
    return true;
}

QString AppController::monitorDeviceSelection(int slot) const
{
    if (slot < 0 || slot > 1) return {};
    QSettings settings(m_monitorSettingsPath, QSettings::IniFormat);
    return settings.value(QStringLiteral("motion/slot%1").arg(slot)).toString();
}

void AppController::setMonitorDeviceSelection(int slot, const QString &name)
{
    if (slot < 0 || slot > 1) return;
    QSettings settings(m_monitorSettingsPath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("motion/slot%1").arg(slot), name);
    settings.sync();
}

void AppController::loadTrajectoryOverrides()
{
    m_trajectoryOverrides.clear();
    QFile file(m_trajectoryOverridesPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;
    m_trajectoryOverrides = doc.object().toVariantMap();
    if (!m_config) return;
    for (auto it = m_trajectoryOverrides.cbegin(); it != m_trajectoryOverrides.cend(); ++it) {
        const int separator = it.key().lastIndexOf(QStringLiteral("::"));
        if (separator <= 0) continue;
        const QString name = it.key().left(separator);
        const QString actor = it.key().mid(separator + 2);
        if (actor != QLatin1String("ego") && actor != QLatin1String("target")) continue;
        const QList<TrajectorySegment> segments = segmentsFromVariant(it.value().toList());
        bool valid = !segments.isEmpty();
        for (const TrajectorySegment &segment : segments)
            valid = valid && TrajectoryGeometry::derive(segment).valid;
        if (valid) {
            m_config->setScenarioPath(name, actor == QLatin1String("ego"),
                                      TrajectoryGeometry::sample(segments));
        }
    }
}

bool AppController::persistTrajectoryOverrides()
{
    QDir().mkpath(QFileInfo(m_trajectoryOverridesPath).absolutePath());
    QSaveFile file(m_trajectoryOverridesPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument::fromVariant(m_trajectoryOverrides).toJson());
    return file.commit();
}

void AppController::ensureCatalogScenarios()
{
    if (!m_scenarioCatalog || !m_config) return;
    const QList<ScenarioNode> nodes = m_scenarioCatalog->nodes();
    auto findNode = [&nodes](const QString &id) -> const ScenarioNode * {
        for (const ScenarioNode &node : nodes) if (node.id == id) return &node;
        return nullptr;
    };
    bool added = false;
    for (const ScenarioNode &node : nodes) {
        if (node.kind != ScenarioNodeKind::Scenario || m_config->scenarioByName(node.name))
            continue;
        const ScenarioNode *subgroup = findNode(node.parentId);
        const ScenarioNode *group = subgroup ? findNode(subgroup->parentId) : nullptr;
        if (!subgroup || !group) continue;
        ScenarioConfig scenario;
        scenario.name = node.name;
        scenario.type = QStringLiteral("NCAP");
        scenario.group = group->name;
        scenario.subgroup = subgroup->name;
        scenario.description = QStringLiteral("用户创建的 NCAP 场景");
        scenario.maxTestTimeSec = 30.0;
        scenario.repeatCount = 1;
        m_config->addScenario(scenario);
        added = true;
    }
    if (added && m_scenario) m_scenario->setScenarios(m_config->scenarios());
}

QVariantList AppController::scenarioWaypoints(const QString &name) const
{
    QVariantList list;
    if (!m_config) return list;
    const ScenarioConfig *s = m_config->scenarioByName(name);
    if (!s) return list;
    for (const Waypoint &wp : s->targetPath) {
        QVariantMap point;
        point.insert(QStringLiteral("x"), wp.x);
        point.insert(QStringLiteral("y"), wp.y);
        point.insert(QStringLiteral("speed"), wp.speed);
        list.append(point);
    }
    return list;
}

QVariantList AppController::scenarioEgoWaypoints(const QString &name) const
{
    QVariantList list;
    if (!m_config) return list;
    const ScenarioConfig *s = m_config->scenarioByName(name);
    if (!s) return list;
    for (const Waypoint &wp : s->egoPath) {
        QVariantMap point;
        point.insert(QStringLiteral("x"), wp.x);
        point.insert(QStringLiteral("y"), wp.y);
        point.insert(QStringLiteral("speed"), wp.speed);
        list.append(point);
    }
    return list;
}

QString AppController::createScenario(const QString &name, const QString &type,
                                       const QString &group, const QString &subgroup)
{
    if (!m_config || name.trimmed().isEmpty()) return QString();

    // 名称去重：若已存在则追加序号
    QString finalName = name.trimmed();
    int suffix = 2;
    while (m_config->scenarioByName(finalName)) {
        finalName = QStringLiteral("%1 %2").arg(name.trimmed()).arg(suffix++);
    }

    ScenarioConfig scenario;
    scenario.name = finalName;
    scenario.type = type;
    scenario.group = group.isEmpty() ? type : group;
    scenario.subgroup = subgroup;
    scenario.description = QStringLiteral("新建 %1 工况").arg(type);
    scenario.repeatCount = 1;

    // 按类型设置默认参数模板
    if (type.compare(QStringLiteral("AEB"), Qt::CaseInsensitive) == 0) {
        scenario.egoTargetSpeed = 13.89;   // 50 km/h
        scenario.egoStartX = -50.0;
        scenario.targetType = QStringLiteral("静止车辆");
        scenario.targetStartX = 0.0;
        scenario.maxTestTimeSec = 20.0;
        scenario.passTtcSec = 2.0;
        scenario.noCollisionRequired = true;
        scenario.safetyBoundaryM = 60.0;
        scenario.triggerCondition = QStringLiteral("主车进入触发区域");
        scenario.stopCondition = QStringLiteral("主车停止或通过目标");
    } else if (type.compare(QStringLiteral("RCTA"), Qt::CaseInsensitive) == 0) {
        scenario.egoTargetSpeed = -2.0;
        scenario.egoStartYaw = 270.0;   // 倒车
        scenario.targetType = QStringLiteral("横穿车辆");
        scenario.targetSpeed = 3.0;
        scenario.targetStartX = -10.0;
        scenario.targetStartY = 5.0;
        scenario.maxTestTimeSec = 15.0;
        scenario.passTtcSec = 1.5;
        scenario.noCollisionRequired = true;
        scenario.safetyBoundaryM = 40.0;
        scenario.triggerCondition = QStringLiteral("后向雷达探测到目标");
        scenario.stopCondition = QStringLiteral("碰撞风险解除或超时");
    } else if (type.compare(QStringLiteral("LKA"), Qt::CaseInsensitive) == 0) {
        scenario.egoTargetSpeed = 20.0;
        scenario.targetType = QStringLiteral("车道线");
        scenario.maxTestTimeSec = 30.0;
        scenario.safetyBoundaryM = 80.0;
        scenario.maxLateralDeviationM = 0.3;
        scenario.triggerCondition = QStringLiteral("车道线曲率变化");
        scenario.stopCondition = QStringLiteral("车辆回正或偏离出界");
    }

    m_config->addScenario(scenario);
    return finalName;
}

QVariantList AppController::runSamples(const QString &runId) const
{
    QVariantList list;
    if (!m_database || !m_database->ready() || runId.isEmpty()) return list;
    const QList<SampleRecord> samples = m_database->listSamples(runId);
    for (const SampleRecord &s : samples) {
        QVariantMap m;
        m.insert(QStringLiteral("timestampMs"), s.timestampMs);
        m.insert(QStringLiteral("egoX"), s.egoX);
        m.insert(QStringLiteral("egoY"), s.egoY);
        m.insert(QStringLiteral("egoSpeed"), s.egoSpeed);
        m.insert(QStringLiteral("targetX"), s.targetX);
        m.insert(QStringLiteral("targetY"), s.targetY);
        m.insert(QStringLiteral("targetSpeed"), s.targetSpeed);
        m.insert(QStringLiteral("distance"), s.distance);
        m.insert(QStringLiteral("ttc"), s.ttc);
        m.insert(QStringLiteral("collisionRisk"), s.collisionRisk);
        list.append(m);
    }
    return list;
}
