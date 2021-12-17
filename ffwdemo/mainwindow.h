#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

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
    };
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QLabel * video_area_0 = nullptr;
    QPlainTextEdit * t_url_0 = nullptr;
    QPushButton * bt_file = nullptr;

    VideoCell videoCells[NUM_VIDEO_CELLS];
};
#endif // MAINWINDOW_H
