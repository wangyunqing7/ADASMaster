// =============================================================================
// DeviceManager.cpp —— 见头文件注释
// =============================================================================

#include "DeviceManager.h"
#include "FakeDevice.h"
#include <QStringList>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), m_model(new DeviceListModel(this))
{
}

DeviceManager::~DeviceManager()
{
    qDeleteAll(m_devices);
}

void DeviceManager::configure(const QList<DeviceConfig> &configs)
{
    qDeleteAll(m_devices);
    m_devices.clear();
    m_configs.clear();

    for (const DeviceConfig &config : configs) {
        if (!config.enabled) continue;
        auto *device = new FakeDevice(config.name, config.type, config.ip, config.port,
                                      config.motionTelemetry, this);
        m_devices.append(device);
        m_configs.append(config);
        connect(device, &IDevice::statusChanged, this, [this, device]() {
            const int row = m_devices.indexOf(device);
            if (row >= 0) m_model->refreshRow(row);
            recomputeCounts();
            emit deviceStatusChanged();
        });
    }
    m_model->setDevices(m_devices);
    emit totalCountChanged();
    recomputeCounts();
    emit deviceStatusChanged();
}

int DeviceManager::addDevice(const QString &name, const QString &type,
                             const QString &ip, int port, const QString &protocol)
{
    // 校验：名称、类型、IP、协议不能为空，端口必须合法
    if (name.trimmed().isEmpty() || type.trimmed().isEmpty() ||
        ip.trimmed().isEmpty() || protocol.trimmed().isEmpty() || port <= 0 || port > 65535)
        return -1;
    // 名称不可重复
    for (const IDevice *d : m_devices) {
        if (d->name().compare(name, Qt::CaseInsensitive) == 0) return -1;
    }

    DeviceConfig cfg;
    cfg.name = name.trimmed();
    cfg.type = type.trimmed();
    cfg.ip = ip.trimmed();
    cfg.port = port;
    cfg.protocol = protocol.trimmed();
    cfg.enabled = true;
    cfg.required = false;   // 运行时添加的设备默认非必需，不影响安全判定
    cfg.motionTelemetry = cfg.type.compare(QStringLiteral("DrivingRobot"), Qt::CaseInsensitive) == 0 ||
                          cfg.type.compare(QStringLiteral("TargetPlatform"), Qt::CaseInsensitive) == 0;

    auto *device = new FakeDevice(cfg.name, cfg.type, cfg.ip, cfg.port,
                                  cfg.motionTelemetry, this);
    device->connectDevice();   // 新设备添加后自动连接（模拟"添加即可用"）
    const int row = m_devices.size();
    m_devices.append(device);
    m_configs.append(cfg);
    connect(device, &IDevice::statusChanged, this, [this, device]() {
        const int r = m_devices.indexOf(device);
        if (r >= 0) m_model->refreshRow(r);
        recomputeCounts();
        emit deviceStatusChanged();
    });

    m_model->setDevices(m_devices);
    emit totalCountChanged();
    recomputeCounts();
    emit deviceStatusChanged();
    return row;
}

bool DeviceManager::removeDevice(int index)
{
    if (index < 0 || index >= m_devices.size()) return false;
    auto *device = m_devices.takeAt(index);
    m_configs.removeAt(index);
    delete device;
    m_model->setDevices(m_devices);
    emit totalCountChanged();
    recomputeCounts();
    emit deviceStatusChanged();
    return true;
}

bool DeviceManager::updateDevice(int index, const QString &name,
                                 const QString &ip, int port)
{
    if (index < 0 || index >= m_devices.size()) return false;
    // 名称校验：非空、不与其他设备重复（允许与自身原名相同）
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || ip.trimmed().isEmpty() || port <= 0 || port > 65535) return false;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (i != index && m_devices.at(i)->name().compare(trimmed, Qt::CaseInsensitive) == 0)
            return false;
    }
    IDevice *device = m_devices[index];
    device->setInfo(trimmed, ip.trimmed(), port);
    // 同步配置缓存
    m_configs[index].name = trimmed;
    m_configs[index].ip = ip.trimmed();
    m_configs[index].port = port;
    m_model->refreshRow(index);
    recomputeCounts();
    emit deviceStatusChanged();
    return true;
}

QVariantMap DeviceManager::deviceInfo(int index) const
{
    QVariantMap info;
    if (index < 0 || index >= m_devices.size()) return info;
    const DeviceStatus s = m_devices.at(index)->status();
    info.insert(QStringLiteral("name"), s.name);
    info.insert(QStringLiteral("type"), s.type);
    info.insert(QStringLiteral("ip"), s.ip);
    info.insert(QStringLiteral("port"), s.port);
    info.insert(QStringLiteral("connected"), s.connected);
    return info;
}

int DeviceManager::onlineCount() const { return m_onlineCount; }
int DeviceManager::faultCount()  const { return m_faultCount; }

QVariantMap DeviceManager::drivingRobotStatus() const
{
    return statusForType(QStringLiteral("DrivingRobot"));
}

QVariantMap DeviceManager::targetPlatformStatus() const
{
    return statusForType(QStringLiteral("TargetPlatform"));
}

QVariantMap DeviceManager::statusForType(const QString &type) const
{
    for (const IDevice *device : m_devices) {
        const DeviceStatus status = device->status();
        if (status.type == type)
            return statusToMap(status);
    }
    return {{QStringLiteral("available"), false}};
}

QVariantMap DeviceManager::statusToMap(const DeviceStatus &status)
{
    return {
        {QStringLiteral("available"), true},
        {QStringLiteral("name"), status.name},
        {QStringLiteral("type"), status.type},
        {QStringLiteral("ip"), status.ip},
        {QStringLiteral("port"), status.port},
        {QStringLiteral("connected"), status.connected},
        {QStringLiteral("heartbeatOk"), status.heartbeatOk},
        {QStringLiteral("fault"), status.fault},
        {QStringLiteral("latencyMs"), status.latencyMs},
        {QStringLiteral("packetLossPercent"), status.packetLossPercent},
        {QStringLiteral("signalStrengthPercent"), status.signalStrengthPercent}
    };
}

void DeviceManager::connectAll()
{
    for (auto *d : m_devices) d->connectDevice();
    recomputeCounts();
}

void DeviceManager::disconnectAll()
{
    for (auto *d : m_devices) d->disconnectDevice();
    recomputeCounts();
}

void DeviceManager::armAll()
{
    for (auto *d : m_devices) d->arm();
}

void DeviceManager::startAll()
{
    for (auto *d : m_devices) d->start();
    recomputeCounts();
}

void DeviceManager::stopAll()
{
    for (auto *d : m_devices) d->stop();
}

void DeviceManager::resetAll()
{
    for (auto *d : m_devices) d->reset();
    recomputeCounts();
}

void DeviceManager::emergencyStopAll()
{
    for (auto *d : m_devices) d->emergencyStop();
    recomputeCounts();
}

void DeviceManager::connectDevice(int index)
{
    if (index >= 0 && index < m_devices.size()) {
        m_devices[index]->connectDevice();
        recomputeCounts();
    }
}

void DeviceManager::disconnectDevice(int index)
{
    if (index >= 0 && index < m_devices.size()) {
        m_devices[index]->disconnectDevice();
        recomputeCounts();
    }
}

void DeviceManager::resetDevice(int index)
{
    if (index >= 0 && index < m_devices.size()) {
        m_devices[index]->reset();
        recomputeCounts();
    }
}

void DeviceManager::testDevice(int index)
{
    if (index >= 0 && index < m_devices.size()) {
        m_devices[index]->connectDevice();
        m_devices[index]->arm();
        m_devices[index]->start();
        // 真实场景下这里会触发单设备自检流程
    }
}

void DeviceManager::tick(qint64 nowMs)
{
    // 驱动每个设备刷新模拟量，并刷新对应模型行
    for (int i = 0; i < m_devices.size(); ++i) {
        m_devices[i]->tick(nowMs);
        m_model->refreshRow(i);
    }
    recomputeCounts();
}

void DeviceManager::recomputeCounts()
{
    int online = 0, fault = 0;
    for (const auto *d : m_devices) {
        const auto &s = d->status();
        if (s.connected && s.heartbeatOk) ++online;
        if (s.fault) ++fault;
    }
    if (online != m_onlineCount) { m_onlineCount = online; emit onlineCountChanged(); }
    if (fault  != m_faultCount)  { m_faultCount  = fault;  emit faultCountChanged(); }
    const bool healthy = allRequiredOnline() && allRequiredReady();
    if (healthy != m_requiredHealthy) {
        m_requiredHealthy = healthy;
        emit requiredHealthChanged();
    }
}

bool DeviceManager::allRequiredOnline() const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_configs.value(i).required) {
            const DeviceStatus status = m_devices.at(i)->status();
            if (!status.connected || !status.heartbeatOk) return false;
        }
    }
    return !m_devices.isEmpty();
}

bool DeviceManager::allRequiredReady() const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_configs.value(i).required && !m_devices.at(i)->status().ready) return false;
    }
    return !m_devices.isEmpty();
}

bool DeviceManager::allRequiredArmed() const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_configs.value(i).required && !m_devices.at(i)->status().armed) return false;
    }
    return !m_devices.isEmpty();
}

bool DeviceManager::allRequiredRunning() const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_configs.value(i).required && !m_devices.at(i)->status().running) return false;
    }
    return !m_devices.isEmpty();
}

bool DeviceManager::anyRunning() const
{
    for (const IDevice *device : m_devices) {
        if (device->status().running) return true;
    }
    return false;
}

QString DeviceManager::requiredHealthSummary() const
{
    QStringList failed;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (!m_configs.value(i).required) continue;
        const DeviceStatus status = m_devices.at(i)->status();
        if (!status.connected || !status.heartbeatOk || !status.ready)
            failed.append(status.name);
    }
    return failed.isEmpty() ? QStringLiteral("全部必需设备正常") : failed.join(QStringLiteral("、"));
}
