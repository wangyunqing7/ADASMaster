// =============================================================================
// ScenarioCatalog.cpp —— 见头文件注释
// =============================================================================

#include "ScenarioCatalog.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace {
QString kindText(ScenarioNodeKind kind)
{
    switch (kind) {
    case ScenarioNodeKind::Group: return QStringLiteral("group");
    case ScenarioNodeKind::Subgroup: return QStringLiteral("subgroup");
    case ScenarioNodeKind::Scenario: return QStringLiteral("scenario");
    }
    return {};
}

bool parseKind(const QString &text, ScenarioNodeKind *kind)
{
    if (text == QLatin1String("group")) *kind = ScenarioNodeKind::Group;
    else if (text == QLatin1String("subgroup")) *kind = ScenarioNodeKind::Subgroup;
    else if (text == QLatin1String("scenario")) *kind = ScenarioNodeKind::Scenario;
    else return false;
    return true;
}
}

ScenarioCatalog::ScenarioCatalog(const QString &storagePath, QObject *parent)
    : QObject(parent), m_storagePath(storagePath)
{
}

bool ScenarioCatalog::load(const QList<ScenarioConfig> &bundled, QString *error)
{
    m_nodes.clear();
    QFile file(m_storagePath);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) *error = file.errorString();
            return false;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject() || !doc.object().value(QStringLiteral("nodes")).isArray()) {
            if (error) *error = QStringLiteral("场景目录格式无效");
            return false;
        }
        for (const QJsonValue &value : doc.object().value(QStringLiteral("nodes")).toArray()) {
            const QJsonObject object = value.toObject();
            ScenarioNode n;
            if (!parseKind(object.value(QStringLiteral("kind")).toString(), &n.kind))
                continue;
            n.id = object.value(QStringLiteral("id")).toString();
            n.parentId = object.value(QStringLiteral("parentId")).toString();
            n.name = object.value(QStringLiteral("name")).toString();
            n.order = object.value(QStringLiteral("order")).toInt();
            if (!n.id.isEmpty() && !n.name.isEmpty()) m_nodes.append(n);
        }
    }

    bool imported = false;
    for (const ScenarioConfig &scenario : bundled) {
        const QString groupName = scenario.group.trimmed().isEmpty()
            ? QStringLiteral("未分组") : scenario.group.trimmed();
        QString groupId = findChild({}, ScenarioNodeKind::Group, groupName);
        if (groupId.isEmpty()) { groupId = createImportedNode(ScenarioNodeKind::Group, {}, groupName); imported = true; }
        const QString subgroupName = scenario.subgroup.trimmed().isEmpty()
            ? QStringLiteral("默认") : scenario.subgroup.trimmed();
        QString subgroupId = findChild(groupId, ScenarioNodeKind::Subgroup, subgroupName);
        if (subgroupId.isEmpty()) { subgroupId = createImportedNode(ScenarioNodeKind::Subgroup, groupId, subgroupName); imported = true; }
        if (findChild(subgroupId, ScenarioNodeKind::Scenario, scenario.name).isEmpty()) {
            createImportedNode(ScenarioNodeKind::Scenario, subgroupId, scenario.name);
            imported = true;
        }
    }
    if (error) error->clear();
    if (imported && !m_storagePath.isEmpty()) return save(error);
    return true;
}

bool ScenarioCatalog::save(QString *error) const
{
    if (m_storagePath.isEmpty()) return true;
    QDir().mkpath(QFileInfo(m_storagePath).absolutePath());
    QJsonArray nodes;
    for (const ScenarioNode &n : m_nodes) {
        nodes.append(QJsonObject{
            {QStringLiteral("id"), n.id},
            {QStringLiteral("parentId"), n.parentId},
            {QStringLiteral("name"), n.name},
            {QStringLiteral("kind"), kindText(n.kind)},
            {QStringLiteral("order"), n.order}
        });
    }
    QSaveFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                         {QStringLiteral("nodes"), nodes}}).toJson());
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    if (error) error->clear();
    return true;
}

QString ScenarioCatalog::createNode(ScenarioNodeKind kind, const QString &parentId,
                                    const QString &name, QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error) *error = QStringLiteral("名称不能为空");
        return {};
    }
    if (!validParent(kind, parentId)) {
        if (error) *error = QStringLiteral("父级节点不符合三层结构");
        return {};
    }
    if (!findChild(parentId, kind, trimmed).isEmpty()) {
        if (error) *error = QStringLiteral("同级名称重复");
        return {};
    }
    ScenarioNode n;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.parentId = parentId;
    n.name = trimmed;
    n.kind = kind;
    n.order = m_nodes.size();
    m_nodes.append(n);
    QString saveError;
    if (!save(&saveError)) {
        m_nodes.removeLast();
        if (error) *error = saveError;
        return {};
    }
    if (error) error->clear();
    emit changed();
    return n.id;
}

QVariantList ScenarioCatalog::tree() const
{
    QVariantList groups;
    for (const ScenarioNode &group : m_nodes) {
        if (group.kind != ScenarioNodeKind::Group) continue;
        QVariantList subgroups;
        for (const ScenarioNode &subgroup : m_nodes) {
            if (subgroup.kind != ScenarioNodeKind::Subgroup || subgroup.parentId != group.id) continue;
            QVariantList scenarios;
            QStringList scenarioNames;
            for (const ScenarioNode &scenario : m_nodes) {
                if (scenario.kind != ScenarioNodeKind::Scenario || scenario.parentId != subgroup.id) continue;
                scenarios.append(QVariantMap{{QStringLiteral("id"), scenario.id},
                                             {QStringLiteral("name"), scenario.name}});
                scenarioNames.append(scenario.name);
            }
            subgroups.append(QVariantMap{{QStringLiteral("id"), subgroup.id},
                                         {QStringLiteral("name"), subgroup.name},
                                         {QStringLiteral("subgroup"), subgroup.name},
                                         {QStringLiteral("children"), scenarios},
                                         {QStringLiteral("scenarioNodes"), scenarios},
                                         {QStringLiteral("scenarios"), scenarioNames}});
        }
        groups.append(QVariantMap{{QStringLiteral("id"), group.id},
                                  {QStringLiteral("name"), group.name},
                                  {QStringLiteral("group"), group.name},
                                  {QStringLiteral("children"), subgroups},
                                  {QStringLiteral("subgroups"), subgroups}});
    }
    return groups;
}

QString ScenarioCatalog::nodeIdForScenario(const QString &scenarioName) const
{
    for (const ScenarioNode &n : m_nodes)
        if (n.kind == ScenarioNodeKind::Scenario && n.name.compare(scenarioName, Qt::CaseInsensitive) == 0)
            return n.id;
    return {};
}

const ScenarioNode *ScenarioCatalog::node(const QString &id) const
{
    for (const ScenarioNode &n : m_nodes) if (n.id == id) return &n;
    return nullptr;
}

bool ScenarioCatalog::validParent(ScenarioNodeKind kind, const QString &parentId) const
{
    if (kind == ScenarioNodeKind::Group) return parentId.isEmpty();
    const ScenarioNode *parent = node(parentId);
    if (!parent) return false;
    if (kind == ScenarioNodeKind::Subgroup) return parent->kind == ScenarioNodeKind::Group;
    return parent->kind == ScenarioNodeKind::Subgroup;
}

QString ScenarioCatalog::findChild(const QString &parentId, ScenarioNodeKind kind,
                                   const QString &name) const
{
    for (const ScenarioNode &n : m_nodes) {
        if (n.kind == kind && n.parentId == parentId &&
            n.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return n.id;
    }
    return {};
}

QString ScenarioCatalog::createImportedNode(ScenarioNodeKind kind, const QString &parentId,
                                            const QString &name)
{
    ScenarioNode n;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.parentId = parentId;
    n.name = name;
    n.kind = kind;
    n.order = m_nodes.size();
    m_nodes.append(n);
    return n.id;
}
