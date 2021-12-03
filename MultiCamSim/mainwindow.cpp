
/**********************************************************
 *
 *        M u l t i   C a m e r a   S i m u l a t o r
 *
 * License: proprietary
 *
 * Copyrights: Marcelo Varanda
 * Initial date: October 2021
 * http://www.varanda.ca
 *
 ***********************************************************/

// ref: https://github.com/GStreamer/qt-gstreamer/tree/master/examples/player

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , camStreamer(nullptr)
{
    ui->setupUi(this);
    video_area_label = ui->video_area;
    LOG("MultiCamSim started");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateVideoArea()
{
  QRect p = video_area_label->geometry();
  QRect w = this->geometry();
  int x = p.x() + w.x();
  int y = p.y() + w.y();
  LOG("x = %d, y = %d     width = %d, height = %d", x, y, p.width(), p.height());
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
  updateVideoArea();
}
void MainWindow::moveEvent(QMoveEvent *event)
{
  updateVideoArea();
}

void MainWindow::on_bt_play_clicked()
{
    if (camStreamer == nullptr) {
        camStreamer = new CamStreamer(video_area_label->winId());

        camStreamer->start();
        video_area_label->setText(tr("Started player"));
    }
    else {
        video_area_label->setText(tr("Player already running"));
    }
}

