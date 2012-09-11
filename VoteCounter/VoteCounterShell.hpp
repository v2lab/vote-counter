#ifndef VOTECOUNTERSHELL_HPP
#define VOTECOUNTERSHELL_HPP

#include <QMainWindow>

class VoteCounterShell : public QMainWindow
{
    Q_OBJECT
public:
    explicit VoteCounterShell(QWidget *parent = 0);

signals:

public slots:
    void loadSettings();
    void on_snapDirPicker_clicked();

protected:
};

#endif // VOTECOUNTERSHELL_HPP
