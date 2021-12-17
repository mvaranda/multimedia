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

  // pre populate
  videoCells[0].t_url->setPlainText("~/Videos/ironman.mp4");
  QImage image(":/images/3X_screen.png");
  int w = videoCells[0].video_area->width();
  int h = videoCells[0].video_area->height();
  videoCells[0].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
  // resize: https://exceptionshub.com/qt-resizing-a-qlabel-containing-a-qpixmap-while-keeping-its-aspect-ratio.html
  //         https://stackoverflow.com/questions/14107144/how-do-i-make-an-image-resize-to-scale-in-qt


}

MainWindow::~MainWindow()
{
  delete ui;
}


