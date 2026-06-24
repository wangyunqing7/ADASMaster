#pragma once

// =============================================================================
// DeviceConfig.h
// 功能：设备 JSON 配置的强类型表示，供 ConfigManager 与 DeviceManager 使用。
// =============================================================================

#include <QString>

struct DeviceConfig
{
    QString name;
    QString type;
    QString ip;
    QString protocol;
    int port = 0;
    bool enabled = true;
    bool required = true;
    bool motionTelemetry = false;
};
