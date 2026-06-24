#pragma once

// =============================================================================
// TestResult.h
// 功能：定义单次 NCAP/ADAS 测试运行的结构化结果。由 ScenarioEngine 在运行中
//       填充，交由 NcapEvaluator 评定 PASS/FAIL，最终落库与展示。
// 与旧版"resultText 字符串"的区别：本结构是强类型、可追溯、可导出报表的。
// 字段单位：速度 m/s，距离 m，时间 s，角度 deg。
// =============================================================================

#include <QString>

// 测试裁决结果（由 NcapEvaluator 给出）
enum class NcapOutcome {
    Pass = 0,   // 通过
    Fail = 1,   // 未通过
    Pending = 2 // 待评定（数据不足）
};

// 一次测试的原始观测数据（NcapEvaluator 的输入）
struct TestResult
{
    bool   collisionOccurred = false;       // 是否发生碰撞
    double speedAtImpactMps  = 0.0;         // 碰撞瞬间主车速度（未碰撞=0）
    double egoTargetSpeed    = 0.0;         // 工况设定的主车目标速度（评分基准）
    double minTtcSec         = 999.0;       // 全程最小 TTC
    double minDistanceM      = 999.0;       // 全程最小纵向距离
    bool   completedNormally = false;       // 是否正常完成（非急停/异常中断）
    double durationSec       = 0.0;         // 实际运行时长
    double maxLateralDeviationM = 0.0;      // LKA 横向偏离（LKA 工况用）
    double aebTriggerSpeedMps = 0.0;        // AEB 首次触发速度
};

// 评估后的裁决与评分（NcapEvaluator 的输出）
struct NcapVerdict
{
    NcapOutcome verdict = NcapOutcome::Pending;
    double scorePercent = 0.0;      // 0~100，NCAP 网格评分
    QString detail;                 // 中文详情说明

    // 本地化文案
    QString outcomeText() const
    {
        switch (verdict) {
        case NcapOutcome::Pass:    return QStringLiteral("通过");
        case NcapOutcome::Fail:    return QStringLiteral("未通过");
        case NcapOutcome::Pending: return QStringLiteral("待评定");
        }
        return QStringLiteral("未知");
    }
};
