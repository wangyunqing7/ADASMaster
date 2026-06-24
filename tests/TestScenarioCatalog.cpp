// =============================================================================
// TestScenarioCatalog.cpp
// 功能：验证三层场景目录的父子约束、同级去重与 JSON 持久化。
// =============================================================================

#include <QtTest>
#include <QTemporaryDir>

#include "src/scenario/ScenarioCatalog.h"

class TestScenarioCatalog : public QObject
{
    Q_OBJECT

private slots:
    void createsStrictThreeLevelNodes()
    {
        QTemporaryDir dir;
        ScenarioCatalog catalog(dir.filePath(QStringLiteral("scenario_catalog.json")));
        QString error;
        QVERIFY2(catalog.load({}, &error), qPrintable(error));

        const QString groupId = catalog.createNode(ScenarioNodeKind::Group, {},
                                                   QStringLiteral("AEB"), &error);
        QVERIFY2(!groupId.isEmpty(), qPrintable(error));
        const QString subgroupId = catalog.createNode(ScenarioNodeKind::Subgroup, groupId,
                                                      QStringLiteral("CCRs"), &error);
        QVERIFY2(!subgroupId.isEmpty(), qPrintable(error));
        const QString scenarioId = catalog.createNode(ScenarioNodeKind::Scenario, subgroupId,
                                                      QStringLiteral("CCRs 50"), &error);
        QVERIFY2(!scenarioId.isEmpty(), qPrintable(error));

        QVERIFY(catalog.createNode(ScenarioNodeKind::Scenario, groupId,
                                   QStringLiteral("Invalid"), &error).isEmpty());
        QVERIFY(catalog.createNode(ScenarioNodeKind::Scenario, subgroupId,
                                   QStringLiteral("CCRs 50"), &error).isEmpty());
    }

    void persistsEmptyGroupsAndSubgroups()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("scenario_catalog.json"));
        {
            ScenarioCatalog catalog(path);
            QString error;
            QVERIFY2(catalog.load({}, &error), qPrintable(error));
            const QString groupId = catalog.createNode(ScenarioNodeKind::Group, {},
                                                       QStringLiteral("VRU"), &error);
            QVERIFY2(!groupId.isEmpty(), qPrintable(error));
            QVERIFY2(!catalog.createNode(ScenarioNodeKind::Subgroup, groupId,
                                         QStringLiteral("Pedestrian"), &error).isEmpty(),
                     qPrintable(error));
            QVERIFY2(catalog.save(&error), qPrintable(error));
        }

        ScenarioCatalog reloaded(path);
        QString error;
        QVERIFY2(reloaded.load({}, &error), qPrintable(error));
        const QVariantList tree = reloaded.tree();
        QCOMPARE(tree.size(), 1);
        const QVariantMap group = tree.first().toMap();
        QCOMPARE(group.value(QStringLiteral("name")).toString(), QStringLiteral("VRU"));
        QCOMPARE(group.value(QStringLiteral("children")).toList().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestScenarioCatalog)
#include "TestScenarioCatalog.moc"
