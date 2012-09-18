#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QObject>
#include <QGraphicsScene>
#include <opencv2/flann/flann.hpp>

typedef QSet< QString > QStringSet;

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
        ITEM_FULLNAME
    };

    static const int COLOR_GRADATIONS = 5;

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
    void on_clearTrainLayer_clicked();
    void on_learn_clicked();
    void on_count_clicked();
    void on_trainModeGroup_buttonClicked( QAbstractButton * button );
    void on_colorDiffOn_pressed();
    void on_colorDiffOn_released();
    void on_colorDiffThreshold_valueChanged();
    void on_colorDiffThreshold_sliderPressed();
    void on_colorDiffThreshold_sliderReleased();

protected:
    static QStringSet s_cacheableImages;
    static QStringSet s_resizedImages;
    static QStringList s_colorNames;

    QString m_originalPath;
    QDir m_parentDir, m_cacheDir;
    QMap< QString, QImage > m_images;
    QMap< QString, cv::Mat > m_matrices;

    QGraphicsScene * m_scene;
    Mode m_mode;
    QString m_color;
    QMap< QString, QList< QPoint > > m_colorPicks;
    QMap< QString, QPen > m_pens;
    QMap< QString, QGraphicsItem *> m_layers;
    bool m_showColorDiff;

    typedef float ColorType;
    typedef cv::flann::L2<ColorType> ColorDistance;
    cv::flann::GenericIndex< ColorDistance > * m_flann;

    void addCross(int x, int y);
    void updateViews();
    void saveData();
    void loadData();
    QGraphicsItem * layer(const QString& name);
    void selectByFlood(int x, int y);
    void showFeatures();
    void buildFlannRecognizer();

    void classifyPixels();
    void computeColorDiff();
    void countCards();

    QVariant uiValue(const QString& name);

    virtual bool eventFilter(QObject *, QEvent *);
};

#endif // SNAPSHOTMODEL_HPP
