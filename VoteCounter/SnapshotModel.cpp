#include "SnapshotModel.hpp"
#include "QMetaUtilities.hpp"
#include "MouseLogic.hpp"

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
QStringList SnapshotModel::s_persistentMasks = QStringList()
<< "train.contours.green" << "train.contours.pink" << "train.contours.yellow";

SnapshotModel::SnapshotModel(const QString& path, QObject *parent) :
    QObject(parent),
    m_originalPath(path),
    m_scene(new QGraphicsScene(this)),
    m_mouseLogic( new MouseLogic(m_scene) ),
    m_mode(INERT),
    m_color("green"),
    m_flann(0),
    m_showColorDiff(false)
{

    m_pens["white"] = QPen(Qt::white);
    m_pens["+selection"] = QPen(QColor(128,255,128,128), 0);
    m_pens["-selection"] = QPen(QColor(255,128,128,128), 0);
    m_rectSelection = new QGraphicsRectItem(0,m_scene);
    m_rectSelection->setVisible(false);
    m_rectSelection->setZValue(100.0);

    m_mouseLogic->setObjectName("mouseLogic");
    QMetaUtilities::connectSlotsByName( parent, this );

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

    loadData();

    // try to load flann
    QString palette_file = m_parentDir.filePath("palette.png");
    QString flann_file = m_parentDir.filePath("flann.dat");
    if ( QFileInfo(palette_file).exists() && QFileInfo(flann_file).exists()) {
        cv::Mat paletteRGB = cv::imread( palette_file.toStdString(), -1 );
        cv::Mat paletteLab;
        paletteRGB.convertTo(paletteLab, CV_32FC3, 1.0/255.0);
        cv::cvtColor( paletteLab, paletteLab, CV_RGB2Lab );

        setMatrix("paletteRGB", cv::Mat(paletteRGB.rows, 3, CV_8UC1, paletteRGB.data).clone());
        setMatrix("paletteLab", cv::Mat(paletteLab.rows, 3, CV_32FC1, paletteLab.data).clone());

        cvflann::SavedIndexParams params(flann_file.toStdString());
        m_flann = new cv::flann::GenericIndex< ColorDistance >(getMatrix("paletteLab"), params);

        showPalette();
    }

    updateViews();

    qDebug() << qPrintable(path) << "loaded";
}

SnapshotModel::~SnapshotModel()
{
    qDebug() << "closing snapshot...";
    saveData();
}

QVariant SnapshotModel::uiValue(const QString &name)
{
    return parent()->findChild<QObject*>(name)->property("value");
}

void SnapshotModel::pick(int x, int y)
{
    int fuzz = uiValue("pickFuzz").toInt();
    QImage input = getImage("input");
    QString layerName;
    if (input.rect().contains(x,y)) {
        switch(m_mode) {
        case COUNT:
            if (!m_matrices.contains("indices")) {
                qWarning() << "Count cards first!";
                return;
            } else
                // use the result of previous pixel classification
                layerName = "count.contours." + s_colorNames[ getMatrix("indices").at<int>(y,x) / COLOR_GRADATIONS ];
            break;
        case TRAIN:
            layerName = "train.contours." + m_color;
            break;
        }
        floodPickContour(x, y, fuzz, layerName);
        updateViews();
    }
}

void SnapshotModel::setTrainMode(const QString &tag)
{
    setMode(TRAIN);
    m_color = tag;

    updateViews();
}

void SnapshotModel::updateViews()
{
    layer("train")->setVisible(false);
    layer("count")->setVisible(false);

    switch (m_mode) {
    case TRAIN:
        layer("train")->setVisible(true);
        foreach(QString color, s_colorNames) {
            QGraphicsItem * l = layer( "train.contours." + color);
            int count = l->childItems().count();
            parent()->findChild<QLabel*>( color + "TrainCount" )->setText( QString("%1").arg( count ) );
            l->setVisible( color == m_color );
        }
        break;
    case COUNT:
        layer("count")->setVisible(true);
        layer("count.colorDiff")->setVisible( m_showColorDiff );
        layer("count.contours")->setVisible( !m_showColorDiff );

        foreach(QString color, s_colorNames) {
            int count = layer("count.contours." + color)->childItems().count();
            parent()->findChild<QLabel*>( color + "Count" )->setText( QString("%1").arg( count ) );
        }

        break;
    }

    // force repaint
    foreach(QGraphicsView * view,m_scene->views()) {
        view->viewport()->update();
    }
}

void SnapshotModel::saveData()
{
    foreach(QString name, s_persistentMasks) {
        QString fname = m_cacheDir.filePath(name + ".png");
        if (m_matrices.contains(name)) {
            cv::imwrite( fname.toStdString(), getMatrix(name) );
        } else if (QFile( fname ).exists()) {
            QFile( fname ).remove();
        }
    }
}

void SnapshotModel::loadData()
{
    cv::Mat input = getMatrix("input");
    int mrows = input.rows, mcols = input.cols;

    foreach(QString name, s_persistentMasks) {
        QString fname = m_cacheDir.filePath(name + ".png");
        if ( QFile(fname).exists() ) {
            cv::Mat mask = cv::imread( fname.toStdString(), 0);
            if (mask.rows == mrows && mask.cols == mcols) {
                setMatrix(name, mask);
                detectContours(name);
            } else {
                qDebug() << "Incompatible mask" << fname << ", removing";
                QFile(fname).remove();
            }
        }
    }
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

void SnapshotModel::floodPickContour(int x, int y, int fuzz, const QString& layerName)
{
    // flood fill inside roi
    cv::Mat input = getMatrix("lab");
    cv::Mat mask = getMatrix( layerName );

    cv::Rect bounds;
    cv::Mat pickMask( input.rows+2, input.cols+2, CV_8UC1, cv::Scalar(0) );
    int res = cv::floodFill(input, pickMask,
                            cv::Point(x,y),
                            0, // unused
                            &bounds,
                            cv::Scalar( fuzz,fuzz,fuzz ),
                            cv::Scalar( fuzz,fuzz,fuzz ),
                            4 | cv::FLOODFILL_MASK_ONLY | cv::FLOODFILL_FIXED_RANGE );

    if (res < 1) return;

    // merge masks
    cv::Rect img_bounds = bounds - cv::Point(1,1);
    qDebug() << "flood fill bounds:" << bounds.x << bounds.y << bounds.width << bounds.height;
    qDebug() << "  in image coords:" << img_bounds.x << img_bounds.y << img_bounds.width << img_bounds.height;
    cv::Mat(mask, img_bounds) |= cv::Mat(pickMask, bounds) * 255;

    // if intersected some polygons - remove these polygons and grow ROI with their bounds
    QRect q_bounds = toQt(img_bounds);
    q_bounds.adjust(-1,-1,1,1);
    QGraphicsItem * contourGroup = layer( layerName );
    foreach(QGraphicsItem * item, m_scene->items( q_bounds, Qt::IntersectsItemShape )) {
        QGraphicsPolygonItem * poly_item = qgraphicsitem_cast<QGraphicsPolygonItem *>(item);
        if (!poly_item || !contourGroup->isAncestorOf(poly_item)) continue;
        QRect poly_bounds = poly_item->polygon().boundingRect().toRect();
        q_bounds = q_bounds.united(poly_bounds);
        delete poly_item;
    }
    q_bounds.adjust(-1,-1,1,1);
    q_bounds = q_bounds.intersected( getImage("input").rect() );
    img_bounds = toCv(q_bounds);

    detectContours( layerName, true, img_bounds );
}

void SnapshotModel::unpick(int x, int y)
{
    // find which contour we're in (shouldn't we capture it elsewhere then?)
    QGraphicsPolygonItem * unpicked_poly = 0;
    foreach(QGraphicsItem * i, m_scene->items(QPointF(x,y))) {
        unpicked_poly = qgraphicsitem_cast<QGraphicsPolygonItem *>(i);
        if (unpicked_poly) break;
    }

    if (!unpicked_poly)
        return;

    QString layerName = unpicked_poly->parentItem()->data(ITEM_FULLNAME).toString();

    // (un)draw this contour onto the mask
    cv::Mat mask = getMatrix(layerName);
    cv::floodFill( mask, cv::Point(x,y), cv::Scalar(0), 0, cv::Scalar(), cv::Scalar(), 4 | cv::FLOODFILL_FIXED_RANGE);

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
        if (!matrixTag.startsWith("train.contours.")) continue;

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
        // cv::flann::hierarchicalClustering returns float centers even for integer palette
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

    cv::Mat paletteLab = cv::Mat( centers_count, 3, CV_32FC1 );
    for(int i=0; i<centers_list.size(); ++i)
        centers_list[i].copyTo( paletteLab.rowRange( i*COLOR_GRADATIONS,(i+1)*COLOR_GRADATIONS ) );
    m_matrices.remove("paletteRGB");
    setMatrix("paletteLab", paletteLab);

    showPalette();

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
    cv::Mat lut = getMatrix("paletteRGB");
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
        setMatrix( "count.contours." + s_colorNames[i], cardMasks[i] );
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
    int minSize = uiValue("sizeFilter").toInt();
    minSize *= minSize;

    for(int i = 0; i<3; i++) {
        QString layerName =  "count.contours." + s_colorNames[i];

        // clone mask because find contours corrupts
        cv::Mat mask = getMatrix(layerName).clone();
        std::vector< std::vector< cv::Point > > contours;
        cv::findContours(mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);
        // now refresh contour visuals
        clearLayer(layerName);
        int count = 0;
        foreach( const std::vector< cv::Point >& contour, contours ) {

            if (minSize > cv::contourArea(contour) )
                continue;

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
            count ++;
        }
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
        } else if (tag.contains(".contours.")) {
            matrix = cv::Mat(inputSize.height, inputSize.width, CV_8UC1, cv::Scalar(0));
        } else if (tag == "paletteRGB") {
            cv::Mat paletteLab = getMatrix("paletteLab");
            matrix = cv::Mat( paletteLab.rows, 3, CV_32FC1 );
            cv::cvtColor( cv::Mat(paletteLab.rows, 1, CV_32FC3, paletteLab.data),
                          cv::Mat(paletteLab.rows, 1, CV_32FC3, matrix.data),
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

void SnapshotModel::on_resetLayer_clicked()
{
    if (m_mode == TRAIN) {
        QString name = "train.contours." + m_color;
        clearLayer( name );
        m_matrices.remove( name );
    }
    updateViews();
}

void SnapshotModel::setMode(SnapshotModel::Mode m)
{
    m_mode = m;
    updateViews();
}

void SnapshotModel::showPalette()
{
    cv::Mat paletteRGB = getMatrix("paletteRGB");
    QImage palette( paletteRGB.data, paletteRGB.rows, 1, QImage::Format_RGB888 );
    clearLayer("train.palette");
    QGraphicsPixmapItem * gpi = new QGraphicsPixmapItem( QPixmap::fromImage(palette), layer("train.palette") );
    gpi->scale(15,15);
}

void SnapshotModel::buildFlannRecognizer()
{
    cvflann::AutotunedIndexParams params( 0.8, 1, 0, 1.0 );
    //cvflann::LinearIndexParams params;
    if (m_flann) delete m_flann;
    m_flann = new cv::flann::GenericIndex< ColorDistance > (getMatrix("paletteLab"), params);

    QString palette_file = m_parentDir.filePath("palette.png");
    cv::imwrite( palette_file.toStdString(), cv::Mat(getMatrix("paletteRGB").rows, 1, CV_8UC3, getMatrix("paletteRGB").data) );

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
    clearLayer("count.contours");
    m_showColorDiff = true;
    updateViews();
}

void SnapshotModel::on_colorDiffThreshold_sliderReleased()
{
    m_showColorDiff = false;
    countCards();
    updateViews();
}

void SnapshotModel::on_sizeFilter_valueChanged()
{
    countCards();
    updateViews();
}

void SnapshotModel::on_mouseLogic_pointClicked(QPointF point, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    if (button == Qt::LeftButton) {
        pick( point.x(), point.y() );
    } else {
        unpick(point.x(), point.y());
    }
}

void SnapshotModel::on_mouseLogic_rectUpdated(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    m_rectSelection->setVisible(true);
    m_rectSelection->setRect(rect);
    m_rectSelection->setPen(m_pens[(button == Qt::LeftButton) ? "+selection" : "-selection"]);
}

void SnapshotModel::on_mouseLogic_rectSelected(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    m_rectSelection->setVisible(false);
    if (button == Qt::LeftButton)
        mergeContours(rect);
    else
        clearContours(rect);
}

void SnapshotModel::mergeContours(QRectF rect)
{
    qDebug() << "TODO merge contours in" << rect;
    switch(m_mode) {
    case TRAIN:
        break;
    case COUNT:
        break;
    }
    updateViews();
}

void SnapshotModel::clearContours(QRectF rect)
{
    if (m_mode != TRAIN && m_mode != COUNT)
        return;

    // collect selected contours
    QList<QGraphicsItem *>items = m_scene->items(rect,Qt::ContainsItemShape);

    foreach(QGraphicsItem *i, items) {
        QGraphicsPolygonItem * pi = qgraphicsitem_cast<QGraphicsPolygonItem *>(i);
        if (pi) {

            QString layerName = pi->parentItem()->data(ITEM_FULLNAME).toString();
            cv::Mat mask = getMatrix(layerName);
            // erase the polygon from the mask: it's more reliable to flood fill than draw a contour, so
            cv::Point seed = toCv( pi->polygon()[0] );
            cv::floodFill( mask, seed, cv::Scalar(0), 0, cv::Scalar(), cv::Scalar(), 4 | cv::FLOODFILL_FIXED_RANGE);
            delete pi;
        }
    }

    updateViews();
}


QList< QPolygon >  SnapshotModel::detectContours(const QString &maskAndLayerName,
                                                 bool addToScene,
                                                 cv::Rect maskROI,
                                                 int simple)
{
    if (!m_matrices.contains(maskAndLayerName))
        return QList< QPolygon >();

    cv::Mat mask = getMatrix(maskAndLayerName);
    if (maskROI.width)
        mask = cv::Mat(mask, maskROI);
    std::vector< std::vector< cv::Point > > contours;
    cv::findContours(mask.clone(), // clone because findContours corrupts the mask
                     contours,
                     CV_RETR_EXTERNAL,
                     CV_CHAIN_APPROX_TC89_L1,
                     maskROI.tl()); // compensate for ROI

    QList< QPolygon > polygons;

    // simplify and convert contours to Qt polygons
    foreach(std::vector< cv::Point > contour, contours ) {
        QPolygon polygon;
        if (simple > 0) {
            std::vector< cv::Point > approx;
            // simplify contours
            cv::approxPolyDP( contour, approx, simple, true);
            polygon = toQPolygon(approx);
        } else
            polygon = toQPolygon(contour);
        polygons << polygon;

        if (addToScene)
            addContour(polygon, maskAndLayerName);
    }

    if (addToScene)
        updateViews();

    return polygons;
}

void SnapshotModel::addContour(const QPolygon &contour, const QString &name)
{
    QGraphicsPolygonItem * poly_item = new QGraphicsPolygonItem( contour, layer(name) );
    poly_item->setPen(m_pens["white"]);
}
