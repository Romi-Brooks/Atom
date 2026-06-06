#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

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
            // Get the video native frame rate
            // 获取视频原生帧率
            videoFrameRate = formatContext->streams[i]->r_frame_rate;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        return -1;
    }

    // Calculate the display time per frame
    // 计算每帧的显示时间
    double frameDelayMs = 1000.0 / av_q2d(videoFrameRate);
    if (frameDelayMs <= 0) {  // Fallback if frame rate acquisition fails, default to 30fps
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

    sf::RenderWindow window(sf::VideoMode({width, height}), "SFML + FFmpeg Video Test");
    sf::Texture texture({width, height});
    sf::Sprite sprite(texture);

    AVPacket* packet = av_packet_alloc();

    // Record the display start time of the previous frame
    // 记录上一帧的显示开始时间
    auto lastFrameTime = std::chrono::high_resolution_clock::now();

	while (window.isOpen())
	{
		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>())
			{
				window.close();
			}
			else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
			{
				if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
					window.close();
			}
		}

		bool hasNewFrame = false;
		if (av_read_frame(formatContext, packet) >= 0) {
			if (packet->stream_index == videoStreamIndex) {
				int sendRet = avcodec_send_packet(codecContext, packet);
				if (sendRet >= 0) {
					int recvRet = avcodec_receive_frame(codecContext, frame);
					if (recvRet == 0) {  // Successfully decoded a frame
						sws_scale(swsContext, frame->data, frame->linesize, 0, height,
								  rgbFrame->data, rgbFrame->linesize);
						texture.update(rgbFrame->data[0]);
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
			break;  // Video playback finished
		}

		// Time synchronization control — only control display time when a new frame is decoded
		// 时间同步控制——只有解码出新帧，才控制显示时间
		if (hasNewFrame) {
			// Calculate the time difference from the previous frame to now
			// 计算从上一帧到现在的时间差
			auto currentTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> elapsed = currentTime - lastFrameTime;
			double elapsedMs = elapsed.count();

			// If the time difference is less than the frame interval, sleep to compensate
			// 如果时间差小于帧间隔，休眠补足
			if (elapsedMs < frameDelayMs) {
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(frameDelayMs - elapsedMs)));
			}

			// Update the previous frame time
			// 更新上一帧时间
			lastFrameTime = std::chrono::high_resolution_clock::now();
		}

		window.clear();
		window.draw(sprite);
		window.display();
	}

    av_free(buffer);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    return 0;
}