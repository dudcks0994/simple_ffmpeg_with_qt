#include "videoworker.h"
#include <windows.h>
#include <realtimeapiset.h>

void VideoWorker::usleep(int s)
{
    u_int64 start, cur, gap;
    typedef VOID (WINAPI *FuncQueryInterruptTime)(_Out_ PULONGLONG lpInterruptTime);
    HMODULE hModule = ::LoadLibrary(L"KernelBase.dll");
    FuncQueryInterruptTime func_QueryInterruptTime =
        (FuncQueryInterruptTime)::GetProcAddress(hModule, "QueryInterruptTime");
    func_QueryInterruptTime(&start);
    while(1)
    {
        func_QueryInterruptTime(&cur);
        gap = (cur - start) / 10000.0;
        if (gap >= s)
            break;
    }
}

void VideoWorker::ButtonEvent()
{
    if (status == STOP)
        status = PLAY;
    else if (status == PLAY)
        status = STOP;
}

void VideoWorker::run()
{
    AVPacket packet;
    AVFrame frame;
    memset(&packet, 0, sizeof(packet));
    memset(&frame, 0, sizeof(AVFrame));
    uint8_t* buf;
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
                    status = END;
                    return ;
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
                scale_context = sws_getContext(frame.width, frame.height, AVPixelFormat(frame.format), width, height, AV_PIX_FMT_RGB32, SWS_BICUBIC, 0, 0, 0);
                // qDebug() << frame.height << ", " << height;
                int bufsize = av_image_get_buffer_size(AV_PIX_FMT_RGB32, width, height, 1);
                buf = (uint8_t*)av_malloc(bufsize);
                av_image_fill_arrays(convert_frame.data, convert_frame.linesize, buf, AV_PIX_FMT_RGB32, width, height, 1);
            }
            sws_scale(scale_context, frame.data, frame.linesize, 0, frame.height, convert_frame.data, convert_frame.linesize);
            unsigned char *p = convert_frame.data[0];
            // image_update_lock->lock();
            uchar *bits = image.bits();
            memcpy(bits, p, width * height * 4);
            emit frameReady(image);
            // image_update_lock->unlock();
            usleep(delay);
            if (status == STOP)
                return ;
        }
        av_packet_unref(&packet);
    }
    return ;
}
