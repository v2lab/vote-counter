#include "VoteCounterShell.hpp"
#include "ProjectSettings.hpp"
#include "SnapshotModel.hpp"

#include <QDir>
#include <QListWidget>
#include <QFileSystemModel>
#include <QGraphicsView>
#include <QRadioButton>
#include <QButtonGroup>

VoteCounterShell::VoteCounterShell(QWidget *parent) :
    QMainWindow(parent), m_snapshot(0), m_lastWorkMode(0)
{
}

VoteCounterShell::~VoteCounterShell()
{
    if (m_snapshot)
        delete m_snapshot;
}

void VoteCounterShell::loadSettings()
{
    QSpinBox * spinner = findChild<QSpinBox*>("sizeLimit");
    bool ok;
    int value = projectSettings().value("size_limit").toInt(&ok);
    if (ok) spinner->setValue( value );

    spinner = findChild<QSpinBox*>("pickFuzz");
    value = projectSettings().value("pick_fuzz").toInt(&ok);
    if (ok) spinner->setValue( value );

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

    recallLastWorkMode();
}

void VoteCounterShell::on_pickFuzz_valueChanged( int newValue )
{
    projectSettings().setValue("pick_fuzz", newValue);
    projectSettings().sync();
}

void VoteCounterShell::on_sizeLimit_valueChanged( int newValue )
{
    projectSettings().setValue("size_limit", newValue);
    projectSettings().sync();
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
        on_trainModeGroup_buttonClicked(grp->checkedButton());
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

void VoteCounterShell::on_trainModeGroup_buttonClicked( QAbstractButton * button )
{
    if (!m_snapshot) return;
    QString trainMode = button->text().toLower();
    m_snapshot->setTrainMode( trainMode );
}

void VoteCounterShell::on_clearTrainLayer_clicked()
{
    if (!m_snapshot) return;
    m_snapshot->clearLayer();
}

void VoteCounterShell::on_learn_clicked()
{
    if (!m_snapshot) return;
    m_snapshot->trainColors();
}

void VoteCounterShell::on_count_clicked()
{
    if (!m_snapshot) return;
    m_snapshot->countCards();
}
