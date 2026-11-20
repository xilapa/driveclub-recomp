/* Head tracking (the launcher's Driving tab), Windows only. BB_HEAD_TRACKING=1 turns the cockpit (driver's head) view
   with the player's head, read through the TrackIR interface: NPClient64.dll in the folder named by
   HKCU\Software\NaturalPoint\NATURALPOINT\NPClient Location (Path), which TrackIR, opentrack and Beam Eye Tracker register.

   It is applied in the cockpit camera hook of runtime_horizon_lock.inc (+0xb92420), after the levelling, to the camera's
   rows there (left, up, forward, position; metres): the eye point moves with the head (x, y, z, along the camera's axes
   before the turn), then the view turns left/right (yaw) and up/down (pitch). Both are relative to the camera as the game
   left it, so the game's own look-around and head sway stay. Roll is not used: its sign was not measured.

   NP_GetData units (opentrack's NPClient): rotations 16383 = 180 degrees, translations 16383 = 500 mm. Signs measured
   with Beam Eye Tracker: yaw + = head turned left, pitch - = head up, x + = left, y + = up, z + = back. */
#ifdef _WIN32
#include <math.h>
#include <wchar.h>
typedef struct { short status, frame; unsigned cksum; float roll, pitch, yaw, tx, ty, tz, padding[9]; } head_tir_data;
typedef int (__stdcall *head_np_get_data)(head_tir_data *);
static head_np_get_data head_get_data;
/* Loads the TrackIR interface when BB_HEAD_TRACKING=1. Returns 1 when head tracking is on. */
static int head_tracking_install(void) {
    const char *on=getenv("BB_HEAD_TRACKING");
    if (!on || strcmp(on,"1")) return 0;
    wchar_t dir[MAX_PATH]; DWORD bytes=sizeof dir;
    if (RegGetValueW(HKEY_CURRENT_USER,L"Software\\NaturalPoint\\NATURALPOINT\\NPClient Location",L"Path",RRF_RT_REG_SZ,
                     NULL,dir,&bytes)!=ERROR_SUCCESS) {
        puts("Runtime: head tracking: no TrackIR interface is registered (start Beam Eye Tracker, opentrack or TrackIR first); off");
        return 0;
    }
    const size_t n=wcslen(dir);
    wchar_t path[MAX_PATH+16];
    swprintf(path,MAX_PATH+16,L"%ls%lsNPClient64.dll",dir,n && dir[n-1]!=L'\\' && dir[n-1]!=L'/' ? L"\\" : L"");
    HMODULE module=LoadLibraryW(path);
    if (!module) {
        printf("Runtime: head tracking: %ls could not be loaded (error %lu); off\n",path,GetLastError());
        return 0;
    }
    typedef int (__stdcall *np_window)(HWND);
    typedef int (__stdcall *np_u16)(unsigned short);
    typedef int (__stdcall *np_void)(void);
    const np_window register_window=(np_window)GetProcAddress(module,"NP_RegisterWindowHandle");
    const np_u16 request=(np_u16)GetProcAddress(module,"NP_RequestData");
    const np_u16 profile=(np_u16)GetProcAddress(module,"NP_RegisterProgramProfileID");
    const np_void start=(np_void)GetProcAddress(module,"NP_StartDataTransmission");
    head_get_data=(head_np_get_data)GetProcAddress(module,"NP_GetData");
    if (!register_window || !request || !profile || !start || !head_get_data) {
        printf("Runtime: head tracking: %ls is not a TrackIR interface; off\n",path);
        head_get_data=NULL;
        FreeLibrary(module);
        return 0;
    }
    /* shortcut: no game window exists yet, so none is registered; opentrack and Beam ignore it, NaturalPoint's own
       TrackIR software may want one (register the game window on the first frame if that is needed) */
    register_window(NULL);
    request(119);      /* what TrackIR games ask for: all six axes */
    profile(65000);    /* an ID opentrack does not know, so its data comes unencrypted */
    start();
    printf("Runtime: head tracking on: %ls\n",path);
    return 1;
}
/* The cockpit camera's rows (stride 4: left, up, forward, position) moved and turned with the head. */
static void head_tracking_apply(float *cam) {
    head_tir_data d;
    memset(&d,0,sizeof d);
    head_get_data(&d);
    const float rad=3.14159265f/16383.0f, metres=0.5f/16383.0f;
    const float yaw=d.yaw*rad, pitch=-d.pitch*rad;                     /* + left, + up */
    const float x=d.tx*metres, y=d.ty*metres, z=-d.tz*metres;          /* + left, + up, + forward */
    if (!isfinite(yaw) || !isfinite(pitch) || !isfinite(x) || !isfinite(y) || !isfinite(z)) return;
    static int logged;
    if (!logged && (yaw!=0.0f || pitch!=0.0f || x!=0.0f || y!=0.0f || z!=0.0f)) {
        logged=1;
        printf("Runtime: head tracking: first pose yaw %.1f, pitch %.1f degrees, x %.0f, y %.0f, z %.0f mm\n",
               (double)(yaw*57.29578f),(double)(pitch*57.29578f),(double)(x*1000.0f),(double)(y*1000.0f),(double)(z*1000.0f));
    }
    float *l=cam, *u=cam+4, *f=cam+8, *p=cam+12;
    for (int k=0;k<3;++k) p[k]+=l[k]*x+u[k]*y+f[k]*z;
    const float cy=cosf(yaw), sy=sinf(yaw), cp=cosf(pitch), sp=sinf(pitch);
    for (int k=0;k<3;++k) {
        const float f1=cy*f[k]+sy*l[k], l1=cy*l[k]-sy*f[k];
        const float f2=cp*f1+sp*u[k], u2=cp*u[k]-sp*f1;
        l[k]=l1; u[k]=u2; f[k]=f2;
    }
}
#endif
