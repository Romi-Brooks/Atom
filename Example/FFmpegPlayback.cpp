#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <SDL3/SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

int main() {
    std::string videoPath = "Media/Videos/test.mp4";

    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, videoPath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open video file" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info" << std::endl;
        return -1;
    }

    int videoStreamIndex = -1;

    AVRational videoFrameRate;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            videoFrameRate = formatContext->streams[i]->r_frame_rate;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        return -1;
    }

    double frameDelayMs = 1000.0 / av_q2d(videoFrameRate);
    if (frameDelayMs <= 0) {
        frameDelayMs = 1000.0 / 30.0;
        std::cout << "Could not get video frame rate, use default 30fps" << std::endl;
    }
    std::cout << "Video frame rate: " << av_q2d(videoFrameRate) << "fps, frame delay: " << frameDelayMs << "ms" << std::endl;

    AVCodecParameters* codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        std::cerr << "Could not find decoder" << std::endl;
        return -1;
    }

    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParams);
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return -1;
    }

    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();

    unsigned width = codecContext->width;
    unsigned height = codecContext->height;

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    uint8_t* buffer = static_cast<uint8_t*>(av_malloc(numBytes * sizeof(uint8_t)));
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);

    SwsContext* swsContext = sws_getContext(width, height, codecContext->pix_fmt,
                                               width, height, AV_PIX_FMT_RGBA,
                                               SWS_BILINEAR, nullptr, nullptr, nullptr);

    // SDL3 setup
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL3 + FFmpeg Video Test",
                                          static_cast<int>(width),
                                          static_cast<int>(height),
                                          SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    SDL_Texture* texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             static_cast<int>(width),
                                             static_cast<int>(height));

    AVPacket* packet = av_packet_alloc();

    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
            }
        }
        if (!running) break;

        bool hasNewFrame = false;
        if (av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                int sendRet = avcodec_send_packet(codecContext, packet);
                if (sendRet >= 0) {
                    int recvRet = avcodec_receive_frame(codecContext, frame);
                    if (recvRet == 0) {
                        sws_scale(swsContext, frame->data, frame->linesize, 0, height,
                                  rgbFrame->data, rgbFrame->linesize);
                        SDL_UpdateTexture(texture, nullptr, rgbFrame->data[0],
                                          static_cast<int>(width * 4));
                        hasNewFrame = true;
                    } else if (recvRet != AVERROR(EAGAIN) && recvRet != AVERROR_EOF) {
                        std::cerr << "Error receiving frame: " << recvRet << std::endl;
                    }
                } else {
                    std::cerr << "Error sending packet: " << sendRet << std::endl;
                }
            }
            av_packet_unref(packet);
        } else {
            break;
        }

        if (hasNewFrame) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = currentTime - lastFrameTime;
            double elapsedMs = elapsed.count();

            if (elapsedMs < frameDelayMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(frameDelayMs - elapsedMs)));
            }

            lastFrameTime = std::chrono::high_resolution_clock::now();
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_FRect dst = {0, 0, static_cast<float>(width), static_cast<float>(height)};
        SDL_RenderTexture(renderer, texture, nullptr, &dst);

        SDL_RenderPresent(renderer);
    }

    av_free(buffer);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
