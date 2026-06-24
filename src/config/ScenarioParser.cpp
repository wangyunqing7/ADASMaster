// =============================================================================
// ScenarioParser.cpp —— 见头文件注释
// =============================================================================

#include "ScenarioParser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

bool requireString(const QJsonObject &object, const QString &key, const QString &path,
                   QString *value, QString *error)
{
    const QJsonValue item = object.value(key);
    if (!item.isString() || item.toString().trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("%1.%2 must be a non-empty string").arg(path, key);
        return false;
    }
    *value = item.toString();
    return true;
}

bool requireDouble(const QJsonObject &object, const QString &key, const QString &path,
                   double *value, QString *error)
{
    const QJsonValue item = object.value(key);
    if (!item.isDouble()) {
        if (error) *error = QStringLiteral("%1.%2 must be a number").arg(path, key);
        return false;
    }
    *value = item.toDouble();
    return true;
}

bool requireInt(const QJsonObject &object, const QString &key, const QString &path,
                int *value, QString *error)
{
    double number = 0.0;
    if (!requireDouble(object, key, path, &number, error)) return false;
    *value = static_cast<int>(number);
    return true;
}

bool requireObject(const QJsonObject &object, const QString &key, const QString &path,
                   QJsonObject *value, QString *error)
{
    const QJsonValue item = object.value(key);
    if (!item.isObject()) {
        if (error) *error = QStringLiteral("%1.%2 must be an object").arg(path, key);
        return false;
    }
    *value = item.toObject();
    return true;
}

// 解析 target.waypoints 数组；字段缺失时返回空列表（向后兼容），格式错误时报错。
bool parseWaypoints(const QJsonObject &target, const QString &path,
                    QList<Waypoint> *waypoints, QString *error)
{
    waypoints->clear();
    const QJsonValue item = target.value(QStringLiteral("waypoints"));
    if (item.isUndefined() || item.isNull()) return true;   // 可选字段
    if (!item.isArray()) {
        if (error) *error = QStringLiteral("%1.waypoints must be an array").arg(path);
        return false;
    }
    const QJsonArray array = item.toArray();
    for (int i = 0; i < array.size(); ++i) {
        const QJsonObject wp = array.at(i).toObject();
        const QString wpPath = QStringLiteral("%1.waypoints[%2]").arg(path).arg(i);
        Waypoint point;
        if (!requireDouble(wp, QStringLiteral("x"), wpPath, &point.x, error) ||
            !requireDouble(wp, QStringLiteral("y"), wpPath, &point.y, error) ||
            !requireDouble(wp, QStringLiteral("speed"), wpPath, &point.speed, error)) {
            return false;
        }
        waypoints->append(point);
    }
    return true;
}

} // namespace

bool ScenarioParser::parseFile(const QString &path, ScenarioConfig *config, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    return parseJson(file.readAll(), path, config, error);
}

bool ScenarioParser::parseJson(const QByteArray &json, const QString &source,
                               ScenarioConfig *config, QString *error)
{
    if (!config) {
        if (error) *error = QStringLiteral("Output ScenarioConfig is null");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error) *error = QStringLiteral("%1: invalid JSON at offset %2: %3")
                                .arg(source).arg(parseError.offset).arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    QJsonObject ego;
    QJsonObject target;
    ScenarioConfig parsed;
    parsed.sourcePath = source;
    parsed.rawJson = QString::fromUtf8(document.toJson(QJsonDocument::Compact));

    if (!requireString(root, QStringLiteral("name"), QStringLiteral("root"), &parsed.name, error) ||
        !requireString(root, QStringLiteral("type"), QStringLiteral("root"), &parsed.type, error) ||
        !requireObject(root, QStringLiteral("ego"), QStringLiteral("root"), &ego, error) ||
        !requireObject(root, QStringLiteral("target"), QStringLiteral("root"), &target, error) ||
        !requireDouble(root, QStringLiteral("max_test_time_sec"), QStringLiteral("root"),
                       &parsed.maxTestTimeSec, error) ||
        !requireInt(root, QStringLiteral("repeat_count"), QStringLiteral("root"),
                    &parsed.repeatCount, error)) {
        return false;
    }

    if (!requireDouble(ego, QStringLiteral("target_speed_mps"), QStringLiteral("ego"),
                       &parsed.egoTargetSpeed, error) ||
        !requireDouble(ego, QStringLiteral("start_x"), QStringLiteral("ego"),
                       &parsed.egoStartX, error) ||
        !requireDouble(ego, QStringLiteral("start_y"), QStringLiteral("ego"),
                       &parsed.egoStartY, error) ||
        !requireDouble(ego, QStringLiteral("start_yaw"), QStringLiteral("ego"),
                       &parsed.egoStartYaw, error)) {
        return false;
    }

    // 主车航点路径（可选）
    if (!parseWaypoints(ego, QStringLiteral("ego"), &parsed.egoPath, error)) {
        return false;
    }

    if (!requireString(target, QStringLiteral("type"), QStringLiteral("target"),
                       &parsed.targetType, error) ||
        !requireDouble(target, QStringLiteral("speed_mps"), QStringLiteral("target"),
                       &parsed.targetSpeed, error)) {
        return false;
    }

    // LKA 没有物理目标物坐标，使用 0 作为占位但不参与碰撞判定。
    if (target.contains(QStringLiteral("start_x"))) {
        if (!requireDouble(target, QStringLiteral("start_x"), QStringLiteral("target"),
                           &parsed.targetStartX, error) ||
            !requireDouble(target, QStringLiteral("start_y"), QStringLiteral("target"),
                           &parsed.targetStartY, error) ||
            !requireDouble(target, QStringLiteral("start_yaw"), QStringLiteral("target"),
                           &parsed.targetStartYaw, error)) {
            return false;
        }
    } else if (parsed.type.compare(QStringLiteral("LKA"), Qt::CaseInsensitive) != 0) {
        if (error) *error = QStringLiteral("target.start_x must be a number");
        return false;
    }

    // 解析可选的航点路径（缺失时 targetPath 为空，向后兼容）
    if (!parseWaypoints(target, QStringLiteral("target"), &parsed.targetPath, error)) {
        return false;
    }

    parsed.description = root.value(QStringLiteral("description")).toString();
    // 组/子组（可选）；缺失时组默认取 type，子组为空（直接挂在组下）
    parsed.group = root.value(QStringLiteral("group")).toString();
    if (parsed.group.isEmpty()) parsed.group = parsed.type;
    parsed.subgroup = root.value(QStringLiteral("subgroup")).toString();
    parsed.triggerCondition = root.value(QStringLiteral("trigger_condition")).toString();
    parsed.stopCondition = root.value(QStringLiteral("stop_condition")).toString();
    parsed.safetyBoundaryM = root.value(QStringLiteral("safety_boundary_m")).toDouble();

    const QJsonObject criteria = root.value(QStringLiteral("pass_criteria")).toObject();
    parsed.passTtcSec = criteria.value(QStringLiteral("max_ttc_sec")).toDouble();
    parsed.noCollisionRequired = criteria.value(QStringLiteral("no_collision")).toBool();
    parsed.maxLateralDeviationM = criteria.value(QStringLiteral("max_lateral_deviation_m")).toDouble();

    *config = parsed;
    if (error) error->clear();
    return true;
}
