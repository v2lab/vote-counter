#include "SnapshotModel.hpp"
#include "ProjectSettings.hpp"

#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QImage>

SnapshotModel::SnapshotModel(const QString& path, QObject *parent) :
    QObject(parent)
{
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
    m_scene.addPixmap( QPixmap::fromImage( working ) );

    qDebug() << qPrintable(path) << "loaded";
}

QImage SnapshotModel::image(const QString &tag)
{
    if (!m_images.contains(tag)) {
         QString path = m_cacheDir.filePath(tag + ".png");
         QImage img(path);
         if (img.isNull())
             return img;
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
