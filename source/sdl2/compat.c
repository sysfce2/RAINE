// Compatibility layer with allegro

#include <SDL.h>
#include "raine.h"
#include <sys/stat.h>
#include "raine_cpuid.h"
#include "video/tilemod.h" // internal_bpp
#include "blit.h" // ReClipScreen
#include <SDL_ttf.h>
#include "palette.h"
#include "blit_sdl.h"
#include "display_sdl.h"
#include "control.h"
#include "control_internal.h"
#include "blit_x2.h"
#include "video/res.h"
#include "dialogs/messagebox.h"
#include "opengl.h"
#include "loadpng.h"
#include "version.h"
#include "display.h"
#include <SDL_image.h>
#include "files.h"
#include "leds.h"

void sdl_fatal_error(const char *file, const char *func, int line, char *format, ...) {
    char msg[512];
    va_list ap;
    va_start(ap,format);
    snprintf(msg,512,"function %s file %s line %d\n",func,file,line);
    vsnprintf(&msg[strlen(msg)],512-strlen(msg),format,ap);
    va_end(ap);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
	    "Error",
	    msg,
	    NULL);
    if (current_game->clear) (*current_game->clear)();
    exit(1);
}

UINT32 sdl2_color_format;

struct al_bitmap *sdl_create_bitmap_ex(int bpp, int w, int h) {
  // Init a fake bitmap to point to a newly created sdl_surface
  UINT32 r=0,g=0,b=0,a=0; // masks if necessary...
  SDL_Surface *s;

  if (bpp > 8) {

      switch(bpp) {
      case 8:
	  r = g = b = a = 0;
	  break;
      default:
	  // A considerable change compared to sdl-1.2... !
	  // The format here is just one I chose, 32 bits is almost mandatory in 3d, but for GameBitmap we don't need alpha
	  // so rgbx becomes the 1st choice
	  sdl2_color_format = SDL_PIXELFORMAT_RGB565;
	  display_cfg.bpp = 16; // normally this is initialized in InitPaletteMap, assuming the color mapper was defined before that !
	  if (current_colour_mapper == &col_map_24bit_rgb || // macrossp
		  current_colour_mapper == &col_map_xxxx_xxxx_rrrr_rrrr_gggg_gggg_bbbb_bbbb) {// f3 games
	      sdl2_color_format = SDL_PIXELFORMAT_RGBX8888;
	      display_cfg.bpp = 32;
	  }
	  print_debug("bpp initialized from color_mapper : %d\n",display_cfg.bpp);
	  if (!SDL_PixelFormatEnumToMasks(sdl2_color_format,&bpp,&r,&g,&b,&a))
	  {
	      fatal_error("masks pas ok");
	  }
	  // Notice that here display_cfg.bpp is 16 or 32, and the default video mode sets it at 32
	  // so update_pal_banks will eventually reduce the banks which is ok
	  update_pal_banks();
	  if (color_format)
	      SDL_FreeFormat(color_format);
	  color_format = SDL_AllocFormat( sdl2_color_format);
	  if (current_colour_mapper)
	      set_colour_mapper(current_colour_mapper);
      }
  }

  s = SDL_CreateRGBSurface(SDL_SWSURFACE, // these bitmaps are ALWAYS in ram
			   w,h,bpp,r,g,b,a);
  return surface_to_bmp(s);
}

// Not sure the locking functions are usefull, they are used only in the C
// video functions apparently. I should investigate about that...
void acquire_bitmap(al_bitmap *bmp) {
  SDL_LockSurface(get_surface_from_bmp(bmp));
}

void release_bitmap(al_bitmap *bmp) {
  SDL_UnlockSurface(get_surface_from_bmp(bmp));
}

// Strings...

// Return the filename without the path... / and \\ accepted (lovely dos !)
char *get_filename(char *f) {
  char *s = strrchr(f,'/');
  if (!s)
    s = strrchr(f,'\\');
  if (s)
    return s+1;
  return f;
}

#ifndef RAINE_WIN32
void strlwr(char *s) {
  while (*s) {
    if (*s >= 'A' && *s <= 'Z')
      *s+=32;
    s++;
  }
}
#endif

void put_backslash(char *s) {
  int l = strlen(s)-1;
  s += l;
  if (*s != '/' && *s != '\\')
    strcat(s,SLASH);
}

// Todo...

int exists(char *filename) {
  // This is very fast in linux, but the disk cache of windows for cdrom
  // seems catastrophic ! Extremely slow, no wonder that games never read from
  // cds... !!!
  struct stat buf;
  return !stat(filename,&buf); // stat returns 0 on success...
}

SDL_PixelFormat *color_format;
SDL_Window *win;
SDL_Renderer *rend;
SDL_Surface *sdl_screen;
static int my_mouse_x,my_mouse_y;

/* About -fsanitize=address here : it creates an error in SDL_Init, and seems to ignore the __attribute__((no_sanitize_address))
 * Workaround for now : set ASAN_OPTIONS=detect_stack_use_after_return=0
 * It's a little extreme, but works fine for this at least, there might be a better workaround though. */
void  sdl_init() {
  /* Initialize the SDL library */
    static int init;
    if (!init) {
	init = 1;
	// The whole hints used by testgamecontroller, some of them are not even documented !
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
	SDL_SetHint(SDL_HINT_LINUX_JOYSTICK_DEADZONES, "1");
	if ( SDL_Init(
		    SDL_INIT_TIMER|SDL_INIT_AUDIO| SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS | SDL_INIT_JOYSTICK
		    ) < 0 ) {
	    fatal_error( "Couldn't initialize SDL: %s",SDL_GetError());
	}
	if ( TTF_Init() < 0 ) {
	    fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
	    SDL_Quit();
	    return;
	}
	char title[20];
	snprintf(title,20,"%s v%s",EMUNAME,VERSION);
	title[19] = 0;
	// don't send fullscreen here because SDL_SetWindowPosition is affected by window decorations at least in linux and we can't get the decorations size in fullscreen
	SDL_Rect usable;
	SDL_GetDisplayUsableBounds(0, &usable);
	int status = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	if (display_cfg.maximized) {
	    status |= SDL_WINDOW_MAXIMIZED;
	}
	if (display_cfg.fullscreen)
#ifdef RAINE_WIN32
	    win = SDL_CreateWindow(title,
		    display_cfg.prev_posx,
		    display_cfg.prev_posy,
		    display_cfg.prev_sx,
		    display_cfg.prev_sy,
		    status |
		    SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
	    // see comments about broken fullscreen in control.c
	    win = SDL_CreateWindow(title,
		    display_cfg.prev_posx,
		    display_cfg.prev_posy,
		    display_cfg.prev_sx,
		    display_cfg.prev_sy,
		    status);
#endif
	else
	    if (display_cfg.posx || display_cfg.posy)
		win = SDL_CreateWindow(title,
			display_cfg.posx,
			display_cfg.posy,
			display_cfg.screen_x,
			display_cfg.screen_y,
			status);
	    else
		win = SDL_CreateWindow(title,
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			display_cfg.screen_x,
			display_cfg.screen_y,
			status);
#ifdef RAINE_WIN32
	// Without this hint in windows : start a game in windowed mode, use alt-return in game
	// to switch to fullscreen, press esc to call the gui -> crash with the error :
	// fg_layer creation problem (782,785) : CreateTexture(D3DPOOL_DEFAULT): INVALIDCALL
	// Must be set before creating the renderer
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#else
	if (display_cfg.fullscreen && !hack_fs)
	    SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
	if (!win) {
	    printf("Couldn't create window, error: %s\n",SDL_GetError());
	    exit(1);
	}
	rend = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	SDL_Surface *sf = IMG_Load(get_shared("bitmaps" SLASH "bub2.png"));
	if (!sf) {
	    printf("can't load icon\n");
	} else {
	    SDL_SetWindowIcon(win,sf);
	}
	if (!display_cfg.posx && !display_cfg.posy) {
	    // Init at start...
	    SDL_GetWindowPosition(win,&display_cfg.posx,&display_cfg.posy);
	    SDL_GetWindowSize(win,&display_cfg.screen_x,&display_cfg.screen_y);
	}

	// if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1)< 0 || SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) < 0)
	//    printf("pb setting opengl version\n");

	inputs_preinit();

	sdl2_color_format = SDL_PIXELFORMAT_RGB565;
	// sdl2_color_format = SDL_PIXELFORMAT_RGBX8888;
	// default color_format for emudx games
	color_format = SDL_AllocFormat( sdl2_color_format);
	atexit(sdl_done);
	SDL_GetGlobalMouseState( &my_mouse_x, &my_mouse_y);
    }
}

static void save_game_stats() {
    FILE *f = fopen(get_shared("savedata" SLASH "stats"),"w");
    if (!f) {
	printf("save_game_stats: can't create savedata/stats ?!! path:%s\n",get_shared("savedata" SLASH "stats"));
	return;
    }
    for (int n=0; n<game_count; n++) {
	if (game_list[n]->time_played || game_list[n]->last_played)
#ifdef __x86_64__
	    fprintf(f,"%s=%d,%d,%ld\n",game_list[n]->main_name,game_list[n]->nb_loaded,game_list[n]->time_played,game_list[n]->last_played);
#else
	    fprintf(f,"%s=%d,%d,%ld\n",game_list[n]->main_name,game_list[n]->nb_loaded,game_list[n]->time_played,game_list[n]->last_played);
#endif
    }
    fclose(f);
}

void sdl_done() {
    // called at the very end, after main has finished and the debug file is already closed... !
  inputs_done();
  TTF_Quit();
  opengl_done();
  SDL_FreeFormat(color_format);
  save_game_stats();
  SDL_DestroyWindow(win);
  // For some unknown reason the mouse is warped to the bottom of the screen when exiting at least in linux, noticed lately when changing my window manager and using guake
  // It seems to happen when leaving a fullscreen mode, so I restore the mouse here, just after closing the main window...
  SDL_WarpMouseGlobal(my_mouse_x,my_mouse_y);
  SDL_Quit();
#ifdef RAINE_UNIX
  done_leds();
#endif
}

al_bitmap *surface_to_bmp(SDL_Surface *s) {
  int a;
  al_bitmap *bmp;

  bmp = malloc( sizeof(al_bitmap) + sizeof(char*) * s->h);
  bmp->extra = (void*)s;
  bmp->w = s->w; bmp->h = s->h;
  bmp->x_ofs = bmp->y_ofs = 0;
  for (a=0; a < s->h; a++)
    bmp->line[a] = (UINT8 *)s->pixels+a*s->pitch;
  bmp->dat = s->pixels;
  bmp->id = 0; // not a sub bitmap
  return bmp;
}

void destroy_bitmap(al_bitmap *bmp) {
  if (bmp) {
    if (bmp->extra) {
      if (bmp->id == 0) { // real bitmap (not sub bitmap)
	  print_debug("destroy_bitmap: calling sdl_freesurface\n");
	  SDL_FreeSurface((SDL_Surface*)bmp->extra);
      } else if (bmp->id == 2) { // sdl_overlay
	  print_debug("destroy_bitmap: calling sdl_freeoverlay\n");
	  // should be impossible to arrive here normally
	  exit(1);
      }
    }
    free(bmp);
  }
}

UINT16 bytes_per_pixel(al_bitmap *screen) {
  if (screen->extra)
    return ((screen->id&2) == 2 ? 2 // overlay
	: ((SDL_Surface*)screen->extra)->format->BytesPerPixel);
  fatal_error("no vtable and no extras for this bitmap !!!");
  return 0;
}

struct al_bitmap *sdl_create_sub_bitmap(struct al_bitmap *src, int x, int y, int w, int h) {
  /* Apparently a sub bitmap is just a bitmap with x_ofs and y_ofs correctly */
  /* initialised... */

  al_bitmap *bmp;
  int bpp;
  int n;

  if (src->id == 2) // SDL_Overlay
    bpp = 2;
  else
    bpp = bytes_per_pixel(src);

  bmp = malloc( sizeof(struct al_bitmap) + sizeof(char*) * h);
  bmp->dat = src->dat;
  bmp->w = w; bmp->h = h;
  bmp->x_ofs = x;
  bmp->y_ofs = y;
  for (n=0; n<h; n++)
    bmp->line[n] = src->line[n+y] + bpp*x;
  bmp->extra = src->extra;
  bmp->id = src->id+1; // sub bitmap

  if (!(display_cfg.video_mode == 0) &&
	  (!current_game ||
	   !(current_game->video->flags & VIDEO_NEEDS_8BPP))) {
    /* We start raine in 16bpp for yuv overlays.
     * If a hw yuv overlay can't be created, then adapt to the bpp of the
     * screen to speed up blits */
    if (current_game->video->flags & VIDEO_NEEDS_8BPP)
      display_cfg.bpp = 8;
    /*
       if (display_cfg.video_mode == 0)
       display_cfg.bpp = 16;
       */
    print_debug("bpp selected %d\n",display_cfg.bpp);
    if (current_colour_mapper) {
      set_colour_mapper(current_colour_mapper);
      ResetPalette();
    }
  }
  return bmp;
}

void save_png_surf_rev(char *name, SDL_Surface *s) {
    al_bitmap *b = surface_to_bmp(s);
    int a;
    // Flip the picture, it's upside down in opengl
    for (a=0; a < b->h; a++) {
	b->line[a] = (UINT8 *)s->pixels+(b->h-1-a)*s->pitch;
    }
    save_png(name,b,NULL);
    destroy_bitmap(b);
}

