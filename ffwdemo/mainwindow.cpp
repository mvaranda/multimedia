#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

MainWindow * mainApp;

bool MainWindow::initPlayerResources()
{
  //ffwplayer_t * ffw_h;
  msg_thread_h main_msg_th;
  //msg_t msg;

  //log_init();
  mainApp = this;


  int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret != 0) {
    LOG("Could not initialize SDL - %s\n.", SDL_GetError());
    return false;
  }

  // create a message queue for this main thread
  if ( ! (main_msg_th = reg_msg_thread(pthread_self(), 6)) ) {
    LOG_E("Fail to create msg system for main App thread");
    return false;
  }

//  if ( ! (ffw_h = ffw_create_player(argv[1], main_msg_th, 0))) {
//    LOG_E("No memo for ffwplayer_t object");
//    return -1;
//  }
  return true;
}

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
{
  initPlayerResources();
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
    videoCells[i].t_url->setPlainText("/home/mvaranda/Videos/ironman.mp4");
    int w = videoCells[i].video_area->width();
    int h = videoCells[i].video_area->height();
    videoCells[i].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
    // resize: https://exceptionshub.com/qt-resizing-a-qlabel-containing-a-qpixmap-while-keeping-its-aspect-ratio.html
    //         https://stackoverflow.com/questions/14107144/how-do-i-make-an-image-resize-to-scale-in-qt

  }
  QString s = videoCells[0].t_url->toPlainText();
  const std::string& stdS = s.toStdString();
  char buf[MAX_URL_LEN];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, stdS.c_str(), stdS.length());

  connect(this, &MainWindow::imageChanged, this, [&](QImage image){
    int w = videoCells[0].video_area->width();
    int h = videoCells[0].video_area->height();
    videoCells[0].video_area->setPixmap(QPixmap::fromImage(image).scaled(w,h,Qt::KeepAspectRatio));
  });

#if 1
  if ( ! (videoCells[0].ffw_h = ffw_create_player(buf, main_msg_th, 0))) {
    LOG_E("No memo for ffwplayer_t object");
  }
#endif

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
  emit imageChanged(*image);
  delete image;
}

extern "C" {

void update_picture_widget(ffwplayer_t * ffw, VideoPicture * video_picture)
{
  mainApp->updatePicture(ffw, video_picture);
}

}


