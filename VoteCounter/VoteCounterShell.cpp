#include "VoteCounterShell.hpp"
#include "SnapshotModel.hpp"
#include "ScopedDetention.hpp"

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
              << "sizeFilter"
              << "heckleUrl";

VoteCounterShell::VoteCounterShell(QWidget *parent) :
    QMainWindow(parent),
    m_snapshot(0),
    m_lastWorkMode(0),
    m_fsModel(new QFileSystemModel( this ))
{
    m_fsModel->setObjectName("fsModel");

    m_waitDialog = new QMessageBox(this);
    m_waitDialog->setWindowTitle("Counting...");
    m_waitDialog->setText("Please wait while we count the cards");
    m_waitDialog->setStandardButtons(QMessageBox::NoButton);
    m_waitDialog->setWindowModality(Qt::WindowModal);
}

VoteCounterShell::~VoteCounterShell()
{
    saveSettings();
    if (m_snapshot)
        delete m_snapshot;
}

void VoteCounterShell::loadSettings()
{

    QGraphicsView * display = findChild<QGraphicsView*>("display");
    Q_ASSERT(display);
    display->installEventFilter(this);

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
        } else if ((o->metaObject()->indexOfProperty("text") >= 0)) {
            QVariant value = m_settings.value(name);
            if (value.isValid())
                o->setProperty("text", value);
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
        } else if ((o->metaObject()->indexOfProperty("text") >= 0)) {
            m_settings.setValue(name, o->property("text"));
        }
    }

    m_settings.sync();
}



void VoteCounterShell::on_snapDirPicker_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "Snapshots directory",
                                                     m_settings.value("snaps_dir", QString()).toString());
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
        m_fsModel->setRootPath( path );
        list->setModel(m_fsModel);
        list->setRootIndex(m_fsModel->index(path));
        m_fsModel->setFilter( QDir::Files );
        m_fsModel->setNameFilters( QStringList() << "*.jpg" << "*.JPG"  );
        m_fsModel->setNameFilterDisables(false);
    }
}

void VoteCounterShell::on_fsModel_directoryLoaded(QString path)
{
    QListView * list = findChild<QListView*>("snapsList");
    int minlistw = list->sizeHintForColumn(0);
    list->setMinimumWidth( minlistw );

    QSplitter * splitter = findChild<QSplitter*>("splitter");
    splitter->setSizes( QList<int>() << minlistw << splitter->width() - minlistw - splitter->handleWidth() );

    m_fsModel->sort(3, Qt::DescendingOrder); // newest first
    QModelIndex newest = m_fsModel->index(0, 0, list->rootIndex());

    if ( newest.data().toString() != m_lastNewest ) {
        m_lastNewest = newest.data().toString();
        list->setCurrentIndex(newest);
        on_snapsList_clicked(newest);
    }
}

void VoteCounterShell::on_snapsList_clicked( const QModelIndex & index )
{
    QString snap = index.data( ).toString();
    QString dir = m_settings.value("snaps_dir").toString();
    loadSnapshot( dir + "/" + snap);

    connect(m_snapshot, SIGNAL(willCount()), SLOT(willCount()));
    connect(m_snapshot, SIGNAL(doneCounting()), SLOT(doneCounting()));

    findChild<QPushButton*>("count")->animateClick();
}

void VoteCounterShell::loadSnapshot(const QString &path)
{
    if (m_snapshot) delete m_snapshot;
    m_snapshot = new SnapshotModel(path, this);

    QGraphicsView * display = findChild<QGraphicsView*>("display");
    display->setScene( m_snapshot->scene() );
    display->fitInView( display->sceneRect(), Qt::KeepAspectRatio );

    recallLastWorkMode();
}

bool VoteCounterShell::eventFilter(QObject * object, QEvent * event)
{
    // avoid event filter recursion
    if (!m_eventFilterSentinel.contains(event)) {
        // will detain event in the sentinel while inside this scope
        ScopedDetention<QEvent*> detention(m_eventFilterSentinel, event);

        QGraphicsView * display = findChild<QGraphicsView*>("display");
        if (object==display && event->type() == QEvent::Resize) {
            QApplication::sendEvent(display,event);
            display->fitInView( display->sceneRect(), Qt::KeepAspectRatio );
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
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

void VoteCounterShell::willCount()
{
    m_waitDialog->show();
}

void VoteCounterShell::doneCounting()
{
    m_waitDialog->hide();
}
