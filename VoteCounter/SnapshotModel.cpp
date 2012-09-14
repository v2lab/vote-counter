#include "SnapshotModel.hpp"
#include "ProjectSettings.hpp"

#include "QOpenCV.hpp"
using namespace QOpenCV;

#include <qt-json/json.h>
using namespace QtJson;

#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QVector>
#include <QImage>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPolygonItem>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>

SnapshotModel::SnapshotModel(const QString& path, QObject *parent) :
    QObject(parent), m_scene(new QGraphicsScene(this)),
    m_mode(INERT), m_color("green"), m_flann(0)
{
    m_pens["white"] = QPen(Qt::white);
    m_pens["thick-red"] = QPen(Qt::red, 2);

    m_scene->setObjectName("scene"); // so we can autoconnect signals

    qDebug() << "Loading" << qPrintable(path);

    // check if cache is present, create otherwise
    QFileInfo fi(path);

    QDir parent_dir = fi.absoluteDir();
    m_cacheDir = parent_dir.filePath( fi.baseName() + ".cache" );
    if (!m_cacheDir.exists()) {
        qDebug() << "creating cache directory" << qPrintable(m_cacheDir.path());
        parent_dir.mkdir( m_cacheDir.dirName() );
    }

    // try to load cached working image
    int limit = projectSettings().value("size_limit", 640).toInt();
    QImage working = image("working");
    if (working.isNull() || std::max(working.width(), working.height()) != limit ) {
        // no working image: create
        qDebug() << "scaling down" << qPrintable(path) << "to" << limit;
        QImage orig(path);
        working = orig.scaled( limit, limit, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        setImage("working", working);
    }

    // add the image to the scene
    m_scene->addPixmap( QPixmap::fromImage( working ) );
    m_scene->installEventFilter(this);

    loadData();

    qDebug() << qPrintable(path) << "loaded";
}

SnapshotModel::~SnapshotModel()
{
    qDebug() << "closing snapshot...";
    saveData();
}

QImage SnapshotModel::image(const QString &tag, bool mask)
{
    if (!m_images.contains(tag)) {
         QString path = m_cacheDir.filePath(tag + ".png");
         QImage img(path);
         if (img.isNull())
             return img;
         if (mask)
             img = img.convertToFormat(QImage::Format_Indexed8, greyTable());
         else if (img.format() != QImage::Format_RGB888) {
             img = img.convertToFormat(QImage::Format_RGB888);
         }
         m_images[tag] = img;
    }
    return m_images[tag];
}

void SnapshotModel::setImage(const QString &tag, const QImage &img)
{
    m_images[tag] = img;
    QString path = m_cacheDir.filePath(tag + ".png");
    img.save( path );
}

bool SnapshotModel::eventFilter(QObject * target, QEvent * event)
{
    if (m_scene == qobject_cast<QGraphicsScene*>(target)) {
        switch( event->type() ) {
        case QEvent::GraphicsSceneMousePress: {
            QGraphicsSceneMouseEvent * mevent = dynamic_cast<QGraphicsSceneMouseEvent *>(event);
            // mevent->scenePos() is pixel coordinates of the pick
            if (mevent->button() == Qt::LeftButton) {
                pick( mevent->scenePos().x(), mevent->scenePos().y() );
            } else {
                unpick(mevent->scenePos().x(), mevent->scenePos().y());
            }
            break;
        }
        default:
            break;
        }
    }

    return true;
}

void SnapshotModel::pick(int x, int y)
{
    QImage working = image("working");

    if (m_mode == TRAIN) {
        if (working.rect().contains(x,y)) {
            addCross(x,y);
            selectByFlood(x,y);
            m_colorPicks[m_color].append( QPoint(x,y) ); // this is for saving / restoring
        }
    }
}

void SnapshotModel::setTrainMode(const QString &tag)
{
    if (tag.toLower() == "mask") {
        setMode(MASK);
    } else {
        setMode(TRAIN);
        m_color = tag;
    }

    updateViews();
}

void SnapshotModel::addCross(int x, int y)
{
    QGraphicsLineItem * cross = new QGraphicsLineItem(-3,-3,+3,+3, layer(m_color));
    cross->setZValue(5);
    cross->setPen(m_pens["thick-red"]);
    cross->setPos(x, y);

    QGraphicsLineItem * l = new QGraphicsLineItem(+3,-3,-3,+3,cross);
    l->setPen(m_pens["thick-red"]);

    l = new QGraphicsLineItem(+2,-2,-2,+2,cross);
    l->setPen(m_pens["white"]);
    l = new QGraphicsLineItem(+2,+2,-2,-2,cross);
    l->setPen(m_pens["white"]);

    updateViews();
}

void SnapshotModel::updateViews()
{
    QString tag = m_mode == TRAIN ? m_color : QString();
    foreach(QString layer_name, m_layers.keys()) {
        m_layers[layer_name]->setVisible( (layer_name == tag) );
    }
    /* something wrong with repaintage... */
    foreach(QGraphicsView * view,m_scene->views()) {
        view->viewport()->update();
    }
}

void SnapshotModel::saveData()
{
    QFile file( m_cacheDir.filePath("data.json"));
    file.open(QFile::WriteOnly);
    QTextStream strm(&file);
    QVariantHash conf;
    QVariantHash picks;
    foreach(QString color, m_colorPicks.keys()) {
        QVariantList l;
        foreach(QPoint p, m_colorPicks[color])
            l << p.x() << p.y();
        picks[color] = l;
    }
    conf["picks"] = picks;
    strm << Json::serialize(conf);
}

void SnapshotModel::loadData()
{
    QFile file( m_cacheDir.filePath("data.json"));
    file.open(QFile::ReadOnly);
    QTextStream strm(&file);
    QString json = strm.readAll();
    QVariantMap parsed = Json::parse(json).toMap();

    { // load the picks
        Mode old_mode = m_mode;
        QString old_color = m_color;

        QVariantMap picks = parsed["picks"].toMap();
        foreach(QString color, picks.keys()) {
            setTrainMode(color);
            QVariantList list = picks[color].toList();
            QVector<QVariant> array;
            foreach(QVariant v, list) array << v;

            for(int i=0; i<array.size(); i+=2) {
                int x = array[i].toInt(), y = array[i+1].toInt();
                pick(x,y);
            }
        }

        m_mode = old_mode;
        m_color = old_color;
    }

    updateViews();
}

QGraphicsItem * SnapshotModel::layer(const QString &name)
{
    if (!m_layers.contains(name)) {
        m_layers[name] = new QGraphicsItemGroup(0,m_scene);
    }
    return m_layers[name];
}

void SnapshotModel::clearLayer()
{
    if (m_mode == TRAIN) {
        QList<QGraphicsItem*> children = layer( m_color )->childItems();
        foreach(QGraphicsItem* child, children) {
            delete child;
        }
        m_colorPicks[m_color].clear();
    } else if (m_mode == MASK) {
        qDebug() << "TODO clear mask";
    }

    updateViews();
}

void SnapshotModel::selectByFlood(int x, int y)
{
    // flood fill inside roi
    QImage q_input = image("working");
    cv::Mat input(q_input.height(), q_input.width(), CV_8UC3, (void*)q_input.constBits());

    if (!m_matrices.contains(m_color+"_pickMask")) {
        m_matrices[m_color+"_pickMask"] = cv::Mat( input.rows+2, input.cols+2, CV_8UC1, cv::Scalar(0));
    }
    cv::Mat mask = m_matrices[m_color+"_pickMask"];

    cv::Rect bounds;

    int pf = projectSettings().value("pick_fuzz").toInt();
    int res = cv::floodFill(input, mask,
                            cv::Point(x,y),
                            cv::Scalar(0,0,0),
                            &bounds,
                            // FIXME the range should be preference
                            cv::Scalar( pf,pf,pf ),
                            cv::Scalar( pf,pf,pf ),
                            cv::FLOODFILL_MASK_ONLY | cv::FLOODFILL_FIXED_RANGE );

    if (res < 1) return;

    // if intersected some polygons - remove these polygons and grow ROI with their bounds
    QRect q_bounds = toQt(bounds);
    QGraphicsItem * colorLayer = layer(m_color);
    foreach(QGraphicsItem * item, colorLayer->childItems()) {
        QGraphicsPolygonItem * poly_item = qgraphicsitem_cast<QGraphicsPolygonItem *>(item);
        if (!poly_item) continue;
        QRect bounds = poly_item->polygon().boundingRect().toRect();
        if (q_bounds.intersects( bounds )) {
            q_bounds = q_bounds.united(bounds);
            delete poly_item;
        }
    }
    // intersect with the image
    q_bounds = q_bounds.intersected( image("working").rect() );

    bounds = toCv(q_bounds);

    // cut out from mask
    cv::Mat mask0;
    cv::Mat( mask, cv::Rect( bounds.x+1,bounds.y+1,bounds.width,bounds.height ) ).copyTo(mask0);
    // find contours there and add to the polygons list
    std::vector< std::vector< cv::Point > > contours;
    cv::findContours(mask0,
                     contours,
                     CV_RETR_EXTERNAL,
                     CV_CHAIN_APPROX_TC89_L1,
                     // offset: compensate for ROI and flood fill mask border
                     bounds.tl() );

    // now refresh contour visuals
    foreach( const std::vector< cv::Point >& contour, contours ) {
        std::vector< cv::Point > approx;
        // simplify contours
        cv::approxPolyDP( contour, approx, 3, true);
        QPolygon polygon = toQPolygon(approx);
        QGraphicsPolygonItem * poly_item = new QGraphicsPolygonItem( polygon, colorLayer );
        poly_item->setPen(m_pens["thick-red"]);
        poly_item->setZValue(5);

        poly_item = new QGraphicsPolygonItem( polygon, poly_item );
        poly_item->setPen(m_pens["white"]);
    }

}

void SnapshotModel::unpick(int x, int y)
{
    // 1. find which contour we're in (shouldn't we capture it elsewhere then?)
    QString color;
    QGraphicsPolygonItem * unpicked_poly = 0;

    foreach(QString c, m_layers.keys()) {
        QGraphicsItem * l = layer(c);
        foreach(QGraphicsItem * item, l->childItems()) {
            QGraphicsPolygonItem * poly_item = qgraphicsitem_cast<QGraphicsPolygonItem *>(item);
            if (!poly_item) continue;
            if (poly_item->polygon().containsPoint(QPointF(x,y), Qt::OddEvenFill)) {
                unpicked_poly = poly_item;
                color = c;
                break;
            }
        }
        if (unpicked_poly)
            break;
    }
    if (!unpicked_poly)
        return;

    // 2. find which picks are in this contour and remove them and their crosses
    foreach(const QPoint& pick, m_colorPicks[color]) {
        if (unpicked_poly->polygon().containsPoint(pick, Qt::OddEvenFill))
            m_colorPicks[color].removeOne(pick);
    }
    foreach(QGraphicsItem * item, layer(color)->childItems()) {
        QGraphicsLineItem * cross = qgraphicsitem_cast<QGraphicsLineItem *>(item);
        if (!cross) continue;
        if (unpicked_poly->polygon().containsPoint( cross->pos(), Qt::OddEvenFill )) {
            delete cross;
        }
    }

    // 3. draw this contour onto the mask
    cv::Mat mask = m_matrices[color+"_pickMask"];
    std::vector< cv::Point > contour = toCvInt( unpicked_poly->polygon() );
    cv::Point * pts[] = { contour.data() };
    int npts[] = { contour.size() };
    cv::fillPoly( mask, (const cv::Point**)pts, npts, 1, cv::Scalar(0), 8, 0,
                  // compensate for mask border
                  cv::Point(1,1) );

    // 4. delete the polygon itself
    delete unpicked_poly;

    updateViews();
}

cv::Mat SnapshotModel::matrixFromImage(const QString &tag)
{
    if (m_images.contains(tag)) {
        QImage q_image = image(tag);
        int format = q_image.format()==QImage::Format_Indexed8 ? CV_8UC1 : CV_8UC3;
        return cv::Mat(q_image.height(), q_image.width(), format, (void*)q_image.constBits());
    } else {
        return cv::Mat();
    }
}

void SnapshotModel::trainColors()
{
    if (m_displayers.contains("samples-display")) {
        delete m_displayers["samples-display"];
    }
    QGraphicsItemGroup * displayer = new QGraphicsItemGroup(0,m_scene);
    m_displayers["samples-display"] = displayer;

    QVector<cv::Mat> centers_list;
    int centers_count = 0;
    cv::Mat input = matrixFromImage("working");

    int color_index = 0;

    m_palette.clear();

    foreach(QString color, m_layers.keys()) {
        if (!m_matrices.contains(color+"_pickMask")) continue;

        QVector<unsigned char> sample_pixels;
        cv::Mat mask = m_matrices[color+"_pickMask"];

        for(int i = 0; i<input.rows; ++i)
            for(int j = 0; j<input.cols; ++j)
                if (mask.at<unsigned char>(i+1,j+1))
                    // copy this pixel
                    sample_pixels
                            << input.ptr<unsigned char>(i)[j*3]
                            << input.ptr<unsigned char>(i)[j*3+1]
                            << input.ptr<unsigned char>(i)[j*3+2];

        if (sample_pixels.size()==0) continue;

        qDebug() << "collected" << sample_pixels.size() / 3 << qPrintable(color) << "pixels";

        cv::Mat sample(sample_pixels.size()/3, 3, CV_8UC1, sample_pixels.data());
        cv::Mat centers(COLOR_QUANTA_COUNT, 3, CV_32F);
        cvflann::KMeansIndexParams params(
                    COLOR_QUANTA_COUNT, // branching
                    10, // max iterations
                    cvflann::FLANN_CENTERS_KMEANSPP,
                    0);
        int n_clusters = cv::flann::hierarchicalClustering< Distance_U8 >( sample, centers, params );
        for(int i=0; i<n_clusters; i++) {
            QGraphicsRectItem * patch = new QGraphicsRectItem(displayer);
            QColor patch_color( centers.at<float>(i,0),
                                centers.at<float>(i,1),
                                centers.at<float>(i,2) );
            m_palette.append( patch_color.rgb() );
            patch->setBrush( patch_color );
            patch->setPen(m_pens["white"]);
            patch->setRect(0,0,20,20);
            patch->setPos( 10 + (color_index) * 25, 10 + i * 25 );
        }
        centers_list << centers;
        centers_count += n_clusters;
        color_index++;
    }

    // have to convert features to uint8
    cv::Mat features( centers_count, 3, CV_8UC1 );
    for(int i=0; i<centers_list.size(); ++i)
        centers_list[i].convertTo( features.rowRange( i*COLOR_QUANTA_COUNT,(i+1)*COLOR_QUANTA_COUNT ), CV_8UC1, 255.0 );

    ///cvflann::AutotunedIndexParams params(1, 1, -1, 1);
    //cvflann::KMeansIndexParams params( color_index, 11, cvflann::FLANN_CENTERS_KMEANSPP, 0 );
    cvflann::LinearIndexParams params;
    if (m_flann) delete m_flann;
    m_flann = new cv::flann::GenericIndex< Distance_U8 > (features, params);
    qDebug() << "built FLANN classifier";

    updateViews();
}

void SnapshotModel::countCards()
{
    if (!m_flann) {
        qDebug() << "Teach me the colors first";
        return;
    }
    cv::Mat input = matrixFromImage("working");
    int n_pixels = input.rows * input.cols;
    cv::Mat indices( input.rows, input.cols, CV_32SC1 );
    cv::Mat dists( input.rows, input.cols, CV_32FC1 );

    cv::Mat input_1 = input.reshape( 1, n_pixels ),
            indices_1 = indices.reshape( 1, n_pixels ),
            dists_1 = dists.reshape( 1, n_pixels );

    qDebug() << "K-Nearest Neighbout Search";
    cvflann::SearchParams params(cvflann::FLANN_CHECKS_UNLIMITED, 0);
    m_flann->knnSearch( input_1, indices_1, dists_1, 1, params);
    qDebug() << "K-Nearest Neighbout Search done";

    m_matrices["indices"] = indices / COLOR_QUANTA_COUNT;
    m_matrices["dists"] = dists;

    cv::Mat cards_mask;
    float thresh = 2700.0;
    cv::threshold(dists, cards_mask, thresh, 0, cv::THRESH_TRUNC);
    cards_mask.convertTo( cards_mask, CV_8UC1, - 255.0 / thresh, 255.0 );

    setImage("cards-mask", wrapImage(cards_mask));

    QGraphicsPixmapItem * gpi = new QGraphicsPixmapItem( QPixmap::fromImage(image("cards-mask",true)), 0, m_scene );

    /*
    indices.convertTo(indices,CV_8UC1);
    QImage indexed( (unsigned char *)indices.data, indices.cols, indices.rows, QImage::Format_Indexed8 );
    indexed.setColorTable(m_palette);
    QGraphicsPixmapItem * gpi = new QGraphicsPixmapItem( QPixmap::fromImage(indexed), 0, m_scene );
    */

    updateViews();
}
