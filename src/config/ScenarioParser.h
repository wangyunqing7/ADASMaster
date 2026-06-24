#pragma once

// =============================================================================
// ScenarioParser.h
// 功能：严格解析并校验 ADAS 工况 JSON，返回字段路径明确的错误。
// =============================================================================

#include "../scenario/ScenarioConfig.h"
#include <QByteArray>
#include <QString>

class ScenarioParser
{
public:
    static bool parseFile(const QString &path, ScenarioConfig *config, QString *error = nullptr);
    static bool parseJson(const QByteArray &json, const QString &source,
                          ScenarioConfig *config, QString *error = nullptr);
};
