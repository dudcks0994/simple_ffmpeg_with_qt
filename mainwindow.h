#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <queue>
#include <QTimer>
#include <QThread>
#include "videoworker.h"
#include <QMutex>
#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int init_video();
    int update_image();
    int width, height;

protected:
    void paintEvent(QPaintEvent *event) override;

public slots:
    void onFrameReady(const QImage &image);

private:

    Ui::MainWindow *ui;
    SwsContext* scale_context;
    QImage image;
    int vidx, aidx;
    uint8_t* buf;
    AVCodecContext* audio_context, *video_context;
    AVFormatContext* fmtCtx;
    queue<AVFrame> video_frame, audio_frame;
    QTimer *timer;
    VideoWorker* video_worker;
    AVRational rate;
    QPushButton *playButton;
    QString filepath;
    QSlider *playSlider, *soundSlider;
};
#endif // MAINWINDOW_H
