#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  videoCells[0].video_area = ui->lb_video_area_0;
  videoCells[0].t_url = ui->t_url_0;
  videoCells[0].bt_file = ui->bt_select_file_0;

  videoCells[1].video_area = ui->lb_video_area_1;
  videoCells[1].t_url = ui->t_url_1;
  videoCells[1].bt_file = ui->bt_select_file_1;

  videoCells[2].video_area = ui->lb_video_area_2;
  videoCells[2].t_url = ui->t_url_2;
  videoCells[2].bt_file = ui->bt_select_file_2;

  videoCells[3].video_area = ui->lb_video_area_10;
  videoCells[3].t_url = ui->t_url_10;
  videoCells[3].bt_file = ui->bt_select_file_10;

  videoCells[4].video_area = ui->lb_video_area_11;
  videoCells[4].t_url = ui->t_url_11;
  videoCells[4].bt_file = ui->bt_select_file_11;

  videoCells[5].video_area = ui->lb_video_area_12;
  videoCells[5].t_url = ui->t_url_12;
  videoCells[5].bt_file = ui->bt_select_file_12;

  QImage image(":/images/3X_screen.png");
  for (int i=0; i < NUM_VIDEO_CELLS; i++) {
    videoCells[i].t_url->setPlainText("~/Videos/ironman.mp4");
    int w = videoCells[i].video_area->width();
    int h = videoCells[i].video_area->height();
    videoCells[i].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
  }

  // pre populate
//  videoCells[0].t_url->setPlainText("~/Videos/ironman.mp4");
//  QImage image(":/images/3X_screen.png");
//  int w = videoCells[0].video_area->width();
//  int h = videoCells[0].video_area->height();
//  videoCells[0].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));

  // resize: https://exceptionshub.com/qt-resizing-a-qlabel-containing-a-qpixmap-while-keeping-its-aspect-ratio.html
  //         https://stackoverflow.com/questions/14107144/how-do-i-make-an-image-resize-to-scale-in-qt


}

MainWindow::~MainWindow()
{
  delete ui;
}


