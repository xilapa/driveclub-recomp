/* src/runtime_head_tracking.inc: the head pose turns and moves the cockpit camera's rows the right way.
   cc -O2 -Isrc tools/head_tracking_test.c -lm -o out/head_tracking_test && out/head_tracking_test */
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/runtime_head_tracking.inc"
static head_tir_data pose;
static int __stdcall fake_get_data(head_tir_data *d) { *d=pose; return 0; }
/* left (+x), up (+y), forward (+z), at the origin */
static void identity(float *cam) {
    memset(cam,0,16*sizeof *cam);
    cam[0]=1; cam[5]=1; cam[10]=1;
}
static int same(const float *v, float x, float y, float z) {
    return fabsf(v[0]-x)<1e-4f && fabsf(v[1]-y)<1e-4f && fabsf(v[2]-z)<1e-4f;
}
int main(void) {
    head_get_data=fake_get_data;
    float cam[16];
    /* head turned 90 degrees left (yaw +): forward becomes the old left, left becomes the old back */
    memset(&pose,0,sizeof pose); pose.yaw=16383.0f/2; identity(cam); head_tracking_apply(cam);
    assert(same(cam+8,1,0,0) && same(cam,0,0,-1) && same(cam+4,0,1,0));
    /* head 90 degrees up (pitch -): forward becomes the old up, up becomes the old back */
    memset(&pose,0,sizeof pose); pose.pitch=-16383.0f/2; identity(cam); head_tracking_apply(cam);
    assert(same(cam+8,0,1,0) && same(cam+4,0,0,-1) && same(cam,1,0,0));
    /* 500 mm left (x +), up (y +) and back (z +): the eye point moves left, up and back by 0.5 m */
    memset(&pose,0,sizeof pose); pose.tx=16383.0f; pose.ty=16383.0f; pose.tz=16383.0f; identity(cam); head_tracking_apply(cam);
    assert(same(cam+12,0.5f,0.5f,-0.5f) && same(cam+8,0,0,1));
    /* any turn keeps the axes unit length and at right angles */
    memset(&pose,0,sizeof pose); pose.yaw=3000; pose.pitch=-2000; identity(cam); head_tracking_apply(cam);
    const float *l=cam, *u=cam+4, *f=cam+8;
    assert(fabsf(l[0]*u[0]+l[1]*u[1]+l[2]*u[2])<1e-5f && fabsf(l[0]*f[0]+l[1]*f[1]+l[2]*f[2])<1e-5f &&
           fabsf(u[0]*f[0]+u[1]*f[1]+u[2]*f[2])<1e-5f && fabsf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]-1)<1e-5f);
    /* no pose (tracker idle): the camera stays as the game left it */
    memset(&pose,0,sizeof pose); identity(cam); head_tracking_apply(cam);
    assert(same(cam,1,0,0) && same(cam+4,0,1,0) && same(cam+8,0,0,1) && same(cam+12,0,0,0));
    puts("head tracking test: ok");
    return 0;
}
