#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QThread>


#include "gst/gst.h"
#include "camstreamer.h"

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
    //QThread *gst_thread;
    void resizeEvent(QResizeEvent*);
    void moveEvent(QMoveEvent *event);
    void updateVideoArea();

    //-------------- gstreamer --------------
    GstElement *pipeline;
    GstBus *bus;
    CamStreamer * camStreamer;

};
#endif // MAINWINDOW_H
