#pragma once

// =============================================================================
// SafetyStatus.h
// 功能：定义 ADAS 场地测试的安全状态枚举与安全状态结构。
// 输入：由 SafetyManager 根据硬件急停、软件急停、无线急停、通信超时等综合判定。
// 输出：被 SafetyManager 暴露为 Q_PROPERTY，驱动顶部状态栏安全指示灯与
//       ScenarioEngine 的允许预备/开始判定。
// 重要：软件急停只是软件层停止指令，真正的硬件急停由独立安全系统完成；
//       上位机只负责接收急停状态、停止测试流程、记录日志、锁定操作。
// =============================================================================

#include <QObject>

// 安全等级，颜色约定（见 Theme.qml）：
// Normal=绿色, Warning=黄色, Error=橙色, Emergency=红色
// 注：枚举通过 int 暴露给 QML（见各 Manager 的 Q_PROPERTY），无需 Q_ENUM_NS。
enum class SafetyLevel {
    Normal    = 0,   // 一切正常
    Warning   = 1,   // 存在警告（如单设备离线）
    Error     = 2,   // 存在错误（如通信故障）
    Emergency = 3    // 急停激活（最高优先级）
};
