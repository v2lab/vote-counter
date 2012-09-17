#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QObject>
#include <QGraphicsScene>
#include <opencv2/flann/flann.hpp>

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

    enum ItemData {
        ITEM_NAME,
    };

    static const int COLOR_GRADATIONS = 3;
    explicit SnapshotModel(const QString& path, QObject *parent);
    ~SnapshotModel();

    QImage getImage(const QString& tag);
    cv::Mat getMatrix(const QString& tag);
    void setImage(const QString& tag, const QImage& img);
    void setMatrix(const QString& tag, const cv::Mat& matrix);

    QGraphicsScene * scene() { return m_scene; }
signals:

public slots:
    void setMode(Mode m);
    void setTrainMode(const QString& tag);
    void pick(int x, int y);
    void unpick(int x, int y);
    void clearLayer(const QString& name);
    void clearCurrentTrainLayer();
    void trainColors();
    void countCards();

protected:
    static bool s_staticInitialized;
    static QSet< QString > s_cacheableImages;
    static QSet< QString > s_resizedImages;
    static QSet< QString > s_colorNames;

    QString m_originalPath;
    QDir m_cacheDir;
    QMap< QString, QImage > m_images;
    QMap< QString, cv::Mat > m_matrices;

    QGraphicsScene * m_scene;
    Mode m_mode;
    QString m_color;
    QMap< QString, QList< QPoint > > m_colorPicks;
    QMap< QString, QPen > m_pens;
    QMap< QString, QGraphicsItem *> m_layers;

    typedef float ColorType;
    typedef cv::flann::L2<ColorType> ColorDistance;
    cv::flann::GenericIndex< ColorDistance > * m_flann;

    void addCross(int x, int y);
    void updateViews();
    void saveData();
    void loadData();
    QGraphicsItem * layer(const QString& name);
    void selectByFlood(int x, int y);

    virtual bool eventFilter(QObject *, QEvent *);
};

#endif // SNAPSHOTMODEL_HPP
