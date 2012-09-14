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
    void loadDir(const QString& path);
    void loadSnapshot(const QString& path);
    void recallLastWorkMode();

    // automatically connected slots for children's signals
    void on_snapDirPicker_clicked();
    void on_snapsList_clicked ( const QModelIndex & index );
    void on_sizeLimit_valueChanged( int newValue );
    void on_mode_currentChanged( int index );
    void on_trainModeGroup_buttonClicked( QAbstractButton * button );
    void on_clearTrainLayer_clicked();
    void on_pickFuzz_valueChanged( int newValue );
    void on_learn_clicked();
    void on_count_clicked();

protected:
    SnapshotModel * m_snapshot;
    int m_lastWorkMode;
};

#endif // VOTECOUNTERSHELL_HPP
