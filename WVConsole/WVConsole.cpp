// WVConsole.cpp : Defines the entry point for the console application.
//

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
};

#include <windows.h>

#include <winuser.h>

#include <shellapi.h>
#include <iostream>
#include <gdiplus.h>
#include <memory>

#pragma comment(lib, "Gdiplus.lib")

using namespace std;
using namespace Gdiplus;

HWND workerw;
WNDPROC prevProc;

AVFormatContext *pFormatCtx = NULL;
int             i, videoStream;
AVCodecContext  *pCodecCtx = NULL;
AVCodec         *pCodec = NULL;
AVFrame         *pFrame = NULL;
AVFrame			*rgbFrame = NULL;
AVPacket        packet;
int             frameFinished;
//float           aspect_ratio;

static int midWidth;

static int screenHeight;

AVDictionary    *optionsDict = NULL;
struct SwsContext *sws_ctx = NULL;

GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

BOOL CALLBACK EnumWindowsCallback(HWND tophandle, LPARAM topparamhandle);
void drawWallVideo();
int initFfmpeg();

int main()
{
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	auto progman = FindWindow(L"Progman", NULL);
	ULONG result;
	SendMessageTimeout(progman, 0x052C, NULL, NULL, SMTO_NORMAL, 1000, &result);

	EnumWindows(EnumWindowsCallback, NULL);

	if (initFfmpeg() < 0) {
		return -1;
	}

	while (true) {
		drawWallVideo();
		Sleep(5);
	}

    return 0;
}

int initFfmpeg() {
	av_register_all();


	// Open video file
	if (avformat_open_input(&pFormatCtx, "D:\\x.mp4", NULL, NULL) != 0)
		return -1; // Couldn't open file

				   // Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information

	// Find the first video stream
	videoStream = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -1; // Didn't find a video stream

				   // Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec

				   // Allocate video frame
	pFrame = av_frame_alloc();
	rgbFrame = av_frame_alloc();

	auto numBytes = avpicture_get_size(PIX_FMT_RGB24, midWidth,
		screenHeight);

	auto buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)rgbFrame, buffer, PIX_FMT_RGB24,
		midWidth, screenHeight);

	sws_ctx =
		sws_getContext
		(
			pCodecCtx->width,
			pCodecCtx->height,
			pCodecCtx->pix_fmt,
			midWidth,
			screenHeight,
			PIX_FMT_BGR24,
			SWS_BILINEAR,
			NULL,
			NULL,
			NULL
		);

	return 0;

}

BOOL CALLBACK EnumWindowsCallback(HWND tophandle, LPARAM topparamhandle)
{
	auto p = FindWindowEx(tophandle, NULL, L"SHELLDLL_DefView", NULL);
	if (p) {
		workerw = FindWindowEx(NULL,
			tophandle,
			L"WorkerW",
			NULL);
		cout << "found workerw";

		RECT r;
		GetWindowRect(workerw, &r);
		screenHeight = r.bottom - r.top;
		midWidth = (r.right - r.left) / 4 * 4;
	}
	return true;
}

void drawWallVideo() {
	unique_ptr<Bitmap> bmp;

	auto ret = av_read_frame(pFormatCtx, &packet);
	if (ret >= 0) {
		if (packet.stream_index == videoStream) {
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			if (frameFinished) {
				sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, rgbFrame->data, rgbFrame->linesize);
				cout << "last error before bitmap: " << GetLastError() << endl;
				bmp = make_unique<Bitmap>(midWidth, screenHeight, midWidth * 3, PixelFormat24bppRGB, rgbFrame->data[0]);
				cout << "last error after bitmap: " << GetLastError() << endl;
				cout << "bitmap status: " << bmp->GetLastStatus();
			}
		}
	}
	else {
		av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_ANY);
	}

	av_free_packet(&packet);


	auto dc = GetDCEx(workerw, NULL, 0x403);
	if (dc) {
		Graphics grahics(dc);

		if (bmp.get()) {
			grahics.DrawImage(bmp.get(), 0, 0);
		}

		// Fill the rectangle.
		//grahics.FillRectangle(&blackBrush, 0, 0, width, height);
	}
	ReleaseDC(workerw, dc);
}