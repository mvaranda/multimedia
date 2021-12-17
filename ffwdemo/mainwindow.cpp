#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  videoCells[0].video_area = ui->lb_video_area_0;
  videoCells[0].t_url = ui->t_url;
  videoCells[0].bt_file = ui->bt_select_file_0;

  // pre populate
  videoCells[0].t_url->setPlainText("~/Videos/ironman.mp4");
  QImage image("imgPath");
  videoCells[0].video_area->setPixmap(QPixmap::fromImage(image));

}

MainWindow::~MainWindow()
{
  delete ui;
}


