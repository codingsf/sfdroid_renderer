/*
 *  this file is part of sfdroid
 *  Copyright (C) 2015, Franz-Josef Haider <f_haider@gmx.at>
 *  based on harmattandroid by Thomas Perl
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "renderer.h"

#include <iostream>

#include <GLES/gl.h>

#include <SDL_syswm.h>
#include <wayland-egl.h>

using namespace std;

int renderer_t::init()
{
    int err = 0;
    SDL_SysWMinfo info;
    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };
    EGLConfig egl_cfg;
    EGLint contextParams[] = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE};
    EGLint numConfigs;

#if DEBUG
    cout << "initializing SDL" << endl;
#endif
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        cerr << "SDL_Init failed" << endl;
        err = 1;
        goto quit;
    }

#if DEBUG
    cout << "creating SDL window" << endl;
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    window = SDL_CreateWindow("sfdroid", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if(window == NULL)
    {
        cerr << "failed to create SDL window" << endl;
        err = 2;
        goto quit;
    }

    SDL_GetWindowSize(window, &win_width, &win_height);

#if DEBUG
    cout << "window width: " << win_width << " height: " << win_height << endl;
#endif

    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

#if DEBUG
    cout << "getting egl display" << endl;
#endif
    egl_dpy = eglGetDisplay((EGLNativeDisplayType)info.info.wl.display);
    if(egl_dpy == EGL_NO_DISPLAY)
    {
        cerr << "failed to get egl display" << endl;
        err = 5;
        goto quit;
    }

#if DEBUG
    cout << "initializing egl display" << endl;
#endif
    if(!eglInitialize(egl_dpy, NULL, NULL))
    {
        cerr << "failed to initialize egl display" << endl;
        err = 6;
        goto quit;
    }

#if DEBUG
    cout << "choosing egl config" << endl;
#endif
    if(eglChooseConfig(egl_dpy, configAttribs, &egl_cfg, 1, &numConfigs) != EGL_TRUE || numConfigs == 0)
    {
        cerr << "unable to find an EGL Config" << endl;
        err = 7;
        goto quit;
    }

#if DEBUG
    cout << "creating wl egl window" << endl;
#endif
    w_egl_window = wl_egl_window_create (info.info.wl.surface, win_width, win_height);

#if DEBUG
    cout << "creating egl window surface" << endl;
#endif
    egl_surf = eglCreateWindowSurface(egl_dpy, egl_cfg, (EGLNativeWindowType)w_egl_window, 0);
    if(egl_surf == EGL_NO_SURFACE)
    {
        cerr << "unable to create an EGLSurface" << endl;
        err = 8;
        goto quit;
    }

#if DEBUG
    cout << "creating GLES context" << endl;
#endif
    eglBindAPI(EGL_OPENGL_ES_API);
    egl_ctx = eglCreateContext(egl_dpy, egl_cfg, NULL, contextParams);
    if(egl_ctx == EGL_NO_CONTEXT)
    {
        cerr << "unable to create GLES context" << endl;
        err = 9;
        goto quit;
    }

#if DEBUG
    cout << "making GLES context current" << endl;
#endif
    if(eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx) == EGL_FALSE)
    {
        cerr << "unable to make GLES context current" << endl;
        err = 10;
        goto quit;
    }

#if DEBUG
    cout << "getting eglCreateImageKHR" << endl;
#endif
    pfn_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if(pfn_eglCreateImageKHR == NULL)
    {
        cerr << "eglCreateImageKHR not found" << endl;
        err = 11;
        goto quit;
    }

#if DEBUG
    cout << "getting glEGLImageTargetTexture2DOES" << endl;
#endif
    pfn_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if(pfn_glEGLImageTargetTexture2DOES == NULL)
    {
        cerr << "glEGLImageTargetTexture2DOES not found" << endl;
        err = 12;
        goto quit;
    }

#if DEBUG
    cout << "getting eglDestroyImageKHR" << endl;
#endif
    pfn_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if(pfn_eglDestroyImageKHR == NULL)
    {
        cerr << "eglDestroyImageKHR not found" << endl;
        err = 13;
        goto quit;
    }

#if DEBUG
    cout << "getting eglHybrisWaylandPostBuffer" << endl;
#endif
    pfn_eglHybrisWaylandPostBuffer = (int (*)(EGLNativeWindowType, void *))eglGetProcAddress("eglHybrisWaylandPostBuffer");
    if(pfn_eglHybrisWaylandPostBuffer == NULL)
    {
        cerr << "eglHybrisWaylandPostBuffer not found" << endl;
        err = 15;
        goto quit;
    }

#if DEBUG
    cout << "setting up gl" << endl;
#endif
    glViewport(0, 0, win_width, win_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, win_width, win_height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.f, 1.f, 1.f, 1.f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glGenTextures(1, &dummy_tex);
    glBindTexture(GL_TEXTURE_2D, dummy_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

quit:
    return err;
}

void renderer_t::deinit()
{
    glDeleteTextures(1, &dummy_tex);
    eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_dpy, egl_surf);
    wl_egl_window_destroy(w_egl_window);
    eglDestroyContext(egl_dpy, egl_ctx);
    eglTerminate(egl_dpy);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int renderer_t::render_buffer(ANativeWindowBuffer *buffer, buffer_info_t &info)
{
    pfn_eglHybrisWaylandPostBuffer((EGLNativeWindowType)w_egl_window, buffer);
    if(eglGetError() != EGL_SUCCESS)
    {
        return 1;
    }

    return 0;
}

int renderer_t::get_height()
{
    return win_height;
}

int renderer_t::get_width()
{
    return win_width;
}

