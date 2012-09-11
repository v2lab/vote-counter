#include "VoteCounterShell.hpp"
#include "ProjectSettings.hpp"

VoteCounterShell::VoteCounterShell(QWidget *parent) :
    QMainWindow(parent)
{
}

void VoteCounterShell::on_snapDirPicker_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, "Snapshots directory");
    if (path.isNull())
        return;

    QPushButton * picker = qobject_cast<QPushButton*>(QObject::sender());
    if (picker) {
        QFont f = picker->font();
        QFontMetrics fm = QFontMetrics(f);
        int maxWidth = picker->width();
        QString elidedString = fm.elidedText(path ,Qt::ElideLeft, maxWidth);
        picker->setText(elidedString);
    }

    projectSettings().setValue( "snaps_dir", path);
    projectSettings().sync();
}

void VoteCounterShell::loadSettings()
{
    QString path = projectSettings().value("snaps_dir", QString()).toString();

    QPushButton * picker = findChild<QPushButton*>("snapDirPicker");
    if (picker && !path.isNull()) {
        QFont f = picker->font();
        QFontMetrics fm = QFontMetrics(f);
        int maxWidth = picker->width();
        QString elidedString = fm.elidedText(path ,Qt::ElideLeft, maxWidth);
        picker->setText(elidedString);
    }

}
