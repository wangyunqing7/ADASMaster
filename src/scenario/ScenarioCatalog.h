#pragma once

// =============================================================================
// ScenarioCatalog.h
// 功能：管理场景组→场景子组→场景的严格三层目录，并持久化空目录节点。
// 输入：内置 ScenarioConfig 列表与用户创建请求。
// 输出：QML 可消费的树结构及目录变更信号。
// =============================================================================

#include "ScenarioConfig.h"

#include <QObject>
#include <QVariantList>

enum class ScenarioNodeKind {
    Group,
    Subgroup,
    Scenario
};

struct ScenarioNode {
    QString id;
    QString parentId;
    QString name;
    ScenarioNodeKind kind = ScenarioNodeKind::Group;
    int order = 0;
};

class ScenarioCatalog : public QObject
{
    Q_OBJECT

public:
    explicit ScenarioCatalog(const QString &storagePath, QObject *parent = nullptr);

    bool load(const QList<ScenarioConfig> &bundled, QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    QString createNode(ScenarioNodeKind kind, const QString &parentId,
                       const QString &name, QString *error = nullptr);
    QVariantList tree() const;
    QString nodeIdForScenario(const QString &scenarioName) const;
    const QList<ScenarioNode> &nodes() const { return m_nodes; }

signals:
    void changed();

private:
    const ScenarioNode *node(const QString &id) const;
    bool validParent(ScenarioNodeKind kind, const QString &parentId) const;
    QString findChild(const QString &parentId, ScenarioNodeKind kind,
                      const QString &name) const;
    QString createImportedNode(ScenarioNodeKind kind, const QString &parentId,
                               const QString &name);

    QList<ScenarioNode> m_nodes;
    QString m_storagePath;
};

