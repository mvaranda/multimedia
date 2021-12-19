/******************************************
 *
 *             Multiplayer Demo
 *
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 *
 ******************************************/


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>

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
      VideoCell() :
      video_area(nullptr),
        t_url(nullptr),
        bt_file(nullptr),
        ffw_h(nullptr){}

        QLabel * video_area = nullptr;
        QPlainTextEdit * t_url = nullptr;
        QPushButton * bt_file = nullptr;
        QCheckBox * chk_mute = nullptr;
        ffwplayer_t * ffw_h = nullptr;

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
    int videoContextMenuItemIdx;

    bool initPlayerResources();
    void handleMute(int i);
    void doVideoContextMenu(int i, const QPoint &pos);


signals:
    void imageChanged(QImage image, ffwplayer_t * ffw);
private slots:
    void on_pushButton_clicked();
    void on_bt_mute_0_stateChanged(int arg1);
    void on_bt_mute_1_stateChanged(int arg1);
    void on_bt_mute_2_stateChanged(int arg1);
    void on_bt_mute_3_stateChanged(int arg1);
    void on_bt_mute_4_stateChanged(int arg1);
    void on_bt_mute_5_stateChanged(int arg1);
    void on_lb_video_area_0_customContextMenuRequested(const QPoint &pos);

    void doPlay();
    void doPause();
    void doStop();
    void doFull();
    void doNormal();

};
#endif // MAINWINDOW_H
