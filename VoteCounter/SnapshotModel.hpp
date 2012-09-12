#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QObject>
#include <QGraphicsScene>

class SnapshotModel : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        INERT = 0,
        TRAIN,
        MASK,
        COUNT
    };
    explicit SnapshotModel(const QString& path, QObject *parent);
    ~SnapshotModel();

    QImage image(const QString& tag);
    void setImage(const QString& tag, const QImage& img);

    QGraphicsScene * scene() { return m_scene; }
signals:

public slots:
    void setMode(Mode m) { m_mode = m; }
    void setTrainMode(const QString& tag);
    void pick(int x, int y);

protected:
    QDir m_cacheDir;
    QMap< QString, QImage > m_images;
    QGraphicsScene * m_scene;
    Mode m_mode;
    QString m_color;
    QMap< QString, QList< QPoint > > m_colorPicks;

    void addCross(int x, int y, const QColor& color);
    void updateViews();
    void saveData();
    void loadData();

    virtual bool eventFilter(QObject *, QEvent *);
};

#endif // SNAPSHOTMODEL_HPP
