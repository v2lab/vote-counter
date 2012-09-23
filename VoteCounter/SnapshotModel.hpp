#ifndef SNAPSHOTMODEL_HPP
#define SNAPSHOTMODEL_HPP

#include <QtCore>
#include <QtGui>
#include <opencv2/flann/flann.hpp>

class MouseLogic;

typedef QSet< QString > QStringSet;

class SnapshotModel : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        INERT = 0,
        TRAIN,
        COUNT
    };

    enum ItemData {
        ITEM_NAME,
        ITEM_FULLNAME,
        POLYGONS_CONTOUR
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

    void mergeContours(QRectF rect);
    void clearContours(QRectF rect);

    void on_resetLayer_clicked();
    void on_learn_clicked();
    void on_count_clicked();
    void on_trainModeGroup_buttonClicked( QAbstractButton * button );
    void on_colorDiffOn_pressed();
    void on_colorDiffOn_released();
    void on_colorDiffThreshold_valueChanged();
    void on_colorDiffThreshold_sliderPressed();
    void on_colorDiffThreshold_sliderReleased();
    void on_sizeFilter_valueChanged();
    void on_mouseLogic_pointClicked(QPointF point, Qt::MouseButton button, Qt::KeyboardModifiers mods);
    void on_mouseLogic_rectUpdated(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods);
    void on_mouseLogic_rectSelected(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods);

protected:
    static QStringSet s_cacheableImages;
    static QStringSet s_resizedImages;
    static QStringList s_colorNames;
    static QStringList s_persistentMasks;

    QString m_originalPath;
    QDir m_parentDir, m_cacheDir;
    QMap< QString, QImage > m_images;
    QMap< QString, cv::Mat > m_matrices;

    QGraphicsScene * m_scene;
    MouseLogic * m_mouseLogic;
    QGraphicsRectItem * m_rectSelection;

    Mode m_mode;
    QString m_color;
    QMap< QString, QPen > m_pens;
    QMap< QString, QGraphicsItem *> m_layers;
    bool m_showColorDiff;

    typedef float ColorType;
    typedef cv::flann::L2<ColorType> ColorDistance;
    cv::flann::GenericIndex< ColorDistance > * m_flann;

    void updateViews();
    void saveData();
    void loadData();
    QGraphicsItem * layer(const QString& name);
    void showPalette();
    void buildFlannRecognizer();

    void classifyPixels();
    void computeColorDiff();
    void countCards();

    void addContour(const QPolygonF& contour, const QString& name, bool paintToMask = false);
    void floodPickContour(int x, int y, int fuzz, const QString& layerName);
    QList< QPolygon > detectContours(const QString& maskAndLayerName, bool addToScene = true, cv::Rect maskROI = cv::Rect(), int simple = 1);

    template<typename PItem>
    QList<PItem> filterItems(const QList<QGraphicsItem*>& list) {
        QList<PItem> result;
        foreach(QGraphicsItem* item, list) {
            PItem cast = qgraphicsitem_cast<PItem>(item);
            if (cast)
                result << cast;
        }
        return result;
    }

#define foreach_item(PItem, i, list) foreach(PItem i, filterItems<PItem>(list))

    QVariant uiValue(const QString& name);
};

#endif // SNAPSHOTMODEL_HPP
