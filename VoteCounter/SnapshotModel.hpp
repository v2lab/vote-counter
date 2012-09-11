#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QObject>
#include <QGraphicsScene>

class SnapshotModel : public QObject
{
    Q_OBJECT
public:
    explicit SnapshotModel(const QString& path, QObject *parent);

    QImage image(const QString& tag);
    void setImage(const QString& tag, const QImage& img);

    QGraphicsScene * scene() { return &m_scene; }
signals:

public slots:

protected:
    QDir m_cacheDir;
    QMap< QString, QImage > m_images;
    QGraphicsScene m_scene;
};

#endif // SNAPSHOTMODEL_HPP
