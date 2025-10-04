#pragma once
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QString>
#include <QDate>
#include <QVariant>
#include <QStringList>
#include <QRegularExpression>
#include <vector>
#include <functional>

// ==============================
// Helper: Semantic Version -> sortable key with suffix weights and sequence numbers
inline QList<QVariant> semVerToSortableWithSuffixSequence ( const QString& versionStr )
{
    QList<QVariant> key;

    // Regex for SemVer: major.minor.patch-suffix
    QRegularExpression      semVerRegex ( R"(^(\d+)\.(\d+)\.(\d+)(?:-?([0-9A-Za-z]+))?$)" );
    QRegularExpressionMatch match = semVerRegex.match ( versionStr );

    if ( !match.hasMatch() )
    {
        key.append ( versionStr ); // fallback: plain text
        return key;
    }

    int     major  = match.captured ( 1 ).toInt();
    int     minor  = match.captured ( 2 ).toInt();
    int     patch  = match.captured ( 3 ).toInt();
    QString suffix = match.captured ( 4 ); // may be empty

    key.append ( major );
    key.append ( minor );
    key.append ( patch );

    int weight         = 0;
    int sequenceNumber = 0;

    if ( !suffix.isEmpty() )
    {
        static const QMap<QString, int> suffixWeights = { { "alpha", -3 },
                                                          { "beta", -2 },
                                                          { "rc", -1 },
                                                          { "", 0 },
                                                          { "git", 1 }, // for compatibility with old dev versions
                                                          { "dev", 1 },
                                                          { "patch", 2 },
                                                          { "nightly", 3 } };

        // Split label + optional numeric sequence, e.g., "alpha1" -> "alpha", 1 or "dev-XXXXXXXX"
        // Doesn't make sense to sort dev, so leave sequenceNumber as 0 in that case
        QRegularExpression      labelNumRegex ( R"(^([a-zA-Z]+)(?:(\d*)|(-.*))$)" );
        QRegularExpressionMatch lm = labelNumRegex.match ( suffix );
        QString                 suffixLabel;
        if ( lm.hasMatch() )
        {
            suffixLabel    = lm.captured ( 1 );
            QString seqStr = lm.captured ( 2 );
            if ( !seqStr.isEmpty() )
                sequenceNumber = seqStr.toInt();
        }
        else
        {
            suffixLabel = suffix;
        }

        weight = suffixWeights.value ( suffixLabel, 0 );
    }

    key.append ( weight );         // suffix weight for sorting
    key.append ( sequenceNumber ); // numeric sequence for same suffix

    return key;
}

// ==============================
// MappedTreeWidget class
class MappedTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    using MapFunction = std::function<QVariant ( const QString& )>;

    explicit MappedTreeWidget ( QWidget* parent = nullptr ) : QTreeWidget ( parent ) {}

    // Set mapping functions for all columns
    void setColumnMappers ( const std::vector<MapFunction>& mappers ) { columnMappers = mappers; }

    // Set a mapper for a single column
    void setColumnMapper ( int column, MapFunction mapper )
    {
        if ( column < 0 )
            return;

        // Resize vector if necessary
        if ( static_cast<size_t> ( column ) >= columnMappers.size() )
        {
            columnMappers.resize ( column + 1, nullptr );
        }

        columnMappers[column] = mapper;
    }

    // Add top-level item
    QTreeWidgetItem* addItem ( const QStringList& texts ) { return addItem ( nullptr, texts ); }

    // Add child item under parent
    QTreeWidgetItem* addItem ( QTreeWidgetItem* parent, const QStringList& texts )
    {
        QTreeWidgetItem* item = parent ? new QTreeWidgetItem ( parent ) : new QTreeWidgetItem ( this );

        for ( int col = 0; col < texts.size(); ++col )
        {
            const QString& text = texts[col];
            item->setText ( col, text );

            if ( col < static_cast<int> ( columnMappers.size() ) && columnMappers[col] )
            {
                QVariant key = columnMappers[col]( text );
                item->setData ( col, Qt::UserRole, key );
            }
        }

        return item;
    }

private:
    std::vector<MapFunction> columnMappers;
};
