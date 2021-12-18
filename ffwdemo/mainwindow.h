#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

#include "log.h"
#include "msg_thread.h"
#include "ffwplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define NUM_VIDEO_CELLS 6

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    class VideoCell{
    public:
        QLabel * video_area;
        QPlainTextEdit * t_url;
        QPushButton * bt_file;
        ffwplayer_t * ffw_h;
    };
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void updatePicture(ffwplayer_t * ffw, VideoPicture * video_picture);

private:
    Ui::MainWindow *ui;
    QLabel * video_area_0 = nullptr;
    QPlainTextEdit * t_url_0 = nullptr;
    QPushButton * bt_file = nullptr;

    VideoCell videoCells[NUM_VIDEO_CELLS];
    msg_thread_h main_msg_th;

    bool initPlayerResources();

signals:
    void imageChanged(QImage image);
};
#endif // MAINWINDOW_H
