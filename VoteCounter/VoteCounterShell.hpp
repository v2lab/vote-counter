#ifndef VOTECOUNTERSHELL_HPP
#define VOTECOUNTERSHELL_HPP

#include <QMainWindow>

class SnapshotModel;

class VoteCounterShell : public QMainWindow
{
    Q_OBJECT
public:
    explicit VoteCounterShell(QWidget *parent = 0);
    ~VoteCounterShell();

signals:

public slots:
    void loadSettings();
    void saveSettings();
    void loadDir(const QString& path);
    void loadSnapshot(const QString& path);
    void recallLastWorkMode();

    // automatically connected slots for children's signals
    void on_snapDirPicker_clicked();
    void on_snapsList_clicked ( const QModelIndex & index );
    void on_mode_currentChanged( int index );
    void on_fsModel_directoryLoaded(QString path);

protected:
    SnapshotModel * m_snapshot;
    int m_lastWorkMode;
    QSettings m_settings;
    QFileSystemModel * m_fsModel;

    static QStringList s_persistentObjectNames;
};

#endif // VOTECOUNTERSHELL_HPP
