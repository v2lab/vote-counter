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

    void willCount();
    void doneCounting();

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
    QMessageBox * m_waitDialog;
    QString m_lastNewest;

    static QStringList s_persistentObjectNames;

    virtual bool eventFilter(QObject *, QEvent *);
    QSet<QEvent*> m_eventFilterSentinel;

};

#endif // VOTECOUNTERSHELL_HPP
