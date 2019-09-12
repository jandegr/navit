#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>
#include <gdiplus.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "config.h"
#include "item.h"
#include "callback.h"
#include "color.h"
#include "debug.h"
#include "point.h"
#include "graphics.h"
#include "plugin.h"
#include "window.h"
#include "xpm2bmp.h"
}

#include <string>
#include "graphics_win32.h"

#include "support/win32/ConvertUTF.h"
#include "profile.h"
#include "keys.h"



#ifndef STRETCH_HALFTONE
#define STRETCH_HALFTONE 4
#endif

typedef BOOL (WINAPI *FP_AlphaBlend) ( HDC hdcDest,
                                       int nXOriginDest,
                                       int nYOriginDest,
                                       int nWidthDest,
                                       int nHeightDest,
                                       HDC hdcSrc,
                                       int nXOriginSrc,
                                       int nYOriginSrc,
                                       int nWidthSrc,
                                       int nHeightSrc,
                                       BLENDFUNCTION blendFunction
                                     );

typedef int (WINAPI *FP_SetStretchBltMode) (HDC dc,int mode);

struct graphics_priv
{
    struct navit *nav;
    struct window window;
    struct point p;
    int x;
    int y;
    int width;
    int height;
    int frame;
    int disabled;
    HANDLE wnd_parent_handle;
    HANDLE wnd_handle;
    COLORREF bg_color;
    struct callback_list *cbl;
    enum draw_mode_num mode;
    struct graphics_priv* parent;
    struct graphics_priv *overlays;
    struct graphics_priv *next;
    struct color transparent_color;
    DWORD* pPixelData;
    HDC hMemDC;
    HDC hPrebuildDC;
    HBITMAP hBitmap;
    HBITMAP hPrebuildBitmap;
    HBITMAP hOldBitmap;
    HBITMAP hOldPrebuildBitmap;
    FP_AlphaBlend AlphaBlend;
    FP_SetStretchBltMode SetStretchBltMode;
    BOOL (WINAPI *ChangeWindowMessageFilter)(UINT message, DWORD dwFlag);
    BOOL (WINAPI *ChangeWindowMessageFilterEx)( HWND hWnd, UINT message, DWORD action, void *pChangeFilterStruct);
    HANDLE hCoreDll;
    HANDLE hUser32Dll;
    HANDLE hGdi32Dll;
    HANDLE hGdiplusDll;
    GHashTable *image_cache_hash;
};

struct window_priv
{
    HANDLE hBackLight;
};

static HWND g_hwnd = NULL;


#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif


HFONT EzCreateFont (HDC hdc, TCHAR * szFaceName, int iDeciPtHeight,
                    int iDeciPtWidth, int iAttributes, BOOL fLogRes) ;

#define EZ_ATTR_BOLD          1
#define EZ_ATTR_ITALIC        2
#define EZ_ATTR_UNDERLINE     4
#define EZ_ATTR_STRIKEOUT     8

HFONT EzCreateFont (HDC hdc, TCHAR * szFaceName, int iDeciPtHeight,
                    int iDeciPtWidth, int iAttributes, BOOL fLogRes)
{
    FLOAT      cxDpi, cyDpi ;
    HFONT      hFont ;
    LOGFONT    lf ;
    POINT      pt ;
    TEXTMETRIC tm ;

    SaveDC (hdc) ;

    SetGraphicsMode (hdc, GM_ADVANCED) ;
    ModifyWorldTransform (hdc, NULL, MWT_IDENTITY) ;
    SetViewportOrgEx (hdc, 0, 0, NULL) ;
    SetWindowOrgEx   (hdc, 0, 0, NULL) ;

    if (fLogRes)
    {
        cxDpi = (FLOAT) GetDeviceCaps (hdc, LOGPIXELSX) ;
        cyDpi = (FLOAT) GetDeviceCaps (hdc, LOGPIXELSY) ;
    }
    else
    {
        cxDpi = (FLOAT) (25.4 * GetDeviceCaps (hdc, HORZRES) /
                         GetDeviceCaps (hdc, HORZSIZE)) ;

        cyDpi = (FLOAT) (25.4 * GetDeviceCaps (hdc, VERTRES) /
                         GetDeviceCaps (hdc, VERTSIZE)) ;
    }

    pt.x = (int) (iDeciPtWidth  * cxDpi / 72) ;
    pt.y = (int) (iDeciPtHeight * cyDpi / 72) ;

    lf.lfHeight         = - (int) (fabs (pt.y) / 10.0 + 0.5) ;
    lf.lfWidth          = 0 ;
    lf.lfEscapement     = 0 ;
    lf.lfOrientation    = 0 ;
    lf.lfWeight         = iAttributes & EZ_ATTR_BOLD      ? 700 : 0 ;
    lf.lfItalic         = iAttributes & EZ_ATTR_ITALIC    ?   1 : 0 ;
    lf.lfUnderline      = iAttributes & EZ_ATTR_UNDERLINE ?   1 : 0 ;
    lf.lfStrikeOut      = iAttributes & EZ_ATTR_STRIKEOUT ?   1 : 0 ;
    lf.lfCharSet        = DEFAULT_CHARSET ;
    lf.lfOutPrecision   = 0 ;
    lf.lfClipPrecision  = 0 ;
    lf.lfQuality        = 0 ;
    lf.lfPitchAndFamily = 0 ;

    lstrcpy (lf.lfFaceName, szFaceName) ;

    hFont = CreateFontIndirect (&lf) ;

    if (iDeciPtWidth != 0)
    {
        hFont = (HFONT) SelectObject (hdc, hFont) ;

        GetTextMetrics (hdc, &tm) ;

        DeleteObject (SelectObject (hdc, hFont)) ;

        lf.lfWidth = (int) (tm.tmAveCharWidth *
                            fabs (pt.x) / fabs (pt.y) + 0.5) ;

        hFont = CreateFontIndirect (&lf) ;
    }

    RestoreDC (hdc, -1) ;
    return hFont ;
}

struct graphics_image_priv
{
    PXPM2BMP pxpm;
    int width,height,row_bytes,channels;
    unsigned char *png_pixels;
    Gdiplus::Image *gdiplusimage;
    HBITMAP hBitmap;
    struct point hot;
};


static void ErrorExit(LPTSTR lpszFunction)
{
    // Retrieve the system error message for the last-error code
    dbg(lvl_error,"enter\n");
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
                                      (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+40)*sizeof(TCHAR));
    _tprintf((LPTSTR)lpDisplayBuf, TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);

    dbg(lvl_error, "%s failed with error %d: %s", lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw);
}



struct graphics_gc_priv
{
    HWND        hwnd;
    int         line_width;
    COLORREF    fg_color;
    int         fg_alpha;
    int         bg_alpha;
    COLORREF    bg_color;
    int     dashed;
    HPEN hpen;
    HBRUSH hbrush;
    struct graphics_priv *gr;
};


static void create_memory_dc(struct graphics_priv *gr)
{    
    dbg(lvl_error,"enter\n");
    HDC hdc;
    BITMAPINFO bOverlayInfo;

    if (gr->hMemDC)
    {
        (void)SelectBitmap(gr->hMemDC, gr->hOldBitmap);
        DeleteBitmap(gr->hBitmap);
        DeleteDC(gr->hMemDC);

        (void)SelectBitmap(gr->hPrebuildDC, gr->hOldPrebuildBitmap);
        DeleteBitmap(gr->hPrebuildBitmap);
        DeleteDC(gr->hPrebuildDC);
        gr->hPrebuildDC = 0;
    }


    hdc = GetDC( (HWND)gr->wnd_handle );
    // Creates memory DC
    gr->hMemDC = CreateCompatibleDC(hdc);
    dbg(lvl_debug, "resize memDC to: %d %d \n", gr->width, gr->height );


#ifndef  FAST_TRANSPARENCY

    memset(&bOverlayInfo, 0, sizeof(BITMAPINFO));
    bOverlayInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bOverlayInfo.bmiHeader.biWidth = gr->width;
    bOverlayInfo.bmiHeader.biHeight = -gr->height;
    bOverlayInfo.bmiHeader.biBitCount = 32;
    bOverlayInfo.bmiHeader.biCompression = BI_RGB;
    bOverlayInfo.bmiHeader.biPlanes = 1;
    gr->hPrebuildDC = CreateCompatibleDC(NULL);
    gr->hPrebuildBitmap = CreateDIBSection(gr->hMemDC, &bOverlayInfo, DIB_RGB_COLORS , (void **)&gr->pPixelData, NULL, 0);
    gr->hOldPrebuildBitmap = SelectBitmap(gr->hPrebuildDC, gr->hPrebuildBitmap);

#endif
    gr->hBitmap = CreateCompatibleBitmap(hdc, gr->width, gr->height );

    if ( gr->hBitmap )
    {
        gr->hOldBitmap = SelectBitmap( gr->hMemDC, gr->hBitmap);
    }
    ReleaseDC( (HWND)gr->wnd_handle, hdc );
}

static void HandleButtonClick( struct graphics_priv *gra_priv, int updown, int button, long lParam )
{
    struct point pt = { LOWORD(lParam), HIWORD(lParam) };
    callback_list_call_attr_3(gra_priv->cbl, attr_button, (void *)updown, (void *)button, (void *)&pt);
}

static void HandleKeyChar(struct graphics_priv *gra_priv, WPARAM wParam)
{
    TCHAR key = (TCHAR) wParam;
    char *s=NULL;
    char k[]={0,0};
    dbg(lvl_debug,"HandleKey %d\n",key);
    switch (key) {
    default:
        k[0]=key;
        s=k;
        break;
    }
    if (s) 
        callback_list_call_attr_1(gra_priv->cbl, attr_keypress, (void *)s);
}

static void HandleKeyDown(struct graphics_priv *gra_priv, WPARAM wParam)
{
    int key = (int) wParam;
    char up[]={NAVIT_KEY_UP,0};
    char down[]={NAVIT_KEY_DOWN,0};
    char left[]={NAVIT_KEY_LEFT,0};
    char right[]={NAVIT_KEY_RIGHT,0};
    char *s=NULL;
    dbg(lvl_debug,"HandleKey %d\n",key);
    switch (key) {
    case 37:
        s=left;
        break;
    case 38:
        s=up;
        break;
    case 39:
        s=right;
        break;
    case 40:
        s=down;
        break;
    }
    if (s) 
        callback_list_call_attr_1(gra_priv->cbl, attr_keypress, (void *)s);
}


static LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    struct graphics_priv* gra_priv = (struct graphics_priv*)GetWindowLongPtr( hwnd , DWLP_USER );

    switch (Message)
    {
    case WM_CREATE:
    {
        if ( gra_priv )
        {
            RECT rc ;

            GetClientRect( hwnd, &rc );
            gra_priv->width = rc.right;
            gra_priv->height = rc.bottom;
            create_memory_dc(gra_priv);
        }
    }
    break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case WM_USER + 1:
            break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_USER+1:
        if ( gra_priv )
        {
            RECT rc ;

            GetClientRect( hwnd, &rc );
            gra_priv->width = rc.right;
            gra_priv->height = rc.bottom;

            create_memory_dc(gra_priv);
            callback_list_call_attr_2(gra_priv->cbl, attr_resize, (void *)gra_priv->width, (void *)gra_priv->height);
        }
        break;
    case WM_USER+2:
    {
        struct callback_list *cbl = (struct callback_list*)wParam;
        callback_list_call_0(cbl);
    }
    break;

    case WM_SIZE:
        if ( gra_priv )
        {
            gra_priv->width = LOWORD( lParam );
            gra_priv->height  = HIWORD( lParam );
            create_memory_dc(gra_priv);
            dbg(lvl_debug, "resize gfx to: %d %d \n", gra_priv->width, gra_priv->height );
            callback_list_call_attr_2(gra_priv->cbl, attr_resize, (void *)gra_priv->width, (void *)gra_priv->height);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        if ( gra_priv && gra_priv->hMemDC)
        {
            struct graphics_priv* overlay;
            PAINTSTRUCT ps = { 0 };
            HDC hdc;
            profile(0, NULL);
            dbg(lvl_debug, "WM_PAINT\n");
            overlay = gra_priv->overlays;

#ifndef FAST_TRANSPARENCY
            BitBlt( gra_priv->hPrebuildDC, 0, 0, gra_priv->width , gra_priv->height, gra_priv->hMemDC, 0, 0, SRCCOPY);
#endif
            while ( !gra_priv->disabled && overlay)
            {
                if ( !overlay->disabled && overlay->p.x >= 0 &&
                     overlay->p.y >= 0 &&
                     overlay->p.x < gra_priv->width &&
                     overlay->p.y < gra_priv->height )
                {
                    int x,y;
                    int destPixel, srcPixel;
                    int h,w;

                    const COLORREF transparent_color = RGB(overlay->transparent_color.r >> 8,overlay->transparent_color.g >> 8,overlay->transparent_color.b >> 8);

                    BitBlt( overlay->hPrebuildDC, 0, 0, overlay->width , overlay->height, overlay->hMemDC, 0, 0, SRCCOPY);

                    h=overlay->height;
                    w=overlay->width;
                    if (w > gra_priv->width-overlay->p.x)
                        w=gra_priv->width-overlay->p.x;
                    if (h > gra_priv->height-overlay->p.y)
                        h=gra_priv->height-overlay->p.y;
                    for ( y = 0; y < h ;y++ )
                    {
                        for ( x = 0; x < w; x++ )
                        {
                            srcPixel = y*overlay->width+x;
                            destPixel = ((overlay->p.y + y) * gra_priv->width) + (overlay->p.x + x);
                            if ( overlay->pPixelData[srcPixel] == transparent_color )
                            {
                                destPixel = ((overlay->p.y + y) * gra_priv->width) + (overlay->p.x + x);

                                gra_priv->pPixelData[destPixel] = RGB ( ((65535 - overlay->transparent_color.a) * GetRValue(gra_priv->pPixelData[destPixel]) + overlay->transparent_color.a * GetRValue(overlay->pPixelData[srcPixel])) / 65535,
                                                                ((65535 - overlay->transparent_color.a) * GetGValue(gra_priv->pPixelData[destPixel]) + overlay->transparent_color.a * GetGValue(overlay->pPixelData[srcPixel])) / 65535,
                                                                ((65535 - overlay->transparent_color.a) * GetBValue(gra_priv->pPixelData[destPixel]) + overlay->transparent_color.a * GetBValue(overlay->pPixelData[srcPixel])) / 65535);
                            }
                            else
                            {
                                gra_priv->pPixelData[destPixel] = overlay->pPixelData[srcPixel];
                            }
                        }
                    }
                }
                overlay = overlay->next;
            }

            hdc = BeginPaint(hwnd, &ps);
            BitBlt( hdc, 0, 0, gra_priv->width , gra_priv->height, gra_priv->hPrebuildDC, 0, 0, SRCCOPY );
            EndPaint(hwnd, &ps);
            profile(0, "WM_PAINT\n");
        }
        break;
    case WM_MOUSEMOVE:
    {
        struct point pt = { LOWORD(lParam), HIWORD(lParam) };
        callback_list_call_attr_1(gra_priv->cbl, attr_motion, (void *)&pt);
    }
    break;

    case WM_LBUTTONDOWN:
    {
        dbg(lvl_debug, "LBUTTONDOWN\n");
        HandleButtonClick( gra_priv, 1, 1, lParam);
    }
    break;
    case WM_LBUTTONUP:
    {
        dbg(lvl_debug, "LBUTTONUP\n");
        HandleButtonClick( gra_priv, 0, 1, lParam);
    }
    break;
    case WM_RBUTTONDOWN:
        HandleButtonClick( gra_priv, 1, 3,lParam );
        break;
    case WM_RBUTTONUP:
        HandleButtonClick( gra_priv, 0, 3,lParam );
        break;
    case WM_LBUTTONDBLCLK:
        dbg(lvl_debug, "LBUTTONDBLCLK\n");
        HandleButtonClick( gra_priv, 1, 6,lParam );
        break;
    case WM_CHAR:
        HandleKeyChar( gra_priv, wParam);
        break;
    case WM_KEYDOWN:
        HandleKeyDown( gra_priv, wParam);
        break;
   case WM_COPYDATA:
        dbg(lvl_debug,"got WM_COPYDATA\n");
        callback_list_call_attr_2(gra_priv->cbl, attr_wm_copydata, (void *)wParam, (void*)lParam);
        break;
    default:
        return DefWindowProc(hwnd, Message, wParam, lParam);
    }
    return 0;
}

static int fullscreen(struct window *win, int on)
{
    if (on) {
        ShowWindow(g_hwnd, SW_MAXIMIZE);
    } else {
        ShowWindow(g_hwnd, SW_RESTORE);
    }

    return 0;
}

extern void WINAPI SystemIdleTimerReset(void);


static void disable_suspend(struct window *win)
{
}

static const TCHAR g_szClassName[] = {'N','A','V','G','R','A','\0'};

static HANDLE CreateGraphicsWindows( struct graphics_priv* gr, HMENU hMenu )
{
    dbg(lvl_error,"enter\n");
    int wStyle = WS_VISIBLE;
    HWND hwnd;
    WNDCLASSEX wc;
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.hIconSm       = NULL;
    wc.style     = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc  = WndProc;
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 64;
    wc.hInstance    = GetModuleHandle(NULL);
    wc.hIcon    = NULL;
    wc.hCursor  = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;

    if (!RegisterClassEx(&wc))
    {
        dbg(lvl_error, "Window registration failed\n");
        return NULL;
    }else{
        dbg(lvl_error, "Window registration OK\n");
        }

    if(gr->frame)
    {
        if ( hMenu )
        {
            wStyle = WS_CHILD;
            dbg(lvl_error, "wStyle = WS_CHILD;\n");
        } else {
            wStyle = WS_OVERLAPPED|WS_VISIBLE;
            dbg(lvl_error, "wStyle = WS_OVERLAPPED|WS_VISIBLE\n");
        }
    } else {
            wStyle = WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            dbg(lvl_error, "wStyle = WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN\n");
    }
    g_hwnd = hwnd = CreateWindow(g_szClassName,
                                 TEXT("Navit"),
                                 wStyle,
                                 gr->x,
                                 gr->y,
                                 gr->width,
                                 gr->height,
                                 (HWND)gr->wnd_parent_handle,
                                 hMenu,
                                 GetModuleHandle(NULL),
                                 NULL);
    
    if (hwnd == NULL)
    {
        dbg(lvl_error, "Window creation failed: %d\n",  GetLastError());
        return NULL;
    }
    /* For Vista, we need here ChangeWindowMessageFilter(WM_COPYDATA,MSGFLT_ADD); since Win7 we need above one or ChangeWindowMessageFilterEx (MSDN), both are
      not avail for earlier Win and not present in my mingw :(. ChangeWindowMessageFilter may not be present in later Win versions. Welcome late binding!
    */
    if(gr->ChangeWindowMessageFilter)
        gr->ChangeWindowMessageFilter(WM_COPYDATA,1 /*MSGFLT_ADD*/);
    else if(gr->ChangeWindowMessageFilterEx) 
        gr->ChangeWindowMessageFilterEx(hwnd,WM_COPYDATA,1 /*MSGFLT_ALLOW*/,NULL);

    gr->wnd_handle = hwnd;

    callback_list_call_attr_2(gr->cbl, attr_resize, (void *)gr->width, (void *)gr->height);
    create_memory_dc(gr);

    SetWindowLongPtr( hwnd , DWLP_USER, (LONG_PTR)gr );

    ShowWindow( hwnd, SW_SHOW );
    UpdateWindow( hwnd );

    PostMessage( (HWND)gr->wnd_parent_handle, WM_USER + 1, 0, 0 );

    return hwnd;
}


static void graphics_destroy(struct graphics_priv *gr)
{
    g_free( gr );
}


static void gc_destroy(struct graphics_gc_priv *gc)
{
    DeleteObject( gc->hpen );
    DeleteObject( gc->hbrush );
    g_free( gc );
}

static void gc_set_linewidth(struct graphics_gc_priv *gc, int w)
{
    DeleteObject (gc->hpen);
    gc->line_width = w;
    gc->hpen = CreatePen(gc->dashed?PS_DASH:PS_SOLID, gc->line_width, gc->fg_color );
}

static void gc_set_dashes(struct graphics_gc_priv *gc, int width, int offset, unsigned char dash_list[], int n)
{
    gc->dashed=n>0;
    DeleteObject (gc->hpen);
    gc->hpen = CreatePen(gc->dashed?PS_DASH:PS_SOLID, gc->line_width, gc->fg_color );
}


static void gc_set_foreground(struct graphics_gc_priv *gc, struct color *c)
{
    gc->fg_color = RGB( c->r >> 8, c->g >> 8, c->b >> 8);
    gc->fg_alpha = c->a;
    DeleteObject (gc->hpen);
    DeleteObject (gc->hbrush);
    gc->hpen = CreatePen(gc->dashed?PS_DASH:PS_SOLID, gc->line_width, gc->fg_color );
    gc->hbrush = CreateSolidBrush( gc->fg_color );
    if ( gc->gr && c->a < 0xFFFF )
    {
        gc->gr->transparent_color = *c;
    }
}

static void gc_set_background(struct graphics_gc_priv *gc, struct color *c)
{
    gc->bg_color = RGB( c->r >> 8, c->g >> 8, c->b >> 8);
    gc->bg_alpha = c->a;
    if ( gc->gr && gc->gr->hMemDC )
        SetBkColor( gc->gr->hMemDC, gc->bg_color );
}

static struct graphics_gc_methods gc_methods =
{
    gc_destroy,
    gc_set_linewidth,
    gc_set_dashes,
    gc_set_foreground,
    gc_set_background,
};

static struct graphics_gc_priv *gc_new(struct graphics_priv *gr, struct graphics_gc_methods *meth)
{
    struct graphics_gc_priv *gc=g_new(struct graphics_gc_priv, 1);
    *meth=gc_methods;
    gc->hwnd = (HWND)gr->wnd_handle;
    gc->line_width = 1;
    gc->fg_color = RGB( 0,0,0 );
    gc->bg_color = RGB( 255,255,255 );
    gc->fg_alpha = 65535;
    gc->bg_alpha = 0;
    gc->dashed=0;
    gc->hpen = CreatePen( PS_SOLID, gc->line_width, gc->fg_color );
    gc->hbrush = CreateSolidBrush( gc->fg_color );
    gc->gr = gr;
    return gc;
}


static void draw_lines(struct graphics_priv *gr, struct graphics_gc_priv *gc, struct point *p, int count)
{
    int i;
    HPEN hpenold = (HPEN)SelectObject(gr->hMemDC, gc->hpen );
    int oldbkmode=SetBkMode( gr->hMemDC, TRANSPARENT);
    int first = 1;
    for ( i = 0; i< count; i++ )
    {
        if ( first )
        {
            first = 0;
            MoveToEx( gr->hMemDC, p[0].x, p[0].y, NULL );
        }
        else
        {
            LineTo( gr->hMemDC, p[i].x, p[i].y );
        }
    }
    SetBkMode( gr->hMemDC, oldbkmode);
    SelectObject( gr->hMemDC, hpenold);
}

static void draw_polygon(struct graphics_priv *gr, struct graphics_gc_priv *gc, struct point *p, int count)
{
    HPEN holdpen = (HPEN)SelectObject( gr->hMemDC, gc->hpen );
    HBRUSH holdbrush = (HBRUSH)SelectObject( gr->hMemDC, gc->hbrush );
    
    if (sizeof(POINT) != sizeof(struct point)) {
        int i;
        POINT* points=(POINT*)g_alloca(sizeof(POINT)*count);
        for ( i=0;i< count; i++ )
        {
        points[i].x = p[i].x;
        points[i].y = p[i].y;
        }
            Polygon( gr->hMemDC, points,count );
    } else
        Polygon( gr->hMemDC, (POINT *)p, count);
    SelectObject( gr->hMemDC, holdbrush);
    SelectObject( gr->hMemDC, holdpen);
}


static void draw_rectangle(struct graphics_priv *gr, struct graphics_gc_priv *gc, struct point *p, int w, int h)
{
    HPEN holdpen = (HPEN)SelectObject( gr->hMemDC, gc->hpen );
    HBRUSH holdbrush = (HBRUSH)SelectObject( gr->hMemDC, gc->hbrush );

    Rectangle(gr->hMemDC, p->x, p->y, p->x+w, p->y+h);

    SelectObject( gr->hMemDC, holdbrush);
    SelectObject( gr->hMemDC, holdpen);
}

static void draw_circle(struct graphics_priv *gr, struct graphics_gc_priv *gc, struct point *p, int r)
{
    HPEN holdpen = (HPEN)SelectObject( gr->hMemDC, gc->hpen );
    HBRUSH holdbrush = (HBRUSH)SelectObject( gr->hMemDC, GetStockObject(NULL_BRUSH));

    r=r/2;

    Ellipse( gr->hMemDC, p->x - r, p->y -r, p->x + r, p->y + r );

    SelectObject( gr->hMemDC, holdbrush);
    SelectObject( gr->hMemDC, holdpen);
}


static void draw_drag(struct graphics_priv *gr, struct point *p)
{
    if ( p )
    {
        gr->p.x    = p->x;
        gr->p.y    = p->y;

        if ( p->x < 0 || p->y < 0 ||
                ( gr->parent && ((p->x + gr->width > gr->parent->width) || (p->y + gr->height > gr->parent->height) )))
        {
            gr->disabled = TRUE;
        }
        else
        {
            gr->disabled = FALSE;
        }
    }
}

static int
xpmdecode(char *name, struct graphics_image_priv *img);

static int
gdiplusimagedecode(char *name, struct graphics_image_priv *img)
{
    const size_t cSize = strlen(name)+1;
    wchar_t* wc = new wchar_t[cSize];
    size_t tmp = 0;
    mbstowcs_s(&tmp, wc, cSize, name, cSize);

    img->gdiplusimage = Gdiplus::Image::FromFile(wc);
    if (!img->gdiplusimage)
    {
        return FALSE;
    }
    img->height= img->gdiplusimage->Gdiplus::Image::GetHeight();
    img->width= img->gdiplusimage->Gdiplus::Image::GetWidth();
    img->hot.x= 0;
    img->hot.y= 0;
    return TRUE;
}

static struct graphics_image_priv *image_new(struct graphics_priv *gr, struct graphics_image_methods *meth, char *name, int *w, int *h, struct point *hot, int rotation)
{
    dbg(lvl_debug,"enter %s, w = %i, h = %i\n",name, *w, *h);
    
    struct graphics_image_priv* ret;
    
    char* hash_key = g_strdup_printf("%s_%d_%d_%d",name,*w,*h,rotation);

    ret = (graphics_image_priv*)g_hash_table_lookup(gr->image_cache_hash, hash_key);
    if(!ret)
    {
        int len=strlen(name);
        int rc=0;

        if (len >= 4)
        {
            char *ext;
            dbg(lvl_info, "loading image '%s'\n", name );
            ret = g_new0( struct graphics_image_priv, 1 );
            ext = name+len-4;
            if (!strcmp(ext,".xpm")) {
                rc=xpmdecode(name, ret);
            } else if (!strcmp(ext,".png")) {
                rc=gdiplusimagedecode(name, ret);
                ret->hot.x=ret->width/2-1;
                ret->hot.y=ret->height/2-1;
            }else if (!strcmp(ext,".jpg")){
                rc=gdiplusimagedecode(name,ret);
                }
        }
        if (!rc) {
            dbg(lvl_warning, "failed loading '%s'\n", name );
            g_free(ret);
            ret=NULL;
        }

        g_hash_table_insert(gr->image_cache_hash, hash_key,  (gpointer)ret );
        /* Hash_key will be freed ater the hash table, so set it to NULL here to disable freing it on this function return */
        hash_key=NULL;
        if(ret) {
            if (*w==-1)
                *w=ret->width;
            if (*h==-1)
                *h=ret->height;
         //   if (*w!=ret->width || *h!=ret->height) {
         //       if(ret->png_pixels && ret->hBitmap)
         //           pngscale(ret, gr, *w, *h);
         //   }
       }
    }
    if (ret) {
        *w=ret->width;
        *h=ret->height;
        if (hot)
            *hot=ret->hot;
    }
    g_free(hash_key);
    return ret;
}


static void draw_image(struct graphics_priv *gr, struct graphics_gc_priv *fg, struct point *p, struct graphics_image_priv *img)
{
    if (img->pxpm){
        Xpm2bmp_paint( img->pxpm , gr->hMemDC, p->x, p->y );
        return;
    }

    if (img->gdiplusimage){
        Gdiplus::Graphics graphics(gr->hMemDC);
        graphics.DrawImage(img->gdiplusimage, p->x, p->y );
        return;
    }
}

static void
draw_image_warp(struct graphics_priv *gr, struct graphics_gc_priv *fg, struct point *p, int count, struct graphics_image_priv *img)
{
    int w = -1, h = -1;

    dbg(lvl_debug, "draw_image_warp enter\n");
    if (count > 1) {
        w = p[1].x - p->x;
        dbg(lvl_debug, "draw_image_warp width = %i\n", w);
    }
    if (count > 2) {
        h =  p[2].y - p->y;
        dbg(lvl_debug, "draw_image_warp height = %i\n", h);
    }
    if(img->gdiplusimage){ //TODO handle rotation
        Gdiplus::Graphics graphics(gr->hMemDC);
        Gdiplus::Rect destrect(p->x, p->y, w+1, h+1);
        graphics.DrawImage(img->gdiplusimage, destrect);
    }
}

static void draw_mode(struct graphics_priv *gr, enum draw_mode_num mode)
{
    if ( mode == draw_mode_begin )
    {
        if ( gr->wnd_handle == NULL )
        {
            CreateGraphicsWindows( gr, (HMENU)ID_CHILD_GFX );
        }
        if ( gr->mode != draw_mode_begin )
        {
            if ( gr->hMemDC )
            {
                dbg(lvl_debug, "Erase dc: %x, w: %d, h: %d, bg_color: %x\n", gr, gr->width, gr->height, gr->bg_color);
            }
        }
    }

    // force paint
    if (mode == draw_mode_end && gr->mode == draw_mode_begin)
    {
        InvalidateRect( (HWND)gr->wnd_handle, NULL, FALSE );
    }
    gr->mode=mode;
}

static void * get_data(struct graphics_priv *this_, const char *type)
{
    dbg(lvl_error,"enter\n");
    if ( strcmp( "wnd_parent_handle_ptr", type ) == 0 )
    {
        return &( this_->wnd_parent_handle );
    }
    if ( strcmp( "START_CLIENT", type ) == 0 )
    {
        CreateGraphicsWindows( this_, (HMENU)ID_CHILD_GFX );
        return NULL;
    }
    if (!strcmp(type, "window"))
    {
        CreateGraphicsWindows( this_ , NULL);
        this_->window.fullscreen = fullscreen;
        this_->window.disable_suspend = disable_suspend;
        this_->window.priv=g_new0(struct window_priv, 1);
        return &this_->window;
    }
    return NULL;
}


static void background_gc(struct graphics_priv *gr, struct graphics_gc_priv *gc)
{
    RECT rcClient = { 0, 0, gr->width, gr->height };
    HBRUSH bgBrush;

    bgBrush = CreateSolidBrush( gc->bg_color  );
    gr->bg_color = gc->bg_color;

    FillRect( gr->hMemDC, &rcClient, bgBrush );
    DeleteObject( bgBrush );
}

struct graphics_font_priv
{
    LOGFONT lf;
    HFONT hfont;
    int size;
};

static void draw_text(struct graphics_priv *gr, struct graphics_gc_priv *fg, struct graphics_gc_priv *bg, struct graphics_font_priv *font, char *text, struct point *p, int dx, int dy)
{
    RECT rcClient;
    int prevBkMode;
    HFONT hOldFont;
    double angle;

    GetClientRect( (HWND)gr->wnd_handle, &rcClient );

    prevBkMode = SetBkMode( gr->hMemDC, TRANSPARENT );

    if ( NULL == font->hfont )
    {
#ifdef WIN_USE_SYSFONT
        font->hfont = (HFONT) GetStockObject (SYSTEM_FONT);
        GetObject (font->hfont, sizeof (LOGFONT), &font->lf);
#else
        font->hfont = EzCreateFont (gr->hMemDC, TEXT ("Arial"), font->size/2, 0, 0, TRUE);
        GetObject ( font->hfont, sizeof (LOGFONT), &font->lf) ;
#endif
    }

    angle = -atan2( dy, dx ) * 180 / 3.14159 ;
    if (angle < 0)
        angle += 360;

    SetTextAlign (gr->hMemDC, TA_BASELINE) ;
    SetViewportOrgEx (gr->hMemDC, p->x, p->y, NULL) ;
    font->lf.lfEscapement = font->lf.lfOrientation = ( angle * 10 ) ;
    DeleteObject (font->hfont) ;

    font->hfont = CreateFontIndirect (&font->lf);
    hOldFont = (HFONT)SelectObject(gr->hMemDC, font->hfont );

    {
        wchar_t utf16[1024];
        const UTF8 *utf8 = (UTF8 *)text;
        UTF16 *utf16p = (UTF16 *) utf16;
        SetBkMode (gr->hMemDC, TRANSPARENT);
        if (ConvertUTF8toUTF16(&utf8, utf8+strlen(text),
                               &utf16p, utf16p+sizeof(utf16),
                               lenientConversion) == conversionOK)
        {
        if(bg && bg->fg_alpha) {
            SetTextColor(gr->hMemDC, bg->fg_color);
            ExtTextOutW(gr->hMemDC, -1, -1, 0, NULL,
                        utf16, (wchar_t*) utf16p - utf16, NULL);
                ExtTextOutW(gr->hMemDC, 1, 1, 0, NULL,
                        utf16, (wchar_t*) utf16p - utf16, NULL);
            ExtTextOutW(gr->hMemDC, -1, 1, 0, NULL,
                        utf16, (wchar_t*) utf16p - utf16, NULL);
                ExtTextOutW(gr->hMemDC, 1, -1, 0, NULL,
                        utf16, (wchar_t*) utf16p - utf16, NULL);
            }
            SetTextColor(gr->hMemDC, fg->fg_color);
            ExtTextOutW(gr->hMemDC, 0, 0, 0, NULL,
                        utf16, (wchar_t*) utf16p - utf16, NULL);
        }
    }

    SelectObject(gr->hMemDC, hOldFont);
    DeleteObject (font->hfont) ;

    SetBkMode( gr->hMemDC, prevBkMode );

    SetViewportOrgEx (gr->hMemDC, 0, 0, NULL) ;
}

static void font_destroy(struct graphics_font_priv *font)
{
    dbg(lvl_error,"enter\n");
    if ( font->hfont )
    {
        DeleteObject(font->hfont);
    }
    g_free(font);
}

static struct graphics_font_methods font_methods =
{
    font_destroy
};

static struct graphics_font_priv *font_new(struct graphics_priv *gr, struct graphics_font_methods *meth, char *name, int size, int flags)
{
    dbg(lvl_error,"enter name = %s\n", name);
    struct graphics_font_priv *font=g_new(struct graphics_font_priv, 1);
    *meth = font_methods;

    font->hfont = NULL;
    font->size = size;

    return font;
}


static int
xpmdecode(char *name, struct graphics_image_priv *img)
{
    img->pxpm = Xpm2bmp_new();
    if (Xpm2bmp_load( img->pxpm, name ) != 0)
    {
        g_free(img->pxpm);
        return FALSE;
    }
    img->width=img->pxpm->size_x;
    img->height=img->pxpm->size_y;
    img->hot.x=img->pxpm->hotspot_x;
    img->hot.y=img->pxpm->hotspot_y;
    return TRUE;
}




static struct graphics_priv *
            graphics_win32_new_helper(struct graphics_methods *meth);

static void overlay_resize(struct graphics_priv *gr, struct point *p, int w, int h, int alpha, int wraparound)
{
    dbg(lvl_debug, "resize overlay: %x, x: %d, y: %d, w: %d, h: %d, alpha: %x, wraparound: %d\n", gr, p->x, p->y, w, h, alpha, wraparound);

    if ( gr->width != w || gr->height != h )
    {
        gr->width  = w;
        gr->height = h;
        create_memory_dc(gr);
    }
    gr->p.x    = p->x;
    gr->p.y    = p->y;
}


static struct graphics_priv *
            overlay_new(struct graphics_priv *gr, struct graphics_methods *meth, struct point *p, int w, int h, int alpha, int wraparound)
{
    struct graphics_priv *gpriv=graphics_win32_new_helper(meth);
    dbg(lvl_debug, "overlay: %x, x: %d, y: %d, w: %d, h: %d, alpha: %x, wraparound: %d\n", gpriv, p->x, p->y, w, h, alpha, wraparound);
    gpriv->width  = w;
    gpriv->height = h;
    gpriv->parent = gr;
    gpriv->p.x    = p->x;
    gpriv->p.y    = p->y;
    gpriv->disabled = 0;
    gpriv->hPrebuildDC = 0;
    gpriv->AlphaBlend = gr->AlphaBlend;
    gpriv->image_cache_hash = gr->image_cache_hash;
    
    gpriv->next = gr->overlays;
    gr->overlays = gpriv;
    gpriv->wnd_handle = gr->wnd_handle;

    if (wraparound)
    {
        if ( p->x < 0 )
        {
            gpriv->p.x += gr->width;
        }

        if ( p->y < 0 )
        {
            gpriv->p.y += gr->height;
        }
    }

    create_memory_dc(gpriv);
    return gpriv;
}

static void overlay_disable(struct graphics_priv *gr, int disable)
{
    dbg(lvl_debug, "overlay: %x, disable: %d\n", gr, disable);
    gr->disabled = disable;
}

static void get_text_bbox(struct graphics_priv *gr, struct graphics_font_priv *font, char *text, int dx, int dy, struct point *ret, int estimate)
{
    int len = g_utf8_strlen(text, -1);
    int xMin = 0;
    int yMin = 0;
    int yMax = 13*font->size/256;
    int xMax = 9*font->size*len/256;

    dbg(lvl_info, "Get bbox for %s\n", text);

    ret[0].x = xMin;
    ret[0].y = -yMin;
    ret[1].x = xMin;
    ret[1].y = -yMax;
    ret[2].x = xMax;
    ret[2].y = -yMax;
    ret[3].x = xMax;
    ret[3].y = -yMin;
}


static struct graphics_methods graphics_methods =
{
    graphics_destroy,
    draw_mode,
    draw_lines,
    draw_polygon,
    draw_rectangle,
    draw_circle,
    draw_text,
    draw_image,
    draw_image_warp,
    draw_drag,
    font_new,
    gc_new,
    background_gc,
    overlay_new,
    image_new,
    get_data,
    NULL,   //image_free
    get_text_bbox,
    overlay_disable,
    overlay_resize,
};


static struct graphics_priv *
            graphics_win32_new_helper(struct graphics_methods *meth)
{
    dbg(lvl_error,"enter\n");
    struct graphics_priv *this_=g_new0(struct graphics_priv,1);
    *meth=graphics_methods;
    this_->mode = (draw_mode_num)-1;
    return this_;
}

static void bind_late(struct graphics_priv* gra_priv)
{
    dbg(lvl_error,"enter\n");
    gra_priv->hCoreDll = LoadLibrary(TEXT("msimg32.dll"));
    gra_priv->hGdi32Dll = LoadLibrary(TEXT("gdi32.dll"));
    gra_priv->hGdiplusDll = LoadLibrary(TEXT("Gdiplus.dll"));
    gra_priv->hUser32Dll = GetModuleHandle("user32.dll");
    
    if (gra_priv->hGdiplusDll){
        dbg(lvl_error, "++++++++ Gdiplus loaded +++++++++\n");
    }else{
        dbg(lvl_error,"----------- Gdiplus NOT loaded-------------\n");
    }

   Gdiplus::GdiplusStartupInput gdiplusStartupInput;
   ULONG_PTR gdiplusToken;
   
   dbg(lvl_error,"GDI startup result = %d\n",Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL))
   
    if ( gra_priv->hCoreDll )
    {
        gra_priv->AlphaBlend  = (FP_AlphaBlend)GetProcAddress((HMODULE)gra_priv->hCoreDll, TEXT("AlphaBlend") );
        if (!gra_priv->AlphaBlend)
        {
            dbg(lvl_warning, "AlphaBlend not supported\n");
        }

        gra_priv->SetStretchBltMode= (FP_SetStretchBltMode)GetProcAddress((HMODULE)gra_priv->hGdi32Dll, TEXT("SetStretchBltMode") );

        if (!gra_priv->SetStretchBltMode)
        {
            dbg(lvl_warning, "SetStretchBltMode not supported\n");
        }
    }
    else
    {
        dbg(lvl_error, "Error loading coredll\n");
    }
    
//    if(gra_priv->hUser32Dll) {
//      gra_priv->ChangeWindowMessageFilterEx=GetProcAddress(gra_priv->hUser32Dll,"ChangeWindowMessageFilterEx");
//      gra_priv->ChangeWindowMessageFilter=GetProcAddress(gra_priv->hUser32Dll,"ChangeWindowMessageFilter");
//    }
}

static struct graphics_priv*
            graphics_win32_new( struct navit *nav, struct graphics_methods *meth, struct attr **attrs, struct callback_list *cbl)
{

    dbg(lvl_error,"enter\n");
    struct attr *attr;

    struct graphics_priv* this_;
    if (!event_request_system("eventwin32","eventwin32"))
        return NULL;
    this_=graphics_win32_new_helper(meth);
    this_->nav=nav;
    this_->frame=1;
    if ((attr=attr_search(attrs, NULL, attr_frame)))
        this_->frame=attr->u.num;
    this_->x=0;
    if ((attr=attr_search(attrs, NULL, attr_x)))
        this_->x=attr->u.num;
    this_->y=0;
    if ((attr=attr_search(attrs, NULL, attr_y)))
        this_->y=attr->u.num;
    this_->width=792;
    if ((attr=attr_search(attrs, NULL, attr_w)))
        this_->width=attr->u.num;
    this_->height=547;
    if ((attr=attr_search(attrs, NULL, attr_h)))
        this_->height=attr->u.num;
    this_->overlays = NULL;
    this_->cbl=cbl;
    this_->parent = NULL;
    this_->window.priv = NULL;
    this_->image_cache_hash = g_hash_table_new(g_str_hash, g_str_equal);

    bind_late(this_);

    return this_;
}

struct event_timeout
{
    UINT_PTR timer_id;
    int multi;
    struct callback *cb;
    struct event_timeout *next;
};


void
plugin_init(void)
{
    plugin_register_graphics_type("win32", graphics_win32_new);
}
