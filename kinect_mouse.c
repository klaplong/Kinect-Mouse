/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "libfreenect.h"

#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>

#include <pthread.h>

#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <math.h>

#define SCREEN (DefaultScreen(display))

int depth;
char *display_name;

Display *display;
Window main_window;
Window root_window;

pthread_t freenect_thread;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

float tmprot = 1;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t gl_depth_front[640*480*4];
uint8_t gl_depth_back[640*480*4];

uint8_t gl_rgb_front[640*480*4];
uint8_t gl_rgb_back[640*480*4];

GLuint gl_depth_tex;
GLuint gl_rgb_tex;

freenect_context *f_ctx;
freenect_device *f_dev;

int freenect_angle = 17;
int freenect_led;

float mousex = 0, mousey = 0;
float tmousex = 0.0f, tmousey = 0.0f;
float prox_min_x = 40.0f;
float prox_min_y = 25.0f;
int last_px, last_py;
int steps = 8;
int screenw = 0, screenh = 0;
int snstvty;

int click_w = 280;
int gap = 30;
int point_w = 0;
int point_extr_h = 50;
int point_left = 0;
int point_extr_left = 0;
int point_right = 0;
int point_extr_right = 0;
int point_h = 0;
int point_extr_v = 20;
int point_top = 0;
int point_extr_top = 0;
int point_bottom = 0;
int point_extr_bottom = 0;

int block_mouse = 0;

int min_click = 500;
int click = 0;

int pause = 0;
int pusx = 0, pusy = 0;

int last_alert = 0;

pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_frames = 0;

void DrawGLScene() {
    pthread_mutex_lock(&gl_backbuf_mutex);

    while (got_frames < 2) {
        pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
    }

    memcpy(gl_depth_front, gl_depth_back, sizeof(gl_depth_back));
    memcpy(gl_rgb_front, gl_rgb_back, sizeof(gl_rgb_back));
    got_frames = 0;
    pthread_mutex_unlock(&gl_backbuf_mutex);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, gl_depth_front);

    glTranslated(640, 0, 0);
    glScalef(-1, 1, 1);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(255.0f, 255.0f, 255.0f, 255.0f);
    glTexCoord2f(0, 0); glVertex3f(0,0,0);
    glTexCoord2f(1, 0); glVertex3f(640,0,0);
    glTexCoord2f(1, 1); glVertex3f(640,480,0);
    glTexCoord2f(0, 1); glVertex3f(0,480,0);
    glEnd();

    glutSwapBuffers();
}

void calc_sizes() {
    point_w = 640 - click_w - gap - 2 * point_extr_h;
    point_extr_right = 0;
    point_right = point_extr_h;
    point_left = point_right + point_w;
    point_extr_left = point_left + point_extr_h;

    point_h = (point_w * screenh) / screenw;
    point_top = 240 - point_h / 2;
    point_extr_top = point_top - point_extr_v;
    point_bottom = 240 + point_h / 2;
    point_extr_bottom = point_bottom + point_extr_v;
}

void keyPressed(unsigned char key, int x, int y) {
    switch(key) {
        case 27:
            die = 1;
            pthread_join(freenect_thread, NULL);
            glutDestroyWindow(window);
            pthread_exit(NULL);
        break;
        case 'w':
            if (freenect_angle < 29) freenect_angle++;
            freenect_set_tilt_degs(f_dev,freenect_angle);
            printf("\nAngle: %d degrees\n", freenect_angle);
        break;
        case 's':
            freenect_angle = 0;
            freenect_set_tilt_degs(f_dev,freenect_angle);
            printf("\nAngle: %d degrees\n", freenect_angle);
        break;
        case 'x':
            if (freenect_angle > -30) freenect_angle--;
            freenect_set_tilt_degs(f_dev,freenect_angle);
            printf("\nAngle: %d degrees\n", freenect_angle);
        break;
        case '1':
            freenect_set_led(f_dev,LED_GREEN);
        break;
        case '2':
            freenect_set_led(f_dev,LED_RED);
        break;
        case '3':
            freenect_set_led(f_dev,LED_YELLOW);
        break;
        case '4':
            freenect_set_led(f_dev,LED_BLINK_GREEN);
        break;
        case '5':
            freenect_set_led(f_dev,LED_BLINK_GREEN);
        break;
        case '6':
            freenect_set_led(f_dev,LED_BLINK_RED_YELLOW);
        break;
        case '0':
            freenect_set_led(f_dev,LED_OFF);
        break;
        case 'o':
            tmprot+=0.1;
            printf("\n %f \n", tmprot);
        break;
        case 'p':
            tmprot-=0.1;
            printf("\n %f \n", tmprot);
        break;
        case 'r':
            block_mouse = 1;
            printf("New size: ");
            scanf("%dx%d\n", &screenw, &screenh);
            printf("Screen size set to %dx%d\n", screenw, screenh);
            screenw += 200;
            screenh += 200;
            calc_sizes();
            block_mouse = 0;
    }

}

void ReSizeGLScene(int Width, int Height) {
    glViewport(0,0,Width,Height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho (0, 640, 480, 0, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
}

void InitGL(int Width, int Height) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);
    glGenTextures(1, &gl_depth_tex);
    glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ReSizeGLScene(Width, Height);
}

void *gl_threadfunc(void *arg) {
    printf("GL thread\n");

    glutInit(&g_argc, g_argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
    glutInitWindowSize(640, 480);
    glutInitWindowPosition(0, 0);

    window = glutCreateWindow("Ooblik's Kinect Demo");

    glutDisplayFunc(&DrawGLScene);
    glutIdleFunc(&DrawGLScene);
    glutReshapeFunc(&ReSizeGLScene);
    glutKeyboardFunc(&keyPressed);

    InitGL(640, 480);

    glutMainLoop();

    return NULL;
}

uint16_t t_gamma[2048];

int in_click_area(int x, int y) {
    if (x > 640 - click_w) {
        return 1;
    }
    return 0;
}

int in_point_area(int x, int y) {
    if ((x < point_extr_left) && (y > point_extr_top) &&
        (y < point_extr_bottom)) {
        return 1;
    }
    return 0;
}

void draw_point(int x, int y, int r, int g, int b) {
    int index = 3 * (x + 640 * y);
    gl_depth_back[index + 0] = r;
    gl_depth_back[index + 1] = g;
    gl_depth_back[index + 2] = b;
}

void draw_line_v(int x, int from, int to) {
    int i;
    for (i = from; i < to; i ++) {
        draw_point(x, i, 0, 255, 0);
    }
}

void draw_line_h(int y, int from, int to) {
    int i;
    for (i = from; i < to; i ++) {
        draw_point(i, y, 0, 255, 0);
    }
}

void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp) {
    int i;
    int px = 0 , py = 0;
    int tx = 0 , ty = 0;
    int alert = 0;
    int n_in_click_area = 0;
    int n_in_point_area = 0;
    float total_x = 0;
    float total_y = 0;
    uint16_t *depth = v_depth;

    if (block_mouse) {
        return;
    }

    pthread_mutex_lock(&gl_backbuf_mutex);
    for (i=0; i<640*480; i++) {
        int pval = t_gamma[depth[i]];
        int lb = pval & 0xff;

        tx++;
        if(tx >= 640) {
            tx = 0;
            ty++;
        }
        /*case 0-5*/
        int close_enough = pval >> 8;

        if (close_enough == 0) {
            int this_in_click_area, this_in_point_area;

            draw_point(tx, ty, 255, 0, 0);

            this_in_click_area = in_click_area(tx, ty);
            if (this_in_click_area) {
                n_in_click_area ++;
            }

            this_in_point_area = in_point_area(tx, ty);
            if (this_in_point_area) {
                n_in_point_area ++;
                total_x += tx;
                total_y += ty;
            }

            alert++;
        }
        else if (close_enough == 1) {
            draw_point(tx, ty, 255, 255, 255);
        }
        else {
            draw_point(tx, ty, 0, 0, 0);
        }
    }

    /* Click right border */
    draw_line_v(640 - click_w, 0, 480);
    /* Point top */
    draw_line_h(point_top, 0, point_extr_left);
    /* Point extra top */
    draw_line_h(point_extr_top, 0, point_extr_left);
    /* Point bottom */
    draw_line_h(point_bottom, 0, point_extr_left);
    /* Point extra bottom */
    draw_line_h(point_extr_bottom, 0, point_extr_left);
    /* Point left */
    draw_line_v(point_left, point_extr_top, point_extr_bottom);
    /* Point extra left */
    draw_line_v(point_extr_left, point_extr_top, point_extr_bottom);
    /* Point right */
    draw_line_v(point_right, point_extr_top, point_extr_bottom);

    if (alert > snstvty) {
        printf("\n!!!TOO CLOSE!!!\n");
    }
    else {
        if (!alert) {
            px = last_px;
            py = last_py;
            alert = 1;
        }
        else {
            px = total_x / n_in_point_area;
            py = total_y / n_in_point_area;
        }

        if (alert) {
            mousex = ((point_w - px + point_extr_h) / (float)point_w) *
                     screenw;
            mousey = ((py - point_top) / (float)point_h) * screenh;

            if (n_in_click_area >= min_click) {
                if (!click) {
                    XTestFakeButtonEvent(display, 1, TRUE, CurrentTime);
                    click = 1;
                    printf("Click");
                }
            }
            else {
                if (click) {
                    XTestFakeButtonEvent(display, 1, FALSE, CurrentTime);
                    click = 0;
                    printf("ed!\n");
                }
            }

            /* Smooth move. */
            float prox_x = tmousex - mousex;
            prox_x *= prox_x;
            float prox_y = tmousey - mousey;
            prox_y *= prox_y;

            float speed_x, speed_y;

            if (prox_x > prox_min_x) {
                speed_x = -(tmousex - mousex) / (float)steps;
            }
            else {
                speed_x = -(tmousex - mousex) / (steps * 5.0f);
            }
            tmousex += speed_x;

            if (prox_y > prox_min_y) {
                speed_y = -(tmousey - mousey) / (float)steps;
            }
            else {
                speed_y = -(tmousey - mousey) / (steps * 5.0f);
            }
            tmousey += speed_y;

            XTestFakeMotionEvent(display, -1, tmousex-200, tmousey, CurrentTime);
            XSync(display, 0);
        }

        last_px = px;
        last_py = py;
    }


    got_frames++;
    pthread_cond_signal(&gl_frame_cond);
    pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp)
{
    pthread_mutex_lock(&gl_backbuf_mutex);
    got_frames++;
    memcpy(gl_rgb_back, rgb, 640*480*3);
    pthread_cond_signal(&gl_frame_cond);
    pthread_mutex_unlock(&gl_backbuf_mutex);
}

void *freenect_threadfunc(void *arg)
{
    freenect_set_tilt_degs(f_dev,freenect_angle);
    freenect_set_led(f_dev,LED_GREEN);
    freenect_set_depth_callback(f_dev, depth_cb);
    freenect_set_video_callback(f_dev, rgb_cb);

    freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
    freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));


    freenect_start_depth(f_dev);
    freenect_start_video(f_dev);

    printf("'W'-Tilt Up, 'S'-Level, 'X'-Tilt Down, '0'-'6'-LED Mode\n");

    while(!die && freenect_process_events(f_ctx) >= 0 )
    {
        freenect_raw_tilt_state* state;
        freenect_update_tilt_state(f_dev);
        state = freenect_get_tilt_state(f_dev);;
        double dx,dy,dz;
        freenect_get_mks_accel(state, &dx, &dy, &dz);
        fflush(stdout);
    }

    printf("\nShutting Down Streams...\n");

    freenect_stop_depth(f_dev);
    freenect_stop_video(f_dev);

    freenect_close_device(f_dev);
    freenect_shutdown(f_ctx);

    printf("-- done!\n");
    return NULL;
}


int main(int argc, char **argv)
{
    int res;

    printf("\n===Kinect Mouse===\n");

    if (argc == 2) {
        snstvty = atoi(argv[1]);
    } else {
        snstvty = 20000;
    }

    mousemask(ALL_MOUSE_EVENTS, NULL);

    display = XOpenDisplay(0);

    root_window = DefaultRootWindow(display);

    screenw = XDisplayWidth(display, SCREEN);
    screenh = XDisplayHeight(display, SCREEN);

    printf("\nDefault Display Found\n");
    printf("\nSize: %dx%d\n", screenw, screenh);

    screenw += 200;
    screenh += 200;

    calc_sizes();

    tmousex = screenw / 2.0f;
    tmousey = screenh / 2.0f;

    prox_min_x *= prox_min_x;
    prox_min_y *= prox_min_y;

    int i;
    for (i=0; i<2048; i++) {
        float v = i/2048.0;
        v = powf(v, 3)* 6;
        t_gamma[i] = v*6*256;
    }

    g_argc = argc;
    g_argv = argv;

    if (freenect_init(&f_ctx, NULL) < 0) {
        printf("freenect_init() failed\n");
        return 1;
    }

    freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);

    int nr_devices = freenect_num_devices (f_ctx);
    printf ("\nNumber of Devices Found: %d\n", nr_devices);

    int user_device_number = 0;
    if (argc > 1)
        user_device_number = atoi(argv[1]);

    if (nr_devices < 1)
        return 1;

    if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
        printf("\nCOULD NOT LOCATE KINECT :(\n");
        return 1;
    }

    res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
    if (res) {
        printf("Could Not Create Thread\n");
        return 1;
    }

    gl_threadfunc(NULL);

    return 0;
}
