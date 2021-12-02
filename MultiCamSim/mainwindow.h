#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_bt_play_clicked();

private:
    Ui::MainWindow *ui;
    QLabel * video_area_label;
    QThread *gst_thread;
    void resizeEvent(QResizeEvent*);
    void moveEvent(QMoveEvent *event);
    void updateVideoArea();

};
#endif // MAINWINDOW_H
