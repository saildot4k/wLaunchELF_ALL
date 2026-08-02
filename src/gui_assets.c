#include "launchelf.h"
#include "draw_private.h"
#include "gui_assets.h"

#define GUI_BG_WIDTH 256
#define GUI_BG_HEIGHT 256
#define GUI_SPLASH_WIDTH 512
#define GUI_SPLASH_HEIGHT 256
#define GUI_ICON_WIDTH 32
#define GUI_ICON_HEIGHT 16
#define GUI_ICON_ATLAS_COLUMNS 8
#define GUI_ICON_ATLAS_WIDTH (GUI_ICON_ATLAS_COLUMNS * GUI_ICON_WIDTH)
#define GUI_ICON_ATLAS_HEIGHT 64

#ifndef GS_FILTER_NEAREST
#define GS_FILTER_NEAREST GS_FILTER_LINEAR
#endif

#define GUI_TEXTURE_COLOR GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00)

extern u8 gui_asset_bg_rgba[];
extern u8 gui_asset_splash_rgba[];
extern u8 gui_asset_icons_rgba[];

static GSTEXTURE gui_bg_texture;
static GSTEXTURE gui_splash_texture;
static GSTEXTURE gui_icons_texture;
static int gui_assets_uploaded = 0;

static void guiUploadTexture(GSTEXTURE *texture, u8 *data, int width, int height, int filter)
{
	texture->Width = width;
	texture->Height = height;
	texture->PSM = GS_PSM_CT32;
	texture->Mem = (u32 *)data;
	texture->Vram = gsKit_vram_alloc(gsGlobal,
	                                 gsKit_texture_size(texture->Width, texture->Height, texture->PSM),
	                                 GSKIT_ALLOC_USERBUFFER);
	texture->Filter = filter;
	gsKit_texture_upload(gsGlobal, texture);
}

void guiAssetsUpload(void)
{
	if (gui_assets_uploaded || gsGlobal == NULL)
		return;

	guiUploadTexture(&gui_bg_texture, gui_asset_bg_rgba, GUI_BG_WIDTH, GUI_BG_HEIGHT, GS_FILTER_LINEAR);
	guiUploadTexture(&gui_splash_texture, gui_asset_splash_rgba, GUI_SPLASH_WIDTH, GUI_SPLASH_HEIGHT, GS_FILTER_NEAREST);
	guiUploadTexture(&gui_icons_texture, gui_asset_icons_rgba, GUI_ICON_ATLAS_WIDTH, GUI_ICON_ATLAS_HEIGHT, GS_FILTER_NEAREST);
	gui_assets_uploaded = 1;
}

void guiAssetsInvalidate(void)
{
	memset(&gui_bg_texture, 0, sizeof(gui_bg_texture));
	memset(&gui_splash_texture, 0, sizeof(gui_splash_texture));
	memset(&gui_icons_texture, 0, sizeof(gui_icons_texture));
	gui_assets_uploaded = 0;
}

int guiAssetsReady(void)
{
	guiAssetsUpload();
	return gui_assets_uploaded;
}

static int guiDrawTexture(GSTEXTURE *texture,
                          int src_x, int src_y, int src_w, int src_h,
                          int dst_x, int dst_y, int dst_w, int dst_h,
                          GuiZLayer z)
{
	int prev_alpha_enable;

	if (!guiAssetsReady() || dst_w <= 0 || dst_h <= 0)
		return 0;

	prev_alpha_enable = gsGlobal->PrimAlphaEnable;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsKit_prim_sprite_texture(gsGlobal, texture,
	                          dst_x, dst_y, src_x, src_y,
	                          dst_x + dst_w, dst_y + dst_h, src_x + src_w, src_y + src_h,
	                          z, GUI_TEXTURE_COLOR);
	gsGlobal->PrimAlphaEnable = prev_alpha_enable;

	updateScr_1 = 1;
	return 1;
}

int guiDrawBackground(void)
{
	return guiDrawTexture(&gui_bg_texture,
	                      0, 0, GUI_BG_WIDTH, GUI_BG_HEIGHT,
	                      0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
	                      GUI_Z_BACKGROUND);
}

int guiDrawSplash(void)
{
	int x = (SCREEN_WIDTH - GUI_SPLASH_WIDTH) / 2;
	int y = (SCREEN_HEIGHT - GUI_SPLASH_HEIGHT) / 2;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	return guiDrawTexture(&gui_splash_texture,
	                      0, 0, GUI_SPLASH_WIDTH, GUI_SPLASH_HEIGHT,
	                      x, y, GUI_SPLASH_WIDTH, GUI_SPLASH_HEIGHT,
	                      GUI_Z_DIALOG);
}

int guiDrawFileIcon(GuiIconId icon_id, int x, int y)
{
	int src_x, src_y;

	if (icon_id < 0 || icon_id >= GUI_ICON_COUNT)
		icon_id = GUI_ICON_FILE;

	src_x = (icon_id % GUI_ICON_ATLAS_COLUMNS) * GUI_ICON_WIDTH;
	src_y = (icon_id / GUI_ICON_ATLAS_COLUMNS) * GUI_ICON_HEIGHT;

	return guiDrawTexture(&gui_icons_texture,
	                      src_x, src_y, GUI_ICON_WIDTH, GUI_ICON_HEIGHT,
	                      x, y, GUI_ICON_WIDTH, GUI_ICON_HEIGHT,
	                      GUI_Z_CONTENT);
}
