#include "VoteCounterShell.hpp"
#include "ProjectSettings.hpp"
#include "SnapshotModel.hpp"

#include <QDir>
#include <QListWidget>
#include <QFileSystemModel>
#include <QGraphicsView>

VoteCounterShell::VoteCounterShell(QWidget *parent) :
    QMainWindow(parent), m_snapshot(0)
{
}

void VoteCounterShell::loadSettings()
{
    QSpinBox * spinner = findChild<QSpinBox*>("sizeLimit");
    if (spinner) {
        spinner->setValue( projectSettings().value("size_limit", 640).toInt() );

    }
    loadDir( projectSettings().value("snaps_dir", QString()).toString() );
}

void VoteCounterShell::on_snapDirPicker_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "Snapshots directory");
    if (path.isNull())
        return;

    loadDir( path );
    // and save it in settings
    projectSettings().setValue( "snaps_dir", path);
    projectSettings().sync();
}

void VoteCounterShell::loadDir(const QString &path)
{
    // update Prefs button
    QPushButton * picker = findChild<QPushButton*>("snapDirPicker");
    if (picker && !path.isNull()) {
        QFont f = picker->font();
        QFontMetrics fm = QFontMetrics(f);
        int maxWidth = picker->width();
        QString elidedString = fm.elidedText(path ,Qt::ElideLeft, maxWidth);
        picker->setText(elidedString);
    }

    // load the file list
    QListView * list = findChild<QListView*>("snapsList");
    if (list) {
        QFileSystemModel * model = new QFileSystemModel( this );
        model->setRootPath( path );
        list->setModel(model);
        list->setRootIndex(model->index(path));
        model->setFilter( QDir::Files );
        model->setNameFilters( QStringList() << "*.jpg" << "*.JPG"  );
    }
}

void VoteCounterShell::on_snapsList_clicked( const QModelIndex & index )
{
    QString snap = index.data( ).toString();
    QString dir = projectSettings().value("snaps_dir").toString();
    loadSnapshot( dir + "/" + snap);
}

void VoteCounterShell::loadSnapshot(const QString &path)
{
    if (m_snapshot) delete m_snapshot;
    m_snapshot = new SnapshotModel(path, this);
    QGraphicsView * display = findChild<QGraphicsView*>("display");
    display->setScene( m_snapshot->scene() );
    display->fitInView( m_snapshot->scene()->sceneRect(), Qt::KeepAspectRatio );
}

void VoteCounterShell::on_sizeLimit_valueChanged( int newValue )
{
    projectSettings().setValue("size_limit", newValue);
    projectSettings().sync();
}
