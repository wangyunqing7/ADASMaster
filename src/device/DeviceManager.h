#pragma once

// =============================================================================
// DeviceManager.h
// 功能：统一管理所有设备。负责设备的创建、连接、统一时钟刷新、状态聚合，
//       并把列表暴露给 QML（通过 DeviceListModel）。
// 输入：AppController 在 init() 时调用 createFakeDevices() 建立模拟设备；
//       上层调用 connectAll()/armAll() 等批量操作。
// 输出：QML 通过 deviceListModel 访问设备；onlineCount 等聚合属性驱动顶部状态栏。
//
// 【后续扩展】真实设备接入时，只需在 createDevices() 里把 FakeDevice 替换成
//   对应的真实客户端（DrivingRobotClient / TargetPlatformClient / GnssClient 等），
//   它们都实现 IDevice 接口，上层无需修改。
// =============================================================================

#include <QObject>
#include <QVariantMap>
#include "IDevice.h"
#include "DeviceListModel.h"
#include "../config/DeviceConfig.h"

class DeviceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(DeviceListModel* deviceListModel READ deviceListModel CONSTANT)
    Q_PROPERTY(int onlineCount   READ onlineCount    NOTIFY onlineCountChanged)
    Q_PROPERTY(int totalCount    READ totalCount     NOTIFY totalCountChanged)
    Q_PROPERTY(int faultCount    READ faultCount     NOTIFY faultCountChanged)
    Q_PROPERTY(bool requiredDevicesHealthy READ requiredDevicesHealthy NOTIFY requiredHealthChanged)
    Q_PROPERTY(QVariantMap drivingRobotStatus READ drivingRobotStatus NOTIFY deviceStatusChanged)
    Q_PROPERTY(QVariantMap targetPlatformStatus READ targetPlatformStatus NOTIFY deviceStatusChanged)

public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    DeviceListModel *deviceListModel() const { return m_model; }
    int onlineCount() const;
    int totalCount()  const { return m_devices.size(); }
    int faultCount()  const;
    bool requiredDevicesHealthy() const { return allRequiredOnline() && allRequiredReady(); }
    QVariantMap drivingRobotStatus() const;
    QVariantMap targetPlatformStatus() const;

    void configure(const QList<DeviceConfig> &configs);

    // 运行时动态添加设备（设备管理页"添加设备"对话框调用）。
    // 返回新设备在列表中的索引，失败返回 -1（如名称为空或重复）。
    Q_INVOKABLE int addDevice(const QString &name, const QString &type,
                              const QString &ip, int port, const QString &protocol);
    // 运行时移除设备
    Q_INVOKABLE bool removeDevice(int index);
    // 修改设备信息（名称/IP/端口）。名称若与其他设备重复则失败。
    Q_INVOKABLE bool updateDevice(int index, const QString &name,
                                  const QString &ip, int port);
    // 读取设备基本信息（供编辑对话框填充表单）
    Q_INVOKABLE QVariantMap deviceInfo(int index) const;

    // 批量操作
    Q_INVOKABLE void connectAll();
    Q_INVOKABLE void disconnectAll();
    Q_INVOKABLE void armAll();
    Q_INVOKABLE void startAll();
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE void emergencyStopAll();

    // 单设备操作（QML 设备管理页"连接/断开/复位"按钮调用）
    Q_INVOKABLE void connectDevice(int index);
    Q_INVOKABLE void disconnectDevice(int index);
    Q_INVOKABLE void resetDevice(int index);
    Q_INVOKABLE void testDevice(int index);   // 单设备测试

    // 统一时钟驱动，由 AppController 调用
    void tick(qint64 nowMs);

    bool allRequiredOnline() const;
    bool allRequiredReady() const;
    bool allRequiredArmed() const;
    bool allRequiredRunning() const;
    bool anyRunning() const;
    QString requiredHealthSummary() const;

signals:
    void onlineCountChanged();
    void totalCountChanged();
    void faultCountChanged();
    void requiredHealthChanged();
    void deviceStatusChanged();

private:
    void recomputeCounts();
    QVariantMap statusForType(const QString &type) const;
    static QVariantMap statusToMap(const DeviceStatus &status);

    QList<IDevice*> m_devices;
    QList<DeviceConfig> m_configs;
    DeviceListModel *m_model;
    int m_onlineCount = 0;
    int m_faultCount  = 0;
    bool m_requiredHealthy = false;
};
