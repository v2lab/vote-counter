#include "SnapshotModel.hpp"
#include "QMetaUtilities.hpp"

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

QStringSet SnapshotModel::s_cacheableImages = QStringSet() << "input";
QStringSet SnapshotModel::s_resizedImages = QStringSet() << "input";
QStringList SnapshotModel::s_colorNames = QStringList() << "green" << "pink" << "yellow";

SnapshotModel::SnapshotModel(const QString& path, QObject *parent) :
    QObject(parent),
    m_originalPath(path),
    m_scene(new QGraphicsScene(this)),
    m_mode(INERT),
    m_color("green"),
    m_flann(0),
    m_showColorDiff(false)
{
    QMetaUtilities::connectSlotsByName( parent, this );

    m_pens["white"] = QPen(Qt::white);
    m_pens["thick-red"] = QPen(Qt::red, 2);
    m_scene->setObjectName("scene"); // so we can autoconnect signals

    qDebug() << "Loading" << qPrintable(path);

    // check if cache is present, create otherwise
    QFileInfo fi(path);

    m_parentDir = fi.absoluteDir();
    m_cacheDir = m_parentDir.filePath( fi.baseName() + ".cache" );
    if (!m_cacheDir.exists()) {
        qDebug() << "creating cache directory" << qPrintable(m_cacheDir.path());
        m_parentDir.mkdir( m_cacheDir.dirName() );
    }

    // add the image to the scene
    m_scene->addPixmap( QPixmap::fromImage( getImage("input") ) );
    m_scene->installEventFilter(this);

    loadData();

    // try to load flann
    QString features_file = m_parentDir.filePath("features.png");
    QString flann_file = m_parentDir.filePath("flann.dat");
    if ( QFileInfo(features_file).exists() && QFileInfo(flann_file).exists()) {
        cv::Mat featuresRGB = cv::imread( features_file.toStdString(), -1 );
        cv::Mat featuresLab;
        featuresRGB.convertTo(featuresLab, CV_32FC3, 1.0/255.0);
        cv::cvtColor( featuresLab, featuresLab, CV_RGB2Lab );

        setMatrix("featuresRGB", cv::Mat(featuresRGB.rows, 3, CV_8UC1, featuresRGB.data).clone());
        setMatrix("featuresLab", cv::Mat(featuresLab.rows, 3, CV_32FC1, featuresLab.data).clone());

        cvflann::SavedIndexParams params(flann_file.toStdString());
        m_flann = new cv::flann::GenericIndex< ColorDistance >(getMatrix("featuresLab"), params);

        showFeatures();
    }

    updateViews();

    qDebug() << qPrintable(path) << "loaded";
}

SnapshotModel::~SnapshotModel()
{
    qDebug() << "closing snapshot...";
    saveData();
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
    QImage input = getImage("input");

    if (m_mode == TRAIN) {
        if (input.rect().contains(x,y)) {
            m_colorPicks[m_color].append( QPoint(x,y) ); // this is for saving / restoring
            addCross(x,y);
            selectByFlood(x,y);
            updateViews();
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
    QGraphicsLineItem * cross = new QGraphicsLineItem(-3,-3,+3,+3, layer( QString("train.%1.crosses").arg(m_color)));
    cross->setPen(m_pens["thick-red"]);
    cross->setPos(x, y);

    QGraphicsLineItem * l = new QGraphicsLineItem(+3,-3,-3,+3,cross);
    l->setPen(m_pens["thick-red"]);

    l = new QGraphicsLineItem(+2,-2,-2,+2,cross);
    l->setPen(m_pens["white"]);
    l = new QGraphicsLineItem(+2,+2,-2,-2,cross);
    l->setPen(m_pens["white"]);
}

void SnapshotModel::updateViews()
{
    layer("train")->setVisible(false);
    layer("count")->setVisible(false);

    switch (m_mode) {
    case TRAIN:
        layer("train")->setVisible(true);
        foreach(QString color, s_colorNames) {
            layer( QString("train.%1").arg(color))->setVisible( color == m_color );
        }
        break;
    case COUNT:
        layer("count")->setVisible(true);
        layer("count.colorDiff")->setVisible( m_showColorDiff );
        layer("count.cards")->setVisible( !m_showColorDiff );
        break;
    }


    // force repaint
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
        int dotIdx = name.lastIndexOf(".");
        QGraphicsItem * parent = 0;
        if (dotIdx != -1) {
            parent = layer( name.left(dotIdx) );
        }
        m_layers[name] = new QGraphicsItemGroup(parent, m_scene);
        m_layers[name]->setData(ITEM_NAME, name.right(name.size() - dotIdx - 1) ); // this even works for no dot!
        m_layers[name]->setData(ITEM_FULLNAME, name);
        m_layers[name]->setZValue( name.count('.') + 1 );
    }
    return m_layers[name];
}

void SnapshotModel::clearLayer(const QString& name)
{
    if (m_layers.contains(name))
        foreach(QGraphicsItem* child, layer( name )->childItems()) {
            m_layers.remove( child->data(ITEM_FULLNAME).toString() );
            delete child;
        }

    updateViews();
}

void SnapshotModel::selectByFlood(int x, int y)
{
    // flood fill inside roi
    cv::Mat input = getMatrix("lab");
    cv::Mat mask = getMatrix(m_color+"_pickMask");
    cv::Rect bounds;

    int pf = uiValue("pickFuzz").toInt();
    int res = cv::floodFill(input, mask,
                            cv::Point(x,y),
                            0, // unused
                            &bounds,
                            // FIXME the range should be preference
                            cv::Scalar( pf,pf,pf ),
                            cv::Scalar( pf,pf,pf ),
                            cv::FLOODFILL_MASK_ONLY | cv::FLOODFILL_FIXED_RANGE );

    if (res < 1) return;

    // if intersected some polygons - remove these polygons and grow ROI with their bounds
    QRect q_bounds = toQt(bounds);
    QGraphicsItem * colorLayer = layer( QString("train.%1.contours").arg(m_color) );
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
    q_bounds = q_bounds.intersected( getImage("input").rect() );

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

        poly_item = new QGraphicsPolygonItem( polygon, poly_item );
        poly_item->setPen(m_pens["white"]);
    }

}

void SnapshotModel::unpick(int x, int y)
{
    // 1. find which contour we're in (shouldn't we capture it elsewhere then?)
    QString color;
    QGraphicsPolygonItem * unpicked_poly = 0;

    foreach(QString c, s_colorNames) {
        QString contoursLayerPath = QString("train.%1.contours").arg(c);
        if (! m_layers.contains(contoursLayerPath)) continue;
        QGraphicsItem * contoursLayer = layer(contoursLayerPath);
        foreach(QGraphicsItem * item, contoursLayer->childItems()) {
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
    QString crossesLayerPath = QString("train.%1.crosses").arg(color);
    foreach(QGraphicsItem * item, layer(crossesLayerPath)->childItems()) {
        QGraphicsLineItem * cross = qgraphicsitem_cast<QGraphicsLineItem *>(item);
        if (!cross) continue;
        if (unpicked_poly->polygon().containsPoint( cross->pos(), Qt::OddEvenFill )) {
            delete cross;
        }
    }

    // 3. (un)draw this contour onto the mask
    cv::Mat mask = getMatrix(color+"_pickMask");
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

void SnapshotModel::on_learn_clicked()
{
    QVector<cv::Mat> centers_list;
    int centers_count = 0;
    cv::Mat input = getMatrix("lab");

    int color_index = 0;

    foreach(QString matrixTag, m_matrices.keys()) {
        if (!matrixTag.endsWith("_pickMask")) continue;

        QVector<ColorType> sample_pixels;
        cv::Mat mask = getMatrix(matrixTag);

        for(int i = 0; i<input.rows; ++i)
            for(int j = 0; j<input.cols; ++j)
                if (mask.at<unsigned char>(i+1,j+1))
                    // copy this pixel
                    sample_pixels
                            << input.ptr<ColorType>(i)[j*3]
                            << input.ptr<ColorType>(i)[j*3+1]
                            << input.ptr<ColorType>(i)[j*3+2];

        if (sample_pixels.size()==0) continue;

        cv::Mat sample(sample_pixels.size()/3, 3, CV_32FC1, sample_pixels.data());
        // cv::flann::hierarchicalClustering returns float centers even for integer features
        cv::Mat centers(COLOR_GRADATIONS, 3, CV_32FC1);
        cvflann::KMeansIndexParams params(
                    COLOR_GRADATIONS, // branching
                    10, // max iterations
                    cvflann::FLANN_CENTERS_KMEANSPP,
                    0);
        int n_clusters = cv::flann::hierarchicalClustering< ColorDistance >( sample, centers, params );
        centers_list << centers;
        centers_count += n_clusters;
        color_index++;
    }

    cv::Mat featuresLab = cv::Mat( centers_count, 3, CV_32FC1 );
    for(int i=0; i<centers_list.size(); ++i)
        centers_list[i].copyTo( featuresLab.rowRange( i*COLOR_GRADATIONS,(i+1)*COLOR_GRADATIONS ) );
    m_matrices.remove("featuresRGB");
    setMatrix("featuresLab", featuresLab);

    showFeatures();

    buildFlannRecognizer();

    qDebug() << "built FLANN classifier";

    updateViews();

}

void SnapshotModel::on_count_clicked()
{
    if (!m_flann) {
        qDebug() << "Teach me the colors first";
        return;
    }

    classifyPixels();
    computeColorDiff();
    countCards();

    updateViews();
}

void SnapshotModel::classifyPixels()
{
    cv::Mat input = getMatrix("lab");

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

    setMatrix("indices", indices);
    setMatrix("dists", dists);
}

void SnapshotModel::computeColorDiff()
{
    float thresh = parent()->findChild<QAbstractSlider*>("colorDiffThreshold")->value();
    thresh = 3.0 * thresh * thresh;

    cv::Mat thresholdedDiff;
    cv::threshold(getMatrix("dists"), thresholdedDiff, thresh, 0, cv::THRESH_TRUNC);
    thresholdedDiff.convertTo( thresholdedDiff, CV_8UC1, - 255.0 / thresh, 255.0 );

    // poor man's LookUpTable
    cv::Mat indices = getMatrix("indices");
    int n_pixels = indices.rows * indices.cols;
    cv::Mat lut = getMatrix("featuresRGB");
    // actual per-card-color masks
    QVector<cv::Mat> cardMasks;
    for(int i=0; i<3; i++)
        cardMasks << cv::Mat(  indices.rows, indices.cols, CV_8UC1, cv::Scalar(0) );

    for(int i=0; i<n_pixels; i++) {
        if (thresholdedDiff.data[i]) {
            int index = indices.ptr<int>(0)[i];
            int color = index / COLOR_GRADATIONS;
            cardMasks[color].data[i] = 1;
        }
    }

    for(int i=0; i<3; i++) {
        cv::morphologyEx( cardMasks[i], cardMasks[i], cv::MORPH_OPEN, cv::Mat() );
        setMatrix( "count.cards." + s_colorNames[i], cardMasks[i] );
    }

    // the display
    cv::Mat colorDiff = cv::Mat( indices.rows, indices.cols, CV_8UC3, cv::Scalar(0,0,0,0) );
    for(int i=0; i<n_pixels; i++) {
        int index = indices.ptr<int>(0)[i];
        int color = index / COLOR_GRADATIONS;
        if (cardMasks[color].data[i]) {
            colorDiff.data[i*3] = lut.data[ index*3 ];
            colorDiff.data[i*3+1] = lut.data[ index*3 + 1 ];
            colorDiff.data[i*3+2] = lut.data[ index*3 + 2 ];
        }
    }

    setMatrix("colorDiff", colorDiff);

    // display results
    clearLayer("count.colorDiff");
    QImage vision_image( (unsigned char *)colorDiff.data, colorDiff.cols, colorDiff.rows, QImage::Format_RGB888 );
    QGraphicsPixmapItem * gpi = new QGraphicsPixmapItem( QPixmap::fromImage(vision_image), layer("count.colorDiff") );
}

void SnapshotModel::countCards()
{
    for(int i = 0; i<3; i++) {
        QString layerName =  "count.cards." + s_colorNames[i];

        // clone mask because find contours corrupts
        cv::Mat mask = getMatrix(layerName).clone();
        std::vector< std::vector< cv::Point > > contours;
        cv::findContours(mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);
        // now refresh contour visuals
        clearLayer(layerName);
        foreach( const std::vector< cv::Point >& contour, contours ) {
            int simple = 1;
            QPolygon polygon;
            if (simple > 0) {
                std::vector< cv::Point > approx;
                // simplify contours
                cv::approxPolyDP( contour, approx, simple, true);
                polygon = toQPolygon(approx);
            } else
                polygon = toQPolygon(contour);
            QGraphicsPolygonItem * poly_item = new QGraphicsPolygonItem( polygon, layer(layerName) );
            poly_item->setPen(m_pens["white"]);
        }

        parent()->findChild<QLabel*>( s_colorNames[i] + "Count" )->setText( QString("%1").arg( contours.size() ) );
    }
}

QImage SnapshotModel::getImage(const QString &tag)
{
    if (!m_images.contains(tag)) {
        QImage img;
        int size_limit = uiValue("sizeLimit").toInt();

        if (s_cacheableImages.contains(tag)) {
            // if cacheable - see if we can load it
            QString path = m_cacheDir.filePath(tag + ".png");
            if (img.load(path)) {
                if (tag.endsWith("mask",Qt::CaseInsensitive))
                    img = img.convertToFormat(QImage::Format_Indexed8, greyTable());
                else
                    img = img.convertToFormat(QImage::Format_RGB888);
            }

            // see if size is compatible
            if (s_resizedImages.contains(tag) && !img.isNull() && std::max(img.width(), img.height()) != size_limit)
                img = QImage();
        }
        if (img.isNull()) {
            if (tag == "input") {
                img = QImage( m_originalPath )
                        .scaled( size_limit, size_limit, Qt::KeepAspectRatio, Qt::SmoothTransformation )
                        .convertToFormat(QImage::Format_RGB888);
            }
        }

        setImage(tag,img);
    }

    return m_images[tag];
}

cv::Mat SnapshotModel::getMatrix(const QString &tag)
{
    if (!m_matrices.contains(tag)) {
        cv::Mat matrix;
        // create some well known matrices
        QSize qsz = getImage("input").size();
        cv::Size inputSize = cv::Size( qsz.width(), qsz.height() );

        if (tag == "lab") {
            cv::Mat input = getMatrix("input");
            input.convertTo(matrix, CV_32FC3, 1.0/255.0);
            cv::cvtColor( matrix, matrix, CV_RGB2Lab );
        } else if (tag.endsWith("_pickMask")) {
            matrix = cv::Mat(inputSize.height+2, inputSize.width+2, CV_8UC1, cv::Scalar(0));
        } else if (tag == "featuresRGB") {
            cv::Mat featuresLab = getMatrix("featuresLab");
            matrix = cv::Mat( featuresLab.rows, 3, CV_32FC1 );
            cv::cvtColor( cv::Mat(featuresLab.rows, 1, CV_32FC3, featuresLab.data),
                          cv::Mat(featuresLab.rows, 1, CV_32FC3, matrix.data),
                          CV_Lab2RGB );
            matrix.convertTo( matrix, CV_8UC1, 255.0 );
        } else if (tag == "input") {
            QImage img = getImage(tag);
            matrix = cv::Mat( img.height(), img.width(), CV_8UC3, (void*)img.constBits() );
        } else if (tag == "cacheable_mask") { // <-- FIXME just an example
            QImage img = getImage(tag);
            matrix = cv::Mat( img.height(), img.width(), CV_8UC1, (void*)img.constBits() );
        }

        setMatrix(tag, matrix);
    }
    return m_matrices[tag];
}

void SnapshotModel::setMatrix(const QString &tag, const cv::Mat &matrix)
{
    m_matrices[tag] = matrix;
}

void SnapshotModel::setImage(const QString &tag, const QImage &img)
{
    m_images[tag] = img;
}

void SnapshotModel::on_clearTrainLayer_clicked()
{
    if (m_mode == TRAIN) {
        clearLayer( QString("train.%1.crosses").arg(m_color) );
        clearLayer( QString("train.%1.contours").arg(m_color) );
        m_matrices.remove( m_color + "_pickMask" );
    }
    m_colorPicks[m_color].clear();
    updateViews();
}

void SnapshotModel::setMode(SnapshotModel::Mode m)
{
    m_mode = m;
    updateViews();
}

void SnapshotModel::showFeatures()
{
    cv::Mat featuresRGB = getMatrix("featuresRGB");
    QImage palette( featuresRGB.data, featuresRGB.rows, 1, QImage::Format_RGB888 );
    clearLayer("train.features");
    QGraphicsPixmapItem * gpi = new QGraphicsPixmapItem( QPixmap::fromImage(palette), layer("train.features") );
    gpi->scale(15,15);
}

void SnapshotModel::buildFlannRecognizer()
{
    cvflann::AutotunedIndexParams params( 0.8, 1, 0, 1.0 );
    //cvflann::LinearIndexParams params;
    if (m_flann) delete m_flann;
    m_flann = new cv::flann::GenericIndex< ColorDistance > (getMatrix("featuresLab"), params);

    QString features_file = m_parentDir.filePath("features.png");
    cv::imwrite( features_file.toStdString(), cv::Mat(getMatrix("featuresRGB").rows, 1, CV_8UC3, getMatrix("featuresRGB").data) );

    QString flann_file = m_parentDir.filePath("flann.dat");
    m_flann->save( flann_file.toStdString() );

}

void SnapshotModel::on_trainModeGroup_buttonClicked( QAbstractButton * button )
{
    setTrainMode( button->text().toLower() );
}

void SnapshotModel::on_colorDiffOn_pressed()
{
    m_showColorDiff = true;
    updateViews();
}

void SnapshotModel::on_colorDiffOn_released()
{
    m_showColorDiff = false;
    updateViews();
}

void SnapshotModel::on_colorDiffThreshold_valueChanged()
{
    computeColorDiff();
}

void SnapshotModel::on_colorDiffThreshold_sliderPressed()
{
    clearLayer("count.cards");
    m_showColorDiff = true;
    updateViews();
}

void SnapshotModel::on_colorDiffThreshold_sliderReleased()
{
    m_showColorDiff = false;
    updateViews();
    countCards();
}

QVariant SnapshotModel::uiValue(const QString &name)
{
    return parent()->findChild<QObject*>(name)->property("value");
}
