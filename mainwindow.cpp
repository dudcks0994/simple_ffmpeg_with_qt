#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "videoworker.h"
#include <qpushbutton.h>
#include <QPainter>
#include <QImage>
#include <QMutex>
#include <QSlider>
#include <QScreen>

#define WIDTH 720
#define HEIGHT 480

void MainWindow::onFrameReady(const QImage &image)
{
    this->image = image;
    // qDebug() << image.height() << ", " << this->image.height();
    update();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , fmtCtx(nullptr)
    , video_context(nullptr)
    , audio_context(nullptr)
    , timer(new QTimer(this))
{
    ui->setupUi(this);
    filepath = QFileDialog::getOpenFileName(this, "Open Video File", "", "Video Files (*.mp4 *.avi *.mkv *.m3u8)");
    fmtCtx = 0;
    scale_context = 0;
    if (init_video() != 0) {
        qDebug() << "Failed to initialize video.";
    }
    qDebug() << "success to initialize video\n";
    video_worker = new VideoWorker(this);
    video_worker->setContext(fmtCtx, video_context, vidx, width, height, rate.den, rate.num);
    connect(video_worker, SIGNAL(frameReady(const QImage &)), SLOT(onFrameReady(const QImage &)));
    playButton = new QPushButton("Play", this);
    playButton->setGeometry(10, 10, 50, 30);
    image = QImage(width, height, QImage::Format_RGB32);
    memset(image.bits(), 0, sizeof(image));
    connect(playButton, &QPushButton::clicked, [this]() {
        if (this->playButton->text() == "Play")
            this->playButton->setText("Stop");
        else
            this->playButton->setText("Play");
        video_worker->start();
        video_worker->ButtonEvent();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
    }
    if (video_context) {
        avcodec_free_context(&video_context);
    }
    if (audio_context) {
        avcodec_free_context(&audio_context);
    }
}

void MainWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    // QImage를 창에 그리기
    painter.drawImage(10, 40, image);
}

int MainWindow::init_video()
{
    std::string path = filepath.toStdString();
    const char *url = path.c_str();
    // const char *url = "C:\Users\kinki\Downloads\new\karina_alike.mp4"
    // const char *url = "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.m3u8";
    int ret = avformat_open_input(&fmtCtx, url, nullptr, nullptr);
    if (ret != 0)
        return -1;

    avformat_find_stream_info(fmtCtx, nullptr);
    vidx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    aidx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (vidx == -1 || aidx == -1)
        return (-1);
    AVStream *video_stream = fmtCtx->streams[vidx];
    AVStream *audio_stream = fmtCtx->streams[aidx];

    const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    const AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);

    video_context = avcodec_alloc_context3(video_codec);
    audio_context = avcodec_alloc_context3(audio_codec);

    if (avcodec_parameters_to_context(video_context, video_stream->codecpar) != 0)
        return -1;
    if (avcodec_parameters_to_context(audio_context, audio_stream->codecpar) != 0)
        return -1;
    if (avcodec_open2(video_context, video_codec, nullptr) != 0)
        return -1;
    if (avcodec_open2(audio_context, audio_codec, nullptr) != 0)
        return -1;
    width = video_context->width;
    height = video_context->height;
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize resolution(0, 0);
    if (screen)
        resolution = screen->size();
    if (resolution.width() != 0 && (width + 200 > resolution.width() || height + 200 > resolution.height()))
    {
        double ratio;
        if (width >= resolution.width())
            ratio = (1.0 * (resolution.width())) / (width + 300);
        else
            ratio = (1.0 * (resolution.height())) / (height + 300);
        width = width * 1.0 * ratio;
        height = height * 1.0 * ratio;
    }
    rate = video_stream->r_frame_rate;
    return 0;
}
