// =============================================================================
// DeviceListModel.cpp —— 见头文件注释
// =============================================================================

#include "DeviceListModel.h"
#include "IDevice.h"

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};
    const DeviceStatus s = m_devices.at(index.row())->status();
    switch (role) {
        case NameRole:      return s.name;
        case TypeRole:      return s.type;
        case IpRole:        return s.ip;
        case PortRole:      return s.port;
        case ConnectedRole: return s.connected;
        case HeartbeatRole: return s.heartbeatOk;
        case ReadyRole:     return s.ready;
        case ArmedRole:     return s.armed;
        case RunningRole:   return s.running;
        case FaultRole:     return s.fault;
        case FaultCodeRole: return s.faultCode;
        case BatteryRole:   return s.batteryPercent;
        case LatencyRole:   return s.latencyMs;
        case LastUpdateRole:return s.lastUpdateTimeMs;
        case PacketLossRole:return s.packetLossPercent;
        case SignalStrengthRole:return s.signalStrengthPercent;
        case MotionCapableRole:return s.motionCapable;
        case PositionXRole:   return s.xM;
        case PositionYRole:   return s.yM;
        case SpeedRole:       return s.speedMps;
        case YawRole:         return s.yawDeg;
    }
    return {};
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        {NameRole,       "deviceName"},
        {TypeRole,       "deviceType"},
        {IpRole,         "deviceIp"},
        {PortRole,       "devicePort"},
        {ConnectedRole,  "deviceConnected"},
        {HeartbeatRole,  "deviceHeartbeat"},
        {ReadyRole,      "deviceReady"},
        {ArmedRole,      "deviceArmed"},
        {RunningRole,    "deviceRunning"},
        {FaultRole,      "deviceFault"},
        {FaultCodeRole,  "deviceFaultCode"},
        {BatteryRole,    "deviceBattery"},
        {LatencyRole,    "deviceLatency"},
        {LastUpdateRole, "deviceLastUpdate"},
        {PacketLossRole, "devicePacketLoss"},
        {SignalStrengthRole, "deviceSignalStrength"},
        {MotionCapableRole, "deviceMotionCapable"},
        {PositionXRole, "deviceX"},
        {PositionYRole, "deviceY"},
        {SpeedRole, "deviceSpeed"},
        {YawRole, "deviceYaw"}
    };
}

void DeviceListModel::setDevices(const QList<IDevice*> &devices)
{
    beginResetModel();
    m_devices = devices;
    endResetModel();
    emit countChanged();
}

void DeviceListModel::refreshRow(int row)
{
    if (row < 0 || row >= m_devices.size())
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

int DeviceListModel::count() const { return m_devices.size(); }
