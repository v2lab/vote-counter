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

    QImage image(const QString& tag, bool mask = false);
    void setImage(const QString& tag, const QImage& img);

    QGraphicsScene * scene() { return m_scene; }
signals:

public slots:
    void setMode(Mode m) { m_mode = m; }
    void setTrainMode(const QString& tag);
    void pick(int x, int y);
    void unpick(int x, int y);
    void clearLayer();
    void trainColors();

protected:
    QDir m_cacheDir;
    QMap< QString, QImage > m_images;
    QGraphicsScene * m_scene;
    Mode m_mode;
    QString m_color;
    QMap< QString, QList< QPoint > > m_colorPicks;
    QMap< QString, QPen > m_pens;
    QMap< QString, QGraphicsItem *> m_layers;
    QMap< QString, QGraphicsItem *> m_displayers;
    QMap< QString, cv::Mat > m_matrices;

    void addCross(int x, int y);
    void updateViews();
    void saveData();
    void loadData();
    QGraphicsItem * layer(const QString& name);
    void selectByFlood(int x, int y);
    cv::Mat matrixFromImage(const QString& tag);

    virtual bool eventFilter(QObject *, QEvent *);
};

#endif // SNAPSHOTMODEL_HPP
