#include "VoteCounterShell.hpp"
#include "SnapshotModel.hpp"

#include <QDir>
#include <QListWidget>
#include <QFileSystemModel>
#include <QGraphicsView>
#include <QRadioButton>
#include <QButtonGroup>

QStringList VoteCounterShell::s_persistentObjectNames =
QStringList() << "sizeLimit"
              << "pickFuzz"
              << "colorDiffThreshold"
              << "sizeFilter";

VoteCounterShell::VoteCounterShell(QWidget *parent) :
    QMainWindow(parent), m_snapshot(0), m_lastWorkMode(0)
{

}

VoteCounterShell::~VoteCounterShell()
{
    saveSettings();
    if (m_snapshot)
        delete m_snapshot;
}

void VoteCounterShell::loadSettings()
{
    foreach(QString name, s_persistentObjectNames) {
        QObject * o = findChild<QObject*>(name);
        if (!o) {
            qWarning() << "No such widget to load value from settings:" << name;
            continue;
        }

        if ((o->metaObject()->indexOfProperty("value") >= 0)
                || (o->dynamicPropertyNames().contains("value"))) {
            QVariant value = m_settings.value(name);
            if (value.isValid())
                o->setProperty("value", value);
        }
    }

    loadDir( m_settings.value("snaps_dir", QString()).toString() );
}

void VoteCounterShell::saveSettings()
{
    foreach(QString name, s_persistentObjectNames) {
        QObject * o = findChild<QObject*>(name);
        if (!o) {
            qWarning() << "No such widget to save value to settins:" << name;
            continue;
        }

        if ((o->metaObject()->indexOfProperty("value") >= 0)
                || (o->dynamicPropertyNames().contains("value"))) {
            m_settings.setValue(name, o->property("value"));
        }
    }

    m_settings.sync();
}



void VoteCounterShell::on_snapDirPicker_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "Snapshots directory");
    if (path.isNull())
        return;

    loadDir( path );
    // and save it in settings
    m_settings.setValue( "snaps_dir", path);
    m_settings.sync();
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
        model->setNameFilterDisables(false);
    }
}

void VoteCounterShell::on_snapsList_clicked( const QModelIndex & index )
{
    QString snap = index.data( ).toString();
    QString dir = m_settings.value("snaps_dir").toString();
    loadSnapshot( dir + "/" + snap);
}

void VoteCounterShell::loadSnapshot(const QString &path)
{
    if (m_snapshot) delete m_snapshot;
    m_snapshot = new SnapshotModel(path, this);

    QGraphicsView * display = findChild<QGraphicsView*>("display");
    display->setScene( m_snapshot->scene() );
    display->fitInView( m_snapshot->scene()->sceneRect(), Qt::KeepAspectRatio );

    recallLastWorkMode();
}

void VoteCounterShell::on_mode_currentChanged( int index )
{
    // remember last work mode so we can force it when loaded new snap
    if (index == 0 || index == 1) {
        m_lastWorkMode = index;
    }

    if (!m_snapshot)
        return;

    switch (index) {
    case 0: { // train
        QButtonGroup * grp = findChild<QButtonGroup *>("trainModeGroup");
        m_snapshot->on_trainModeGroup_buttonClicked(grp->checkedButton());
        break;
    }
    case 1: // count
        m_snapshot->setMode( SnapshotModel::COUNT );
        break;
    case 2:
        break;
    }
}

void VoteCounterShell::recallLastWorkMode()
{
    QTabWidget * mode = findChild<QTabWidget*>("mode");
    Q_ASSERT(mode);
    mode->setCurrentIndex(m_lastWorkMode);
    on_mode_currentChanged( m_lastWorkMode );
}


