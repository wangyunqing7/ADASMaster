#pragma once

// =============================================================================
// AlarmFilterProxyModel.h
// 功能：报警列表的筛选代理模型。基于 QSortFilterProxyModel，按等级/来源/
//       是否未确认/关键词组合过滤 AlarmListModel，供报警中心页面绑定。
// 输入：sourceModel 为 AlarmListModel；QML 通过 4 个 Q_PROPERTY 设置筛选条件。
// 输出：QAbstractListModel 视图（自动继承 source 的 roleNames），ListView 直接绑定。
// =============================================================================

#include <QSortFilterProxyModel>

class AlarmFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int activeLevel READ activeLevel WRITE setActiveLevel NOTIFY activeLevelChanged)     // -1=全部, 0..3
    Q_PROPERTY(int activeSource READ activeSource WRITE setActiveSource NOTIFY activeSourceChanged) // -1=全部, 0..5
    Q_PROPERTY(bool onlyUnacked READ onlyUnacked WRITE setOnlyUnacked NOTIFY onlyUnackedChanged)
    Q_PROPERTY(QString keyword READ keyword WRITE setKeyword NOTIFY keywordChanged)

public:
    explicit AlarmFilterProxyModel(QObject *parent = nullptr);

    int activeLevel() const  { return m_activeLevel; }
    int activeSource() const { return m_activeSource; }
    bool onlyUnacked() const { return m_onlyUnacked; }
    QString keyword() const  { return m_keyword; }

    void setActiveLevel(int level);
    void setActiveSource(int source);
    void setOnlyUnacked(bool on);
    void setKeyword(const QString &kw);

    // 把代理视图行号映射回 source(AlarmListModel) 行号，供"确认"等操作
    Q_INVOKABLE int mapRowToSource(int proxyRow) const;

signals:
    void activeLevelChanged();
    void activeSourceChanged();
    void onlyUnackedChanged();
    void keywordChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    int m_activeLevel = -1;     // -1 表示不限等级
    int m_activeSource = -1;    // -1 表示不限来源
    bool m_onlyUnacked = false;
    QString m_keyword;
};
