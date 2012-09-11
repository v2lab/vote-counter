#ifndef VOTECOUNTERSHELL_HPP
#define VOTECOUNTERSHELL_HPP

#include <QMainWindow>

class SnapshotModel;

class VoteCounterShell : public QMainWindow
{
    Q_OBJECT
public:
    explicit VoteCounterShell(QWidget *parent = 0);

signals:

public slots:
    void loadSettings();
    void loadDir(const QString& path);
    void loadSnapshot(const QString& path);

    // automatically connected slots for children's signals
    void on_snapDirPicker_clicked();
    void on_snapsList_clicked ( const QModelIndex & index );
    void on_sizeLimit_valueChanged( int newValue );

protected:
    SnapshotModel * m_snapshot;

};

#endif // VOTECOUNTERSHELL_HPP
