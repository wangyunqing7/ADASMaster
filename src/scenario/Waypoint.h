#pragma once

// =============================================================================
// Waypoint.h
// 功能：定义目标物航点结构。一个航点表示目标物路径上的一个途经点。
// 输入：由 ScenarioParser 从 JSON 的 target.waypoints 数组填充。
// 输出：供 ScenarioConfig.targetPath 组装、ScenarioEngine 航点插值使用。
// =============================================================================

#include <QMetaType>
#include <QString>

struct Waypoint
{
    double x = 0.0;       // 航点 X 坐标（米，场地坐标系）
    double y = 0.0;       // 航点 Y 坐标（米，场地坐标系）
    double speed = 0.0;   // 到达该航点时应达到的目标速度（m/s）

    // 比较运算符，便于测试断言
    bool operator==(const Waypoint &other) const
    {
        return qFuzzyCompare(x, other.x) && qFuzzyCompare(y, other.y)
               && qFuzzyCompare(speed, other.speed);
    }
};

Q_DECLARE_METATYPE(Waypoint)
