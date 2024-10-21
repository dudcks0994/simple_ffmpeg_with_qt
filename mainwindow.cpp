#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "videoworker.h"
#include <qpushbutton.h>
#include <QPainter>
#include <QImage>
#include <QMutex>
#include <QSlider>

#define WIDTH 720
#define HEIGHT 480

void MainWindow::onFrameReady(const QImage &image)
{
    this->image = image;
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
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(playButton);
    connect(video_worker, SIGNAL(frameReady(const QImage &)), SLOT(onFrameReady(const QImage &)));
    playButton = new QPushButton("Play", this);
    playButton->setGeometry(10, 10, 50, 30);
    image = QImage(width, height, QImage::Format_RGB32);
    connect(playButton, &QPushButton::clicked, [this]() {
        if (this->playButton->text() == "Play")
            this->playButton->setText("Stop");
        else
            this->playButton->setText("Play");
        video_worker->start();
        video_worker->ButtonEvent();
    });
    // connect(timer, &QTimer::timeout, [this](){
    //     int ret = update_image();
    //     if (ret == -1)
    //         timer->stop();
    // });
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

int MainWindow::update_image() {
    AVPacket packet;
    AVFrame frame;
    static AVFrame convert_frame;
    memset(&packet, 0, sizeof(packet));
    memset(&frame, 0, sizeof(AVFrame));

    while (av_read_frame(fmtCtx, &packet) == 0) {
        if (packet.stream_index == vidx) {
            int ret = avcodec_send_packet(video_context, &packet);
            if (ret != 0) {
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                } else if (ret == AVERROR_EOF) {
                    avcodec_flush_buffers(video_context);
                    break;
                } else {
                    return (-1);
                }
            }
            ret = avcodec_receive_frame(video_context, &frame);
            if (ret != 0) {
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                } else if (ret == AVERROR_EOF) {
                    avcodec_flush_buffers(video_context);
                    break;
                }
            }
            if (!scale_context)
            {
                scale_context = sws_getContext(frame.width, frame.height, AVPixelFormat(frame.format), frame.width, frame.height, AV_PIX_FMT_RGB32, SWS_BICUBIC, 0, 0, 0);
                int bufsize = av_image_get_buffer_size(AV_PIX_FMT_RGB32, frame.width, frame.height, 1);
                buf = (uint8_t*)av_malloc(bufsize);
                av_image_fill_arrays(convert_frame.data, convert_frame.linesize, buf, AV_PIX_FMT_RGB32, frame.width, frame.height, 1);
            }
            sws_scale(scale_context, frame.data, frame.linesize, 0, frame.height, convert_frame.data, convert_frame.linesize);
            // 픽셀 데이터를 QImage로 변환
            unsigned char *p = convert_frame.data[0];
            uchar *bits = image.bits();
            memcpy(bits, p, frame.width * frame.height * 4);
            // int bytesPerLine = image.bytesPerLine();
            // for (int y = 0; y < frame.height; ++y)
            // {
            //     for (int x = 0; x < frame.width; ++x)
            //     {
            //         int index = (y * frame.width + x) * 4;
            //         // printf("%d %d %d %d\n", p[index], p[index + 1], p[index + 2], p[index + 3]);
            //         bits[index + 3] = p[index + 3];
            //         bits[index + 2] = p[index + 2];
            //         bits[index + 1] = p[index + 1];
            //         bits[index + 0] = p[index + 0];
            //     }
            // }
            // for (int i = 0; i < frame.width * frame.height * 4; ++i)
            // {
            //     bits[i] = p[i];
            // }
            // for (int y = 0; y < frame.height; ++y) {
            //     for (int x = 0; x < WIDTH; ++x) {
            //         int offset = (y * WIDTH + x) * 3;
            //         int index = y * bytesPerLine + x * 4;
            //         // bits[index + 3] = 255;
            //         bits[index + 2] = p[offset];
            //         bits[index + 1] = p[offset + 1];
            //         bits[index] = p[offset + 2];
            //         // image.setPixel(x, y, qRgb(p[offset + 0], p[offset + 1], p[offset + 2]));
            //     }
            // }

            break;
        }
        else if (packet.stream_index == aidx)
        {
            int ret = avcodec_send_packet(audio_context, &packet);
            if (ret != 0)
            {
                if (ret == AVERROR(EAGAIN)) //센드에서 이 에러는 버퍼가 다 찼다는 얘기이므로 프레임 받기를 통해 버퍼를 비워야함
                    ;
                else if (ret == AVERROR_EOF) // 루프탈출
                    break;
                else
                    return (-1);
            }
            ret = avcodec_receive_frame(audio_context, &frame);
            if(ret != 0)
            {
                if (ret == AVERROR(EAGAIN)) // receive에서 이 에러는 프레임을 만들기위해 패킷이 더 필요하단 것이므로 넘기면 됌
                {
                    continue;
                }
                else if (ret == AVERROR_EOF)
                {
                    avcodec_flush_buffers(audio_context);
                    break;
                }
            }
            audio_frame.push(frame);
        }
    }
    av_packet_unref(&packet);
    // 업데이트된 이미지를 그리도록 요청
    update();
    return (0);
}

int MainWindow::init_video()
{
    std::string path = filepath.toStdString();
    const char *url = path.c_str();
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
    rate = video_stream->r_frame_rate;
    return 0;
}
