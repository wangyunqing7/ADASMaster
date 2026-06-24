#pragma once

// =============================================================================
// ScenarioStatus.h
// 功能：定义测试工况的执行状态枚举与测试状态结构。
// 输入：由 ScenarioEngine 在状态机推进过程中更新。
// 输出：驱动测试执行页面的状态机流程显示与运行监控页面的测试状态。
// 状态流转：
//   Idle -> Checking -> Ready -> Armed -> Running -> Finished
//   任何状态 -> Fault / Emergency（异常）
//   Fault/Emergency -> Idle（复位后）
// =============================================================================

#include <QObject>

// 测试执行状态机（通过 int 暴露给 QML）
enum class TestState {
    Idle       = 0,   // 空闲
    Checking   = 1,   // 检查中（设备/RTK/安全/起点）
    Ready      = 2,   // 就绪（检查通过）
    Armed      = 3,   // 预备（操作员点击预备后）
    Running    = 4,   // 运行中（倒计时结束，测试进行）
    Finished   = 5,   // 完成
    Fault      = 6,   // 故障
    Emergency  = 7    // 急停
};
