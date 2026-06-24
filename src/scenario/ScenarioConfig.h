#pragma once

// =============================================================================
// ScenarioConfig.h
// 功能：定义一个 NCAP/ADAS 测试工况的配置结构。从 scenarios/*.json 读取。
// 输入：ScenarioParser（或直接代码构造）填充。
// 输出：供 ScenarioEngine 执行测试时使用。
// 第一阶段：仅使用少量字段做模拟；字段已尽量贴合真实工况定义，便于后续扩展。
// =============================================================================

#include <QString>
#include "Waypoint.h"
#include <QList>

struct ScenarioConfig
{
    QString name;             // 工况名称，如 "AEB CCRs 50"
    QString type;             // 工况类型，如 "AEB"、"RCTA"、"LKA"
    QString group;            // 工况组（一级分类），如 "AEB"、"RCTA"、"LKA"；为空归入"未分组"
    QString subgroup;         // 工况子组（二级分类），如 "CCRs"、"CCRs"；为空直接挂在组下
    QString description;
    double  egoTargetSpeed  = 0.0;   // 主车目标速度 m/s
    double  egoStartX = 0.0;
    double  egoStartY = 0.0;
    double  egoStartYaw = 0.0;
    double  targetSpeed     = 0.0;   // 目标物速度 m/s
    double  targetStartX = 0.0;
    double  targetStartY = 0.0;
    double  targetStartYaw = 0.0;
    QList<Waypoint> targetPath;     // 目标物（平板）航点路径；为空时退回单点 targetStart* 逻辑（向后兼容）
    QList<Waypoint> egoPath;        // 主车航点路径；为空时退回 egoStart* + egoTargetSpeed 直线加速逻辑
    QString targetType;             // 目标物类型（车/行人/自行车）
    QString triggerCondition;       // 触发条件
    QString stopCondition;          // 停止条件
    double  maxTestTimeSec  = 30.0; // 最大测试时长
    double  passTtcSec      = 0.0;  // 合格判据 TTC
    double  safetyBoundaryM = 0.0;
    bool    noCollisionRequired = false;
    double  maxLateralDeviationM = 0.0;
    int     repeatCount     = 1;    // 测试次数
    QString sourcePath;
    QString rawJson;
};
