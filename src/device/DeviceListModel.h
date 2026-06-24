#pragma once

// =============================================================================
// DeviceListModel.h
// 功能：以 QAbstractListModel 形式把设备列表暴露给 QML。
// 输入：由 DeviceManager 维护底层 QList<IDevice*>，并通知本模型增删/更新。
// 输出：QML ListView/GridView 通过 role 访问设备各字段。
// 说明：QML 不直接访问底层设备对象，只能通过本模型只读展示。
// =============================================================================

#include <QAbstractListModel>

class IDevice;

class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        IpRole,
        PortRole,
        ConnectedRole,
        HeartbeatRole,
        ReadyRole,
        ArmedRole,
        RunningRole,
        FaultRole,
        FaultCodeRole,
        BatteryRole,
        LatencyRole,
        LastUpdateRole,
        PacketLossRole,
        SignalStrengthRole,
        MotionCapableRole,
        PositionXRole,
        PositionYRole,
        SpeedRole,
        YawRole
    };
    Q_ENUM(Roles)

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 由 DeviceManager 调用
    void setDevices(const QList<IDevice*> &devices);
    void refreshRow(int row);          // 刷新指定行（触发 dataChanged）
    int  count() const;

signals:
    void countChanged();

private:
    QList<IDevice*> m_devices;
};
