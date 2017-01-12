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

#include <GL/glew.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

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


const GLchar* SHADER = R"(
uniform vec2 center;
uniform vec2 resolution;
uniform float time;

void main(void)
{
    vec2 p = 2.0 * (gl_FragCoord.xy - center.xy) / resolution.xy;
	p.x *= resolution.x/resolution.y;

	float zoo = .62+.38*sin(.1*time);
	float coa = cos( 0.1*(1.0-zoo)*time );
	float sia = sin( 0.1*(1.0-zoo)*time );
	zoo = pow( zoo,8.0);
	vec2 xy = vec2( p.x*coa-p.y*sia, p.x*sia+p.y*coa);
	vec2 cc = vec2(-.745,.186) + xy*zoo;

	vec2 z  = vec2(0.0);
	vec2 z2 = z*z;
	float m2;
	float co = 0.0;

	for( int i=0; i<256; i++ )
	{
		z = cc + vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y );
		m2 = dot(z,z);
		if( m2>1024.0 ) break;
		co += 1.0;
	}
	co = co + 1.0 - log2(.5*log2(m2));

	co = sqrt(co/256.0);
	gl_FragColor = vec4( .5+.5*cos(6.2831*co+0.0),
						.5+.5*cos(6.2831*co+0.4),
						.5+.5*cos(6.2831*co+0.7),
						1.0 );
}
)";




HWND hwnd;
HDC hdc;
float angle;
HGLRC hrc;
unsigned int tex;
GLuint Program;
static int midWidth;

static int screenHeight;

AVDictionary    *optionsDict = NULL;
struct SwsContext *sws_ctx = NULL;

GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

BOOL CALLBACK EnumWindowsCallback(HWND tophandle, LPARAM topparamhandle);

void initShader();

void resize()
{
	RECT rec;
	GetClientRect(hwnd, &rec);
	float width = 400;
	float height = 400;
	GLfloat fieldOfView = 60.0f;
	glViewport(0, 0, rec.right, rec.bottom);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fieldOfView, (GLfloat)width / (GLfloat)height, 0.1, 500.0);


	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_TEXTURE_2D);
	glLoadIdentity();
}

void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC);

void init()
{
	GLubyte pixels[12] = {
		0, 0, 0,   1, 1, 1,
		1, 1, 1,   0, 0, 0
	};
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

void draw()
{
	angle -= 1.f;
	GLint timeLocation = glGetUniformLocation(Program, "time");
	GLint resolutionLocation = glGetUniformLocation(Program, "resolution");
	GLint centerLocation = glGetUniformLocation(Program, "center");
	

	float rtri = 0;
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(Program);

	glUniform1f(timeLocation, angle);
	glUniform2f(resolutionLocation, angle, angle);
	glUniform2f(centerLocation, 0.f, 0.f);
	



	glTranslatef(0, 0, -5);
	glRotatef(angle, 0, 1, 0);
	glBindTexture(GL_TEXTURE_2D, tex);

	glBegin(GL_QUADS);
	glColor3f(0, 1, 0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(1.0, 0.0, 0.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(1.0, 1.0, 0.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(0.0, 1.0, 0.0);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	auto res = SwapBuffers(hdc);
	res = GetLastError();
	if (res != Status::Ok) {
		cout << "Error swapping buffers: " << res;
	}
}

int main()
{
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	auto progman = FindWindow(L"Progman", NULL);
	ULONG_PTR result;
	SendMessageTimeout(progman, 0x052C, NULL, NULL, SMTO_NORMAL, 1000, &result);

	EnumWindows(EnumWindowsCallback, NULL);

	hwnd = workerw;
	hdc = GetDCEx(workerw, NULL, 0x403);

	EnableOpenGL(hwnd, &hdc, &hrc);
	auto res = glewInit();

	init();
	resize();
	initShader();
	cout<< hrc;

	while (true) {
		draw();
		Sleep(2);
	}

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

void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
	PIXELFORMATDESCRIPTOR pfd;

	int iFormat;

	*hDC = GetDC(hwnd);

	ZeroMemory(&pfd, sizeof(pfd));

	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;

	iFormat = ChoosePixelFormat(*hDC, &pfd);

	SetPixelFormat(*hDC, iFormat, &pfd);

	*hRC = wglCreateContext(*hDC);

	wglMakeCurrent(*hDC, *hRC);
}

void initShader() {
	GLuint fragment;
	GLint success;

	char initLog[512]{};
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &SHADER, NULL);
	glCompileShader(fragment);
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);

	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, initLog);
		std::cout << "Error::SHADER::FRAGMENT::COMPILATION_FAILED\n" << initLog << std::endl;
	}

	Program = glCreateProgram();
	glAttachShader(Program, fragment);
	glLinkProgram(Program);

	glGetProgramiv(Program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(Program, 512, NULL, initLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << initLog << std::endl;
	}

	glDeleteShader(fragment);
}