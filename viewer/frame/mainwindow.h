#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "frame/mainwidget.h"
#include "widgets/dwindowframe.h"
#include <QWidget>

class MainWindow : public DWindowFrame
{
public:
    // If manager is false, the Manager panel(eg.TimelinePanel) will not be
    // initialize to save resource and avoid DB file lock.
    MainWindow(bool manager, QWidget *parent=0);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void moveFirstWindow();
    void moveCenter();

    MainWidget *m_mainWidget;
};

#endif // MAINWINDOW_H