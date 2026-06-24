// =============================================================================
// ConfigManager.cpp —— 见头文件注释
// =============================================================================

#include "ConfigManager.h"
#include "ScenarioParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

ConfigManager::ConfigManager(const QString &dataRoot, QObject *parent)
    : QObject(parent), m_requestedRoot(dataRoot)
{
}

QString ConfigManager::chooseRoot() const
{
    if (!m_requestedRoot.isEmpty()) return QDir(m_requestedRoot).absolutePath();

    const QString environmentRoot = qEnvironmentVariable("ADASMASTER_DATA_DIR");
    if (!environmentRoot.isEmpty()) return QDir(environmentRoot).absolutePath();

    const QString externalRoot = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (QFile::exists(QDir(externalRoot).filePath(QStringLiteral("config/devices.json"))))
        return externalRoot;

    return QStringLiteral(":/qt/qml/ADASMaster");
}

bool ConfigManager::load(QString *error)
{
    m_ready = false;
    m_lastError.clear();
    m_devices.clear();
    m_scenarios.clear();
    m_dataRoot = chooseRoot();

    const QDir root(m_dataRoot);
    if (!loadDevices(root.filePath(QStringLiteral("config/devices.json")), error) ||
        !loadScenarios(root.filePath(QStringLiteral("scenarios")), error)) {
        emit configChanged();
        return false;
    }

    m_ready = true;
    if (error) error->clear();
    emit configChanged();
    return true;
}

const ScenarioConfig *ConfigManager::scenarioByName(const QString &name) const
{
    for (const ScenarioConfig &scenario : m_scenarios) {
        if (scenario.name.compare(name, Qt::CaseInsensitive) == 0) return &scenario;
    }
    return nullptr;
}

bool ConfigManager::setScenarioPath(const QString &name,
                                    bool egoActor,
                                    const QList<Waypoint> &path)
{
    for (ScenarioConfig &scenario : m_scenarios) {
        if (scenario.name.compare(name, Qt::CaseInsensitive) != 0) continue;
        if (egoActor) scenario.egoPath = path;
        else scenario.targetPath = path;
        emit configChanged();
        return true;
    }
    return false;
}

bool ConfigManager::loadDevices(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()), error);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonValue devicesValue = document.object().value(QStringLiteral("devices"));
    if (document.isNull() || !document.isObject() || !devicesValue.isArray()) {
        fail(QStringLiteral("%1: devices must be an array (%2)").arg(path, parseError.errorString()), error);
        return false;
    }

    QSet<QString> names;
    const QJsonArray devices = devicesValue.toArray();
    for (int index = 0; index < devices.size(); ++index) {
        const QJsonObject object = devices.at(index).toObject();
        const QString prefix = QStringLiteral("devices[%1]").arg(index);
        DeviceConfig device;
        device.name = object.value(QStringLiteral("name")).toString().trimmed();
        device.type = object.value(QStringLiteral("type")).toString().trimmed();
        device.ip = object.value(QStringLiteral("ip")).toString().trimmed();
        device.protocol = object.value(QStringLiteral("protocol")).toString().trimmed();

        if (device.name.isEmpty() || device.type.isEmpty() || device.protocol.isEmpty() ||
            !object.value(QStringLiteral("port")).isDouble() ||
            !object.value(QStringLiteral("enabled")).isBool() ||
            !object.value(QStringLiteral("required")).isBool()) {
            fail(QStringLiteral("%1 has invalid required fields").arg(prefix), error);
            return false;
        }
        if (names.contains(device.name)) {
            fail(QStringLiteral("Duplicate device name: %1").arg(device.name), error);
            return false;
        }

        names.insert(device.name);
        device.port = object.value(QStringLiteral("port")).toInt();
        device.enabled = object.value(QStringLiteral("enabled")).toBool();
        device.required = object.value(QStringLiteral("required")).toBool();
        device.motionTelemetry = object.value(QStringLiteral("motion_telemetry")).toBool(false);
        if (device.enabled) m_devices.append(device);
    }

    if (m_devices.isEmpty()) {
        fail(QStringLiteral("No enabled devices found in %1").arg(path), error);
        return false;
    }
    return true;
}

bool ConfigManager::loadScenarios(const QString &directory, QString *error)
{
    QDir dir(directory);
    QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (files.isEmpty() && directory.startsWith(QLatin1Char(':'))) {
        files = {QStringLiteral("AEB_CCR_50.json"),
                 QStringLiteral("LKA_01.json"),
                 QStringLiteral("RCTA_01.json")};
    }
    if (files.isEmpty()) {
        fail(QStringLiteral("No scenario JSON files found in %1").arg(directory), error);
        return false;
    }

    QSet<QString> names;
    for (const QString &fileName : files) {
        ScenarioConfig scenario;
        QString parseError;
        const QString path = dir.filePath(fileName);
        if (!ScenarioParser::parseFile(path, &scenario, &parseError)) {
            fail(parseError, error);
            return false;
        }
        if (names.contains(scenario.name)) {
            fail(QStringLiteral("Duplicate scenario name: %1").arg(scenario.name), error);
            return false;
        }
        names.insert(scenario.name);
        m_scenarios.append(scenario);
    }
    return true;
}

void ConfigManager::fail(const QString &message, QString *error)
{
    m_lastError = message;
    if (error) *error = message;
}
