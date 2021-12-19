/******************************************
 *
 *             Multiplayer Demo
 *
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 *
 ******************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>

#define DEMO_VERSION "0.0.1"
MainWindow * mainApp;

QString demo_videos_dir = "/home/mvaranda/Videos/";
QString demo_video_names[] =  {
  "rtsp://10.10.10.90/1",
  demo_videos_dir + "agent_327.mp4",
  demo_videos_dir + "caminandes-2-gran-dillama.mp4",
  demo_videos_dir + "sintel-the-movie-720p.mp4",
  demo_videos_dir + "tears_of_steel_720p.mov",
  ""
};


bool MainWindow::initPlayerResources()
{
  msg_thread_h main_msg_th;
  mainApp = this;

  // create a message queue for this main thread
  if ( ! (main_msg_th = reg_msg_thread(pthread_self(), 6)) ) {
    LOG_E("Fail to create msg system for main App thread");
    return false;
  }

  return true;
}

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  initPlayerResources();

  ui->setupUi(this);
  setWindowTitle("Multi-Camera Player - Version " DEMO_VERSION);
  move(screen()->geometry().center() - frameGeometry().center());

  videoCells[0].video_area = ui->lb_video_area_0;
  videoCells[0].t_url = ui->t_url_0;
  videoCells[0].bt_file = ui->bt_select_file_0;
  videoCells[0].chk_mute = ui->bt_mute_0;

  videoCells[1].video_area = ui->lb_video_area_1;
  videoCells[1].t_url = ui->t_url_1;
  videoCells[1].bt_file = ui->bt_select_file_1;
  videoCells[1].chk_mute = ui->bt_mute_1;

  videoCells[2].video_area = ui->lb_video_area_2;
  videoCells[2].t_url = ui->t_url_2;
  videoCells[2].bt_file = ui->bt_select_file_2;
  videoCells[2].chk_mute = ui->bt_mute_2;

  videoCells[3].video_area = ui->lb_video_area_10;
  videoCells[3].t_url = ui->t_url_10;
  videoCells[3].bt_file = ui->bt_select_file_10;
  videoCells[3].chk_mute = ui->bt_mute_3;

  videoCells[4].video_area = ui->lb_video_area_11;
  videoCells[4].t_url = ui->t_url_11;
  videoCells[4].bt_file = ui->bt_select_file_11;
  videoCells[4].chk_mute = ui->bt_mute_4;


  videoCells[5].video_area = ui->lb_video_area_12;
  videoCells[5].t_url = ui->t_url_12;
  videoCells[5].bt_file = ui->bt_select_file_12;
  videoCells[5].chk_mute = ui->bt_mute_5;

  QImage image(":/images/3X_screen.png");
  for (int i=0; i < NUM_VIDEO_CELLS; i++) {
    videoCells[i].ffw_h = NULL;
    videoCells[i].t_url->setPlainText(demo_video_names[i]);
    int w = videoCells[i].video_area->width();
    int h = videoCells[i].video_area->height();
    videoCells[i].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
    videoCells[i].video_area->setContextMenuPolicy(Qt::CustomContextMenu);
    videoCells[i].chk_mute->setChecked(true);
  }


  connect(this, &MainWindow::imageChanged, this, [&](QImage image, ffwplayer_t * ffw){
    uint64_t i = (uint64_t) ffw->client_data;
    int w = videoCells[i].video_area->width();
    int h = videoCells[i].video_area->height();
    videoCells[i].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
  });

}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::updatePicture(ffwplayer_t * ffw, VideoPicture * video_picture)
{
  QImage *image = new QImage(QSize(video_picture->width, video_picture->height), QImage::Format_RGB32);
  void * data_ptr = video_picture->frame->data[0];
  memcpy(image->bits(), data_ptr, static_cast<size_t>(image->sizeInBytes()));
  emit imageChanged(*image, ffw);
  delete image;
}

extern "C" {

void update_picture_widget(ffwplayer_t * ffw, VideoPicture * video_picture)
{
  //uint64_t idx = (uint64_t) ffw->client_data;
  mainApp->updatePicture(ffw, video_picture);
}

}



void MainWindow::on_pushButton_clicked()
{
  int i;
  QString s;
  char buf[MAX_URL_LEN];

  for (i=0; i<NUM_VIDEO_CELLS; i++) {
    if (videoCells[i].ffw_h != NULL) {
      LOG("Skip videoCell %d as already playing", i);
      continue;
    }
    s = videoCells[i].t_url->toPlainText();
    const std::string& stdS = s.toStdString();
    if (stdS.length() < 2) {
      LOG("Skip videoCell %d as no URL given", i);
      continue;
    }
    memset(buf, 0, sizeof(buf));
    memcpy(buf, stdS.c_str(), stdS.length());
    if ( ! (videoCells[i].ffw_h = ffw_create_player(buf, main_msg_th, (void *) i))) {
      LOG_E("No memo for ffwplayer_t object");
    } else {
      // mute
      ffw_mute(videoCells[i].ffw_h, true);
    }
  }
}

void MainWindow::handleMute(int i)
{
  if (videoCells[i].ffw_h == NULL) return;

  if (videoCells[i].chk_mute->checkState()) {
    LOG("mute");
    ffw_mute(videoCells[i].ffw_h, true);
  } else {
    LOG("unmute");
    ffw_mute(videoCells[i].ffw_h, false);
  }
}
void MainWindow::on_bt_mute_0_stateChanged(int arg1)
{
  handleMute(0);
}

void MainWindow::on_bt_mute_1_stateChanged(int arg1)
{
  handleMute(1);
}

void MainWindow::on_bt_mute_2_stateChanged(int arg1)
{
  handleMute(2);
}

void MainWindow::on_bt_mute_3_stateChanged(int arg1)
{
  handleMute(3);
}

void MainWindow::on_bt_mute_4_stateChanged(int arg1)
{
  handleMute(4);
}

void MainWindow::on_bt_mute_5_stateChanged(int arg1)
{
  handleMute(5);
}

void MainWindow::doPlay()
{
  int i = videoContextMenuItemIdx;
  LOG("doPlay %d", i);


}

void MainWindow::doPause()
{
  int i = videoContextMenuItemIdx;
  LOG("doPause %d", i);


}

void MainWindow::doStop()
{
  int i = videoContextMenuItemIdx;
  LOG("doStop %d", i);


}

void MainWindow::doFull()
{
  int i = videoContextMenuItemIdx;
  LOG("doFull %d", i);

  videoCells[i].video_area->setWindowFlags(videoCells[i].video_area->windowFlags() | Qt::Window);
  videoCells[i].video_area->show();
  //ui->myImage->showMaximized();
  videoCells[i].video_area->showFullScreen();
}

void MainWindow::doNormal  ()
{
  int i = videoContextMenuItemIdx;
  LOG("doNormal %d", i);

  videoCells[i].video_area->setWindowFlag(Qt::Window, false);
  videoCells[i].video_area->show();
}

void MainWindow::doVideoContextMenu(int i, const QPoint &pos)
{
  videoContextMenuItemIdx = i;
  QMenu * menu = new QMenu(this);
  /* Create actions to the context menu */
  QAction * actPlay = new QAction(QString("Play"), this);
  QAction * actPause = new QAction(QString("Pause"), this);
  QAction * actStop = new QAction(QString("Stop"), this);
  QAction * actFull = new QAction(QString("Full Screen"), this);
  QAction * actNormal = new QAction(QString("Normal Screen"), this);

  connect(actPlay, SIGNAL(triggered()), this, SLOT(doPlay()));
  connect(actPause, SIGNAL(triggered()), this, SLOT(doPause()));
  connect(actStop, SIGNAL(triggered()), this, SLOT(doStop()));
  connect(actFull, SIGNAL(triggered()), this, SLOT(doFull()));
  connect(actNormal, SIGNAL(triggered()), this, SLOT(doNormal()));

  menu->addAction(actPlay);
  menu->addAction(actPause);
  menu->addAction(actStop);
  menu->addAction(actFull);
  menu->addAction(actNormal);

  /* Call the context menu */
  QPoint globalCursorPos = videoCells[i].video_area->mapTo(this,pos);

  menu->popup(mapToGlobal(globalCursorPos));
  //menu->popup(mapToGlobal(pos));
}

void MainWindow::on_lb_video_area_0_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(0, pos);
}


void MainWindow::on_lb_video_area_1_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(1, pos);
}


void MainWindow::on_lb_video_area_2_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(2, pos);
}


void MainWindow::on_lb_video_area_10_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(3, pos);
}


void MainWindow::on_lb_video_area_11_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(4, pos);
}


void MainWindow::on_lb_video_area_12_customContextMenuRequested(const QPoint &pos)
{
  doVideoContextMenu(5, pos);
}

