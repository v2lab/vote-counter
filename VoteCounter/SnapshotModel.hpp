#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QObject>

class SnapshotModel : public QObject
{
    Q_OBJECT
public:
    explicit SnapshotModel(const QString& path, QObject *parent);

    QImage image(const QString& tag);
    void setImage(const QString& tag, const QImage& img);
signals:

public slots:

protected:
    QDir m_cacheDir;
    QMap< QString, QImage > m_images;
};

#endif // SNAPSHOTMODEL_HPP
