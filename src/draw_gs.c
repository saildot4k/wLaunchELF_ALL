//--------------------------------------------------------------
//File name:   draw_gs.c
//--------------------------------------------------------------
#include "draw_private.h"
#include "gui_assets.h"

static void applyGSParams(void)
{
	switch (gsGlobal->Mode) {
		case GS_MODE_VGA_640_60:
		case GS_MODE_DTV_480P:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 448;
			gsGlobal->Interlace = GS_NONINTERLACED;
			gsGlobal->Field = GS_FRAME;
			Menu_end_y = Menu_start_y + 22 * FONT_HEIGHT;
			break;
		case GS_MODE_PAL:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 512;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			Menu_end_y = Menu_start_y + 26 * FONT_HEIGHT;
			break;
		default:
		case GS_MODE_NTSC:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 448;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			Menu_end_y = Menu_start_y + 22 * FONT_HEIGHT;
	}  // end TV_Mode change

	Frame_end_y = Menu_end_y + 3;
	Menu_tooltip_y = Frame_end_y + LINE_THICKNESS + 2;

	// Init screen size
	gsGlobal->Width = SCREEN_WIDTH;
	gsGlobal->Height = SCREEN_HEIGHT;
}
static void initScreenParams(void)
{
	// Ensure that the offsets are within valid ranges.
	if (setting->screen_x < -gsGlobal->StartX)
		setting->screen_x = -gsGlobal->StartX;
	else if (setting->screen_x > gsGlobal->StartX)
		setting->screen_x = gsGlobal->StartX;

	if (setting->screen_y < -gsGlobal->StartY)
		setting->screen_y = -gsGlobal->StartY;
	else if (setting->screen_y > gsGlobal->StartY)
		setting->screen_y = gsGlobal->StartY;
}
void setupGS(void)
{
	int New_TV_mode = setting->TV_mode;

	// GS Init
	gsGlobal = gsKit_init_global();

	if (New_TV_mode == TV_mode_AUTO) {         //If no forced request
		New_TV_mode = uLE_InitializeRegion();  //Let console region decide TV_mode
	}

	// Screen display mode
	TV_mode = New_TV_mode;
	switch (New_TV_mode) {
		case TV_mode_PAL:
			gsGlobal->Mode = GS_MODE_PAL;
			break;
		case TV_mode_480P:
			gsGlobal->Mode = GS_MODE_DTV_480P;
			break;
		case TV_mode_VGA:
			gsGlobal->Mode = GS_MODE_VGA_640_60;
			break;
		default:
		case TV_mode_NTSC:
			gsGlobal->Mode = GS_MODE_NTSC;
			break;
	}

	// Screen size
	applyGSParams();

	// Buffer Init
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_ON;

	// DMAC Init
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	// Screen Init (will also clear screen once)
	gsKit_init_screen(gsGlobal);
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// Screen Position Init
	initScreenParams();

	gsKit_set_display_offset(gsGlobal, setting->screen_x, setting->screen_y);

	// Screen render mode
	gsKit_mode_switch(gsGlobal, GS_ONESHOT);
}
void updateScreenMode(void)
{
	int setGS_flag = 0;
	int New_TV_mode = setting->TV_mode;

	if (New_TV_mode == TV_mode_AUTO) {         //If no forced request
		New_TV_mode = uLE_InitializeRegion();  //Let console region decide TV_mode
	}

	if (New_TV_mode != TV_mode) {
		setGS_flag = 1;
		TV_mode = New_TV_mode;

		switch (New_TV_mode) {
			case TV_mode_PAL:
				gsGlobal->Mode = GS_MODE_PAL;
				break;
			case TV_mode_480P:
				gsGlobal->Mode = GS_MODE_DTV_480P;
				break;
			case TV_mode_VGA:
				gsGlobal->Mode = GS_MODE_VGA_640_60;
				break;
			default:
			case TV_mode_NTSC:
				gsGlobal->Mode = GS_MODE_NTSC;
				break;
		}
	}  // end TV_Mode change

	// Init screen parameters
	applyGSParams();

	if (setGS_flag) {
		// Init screen modes
		gsKit_init_screen(gsGlobal);
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);
		guiAssetsInvalidate();
	}

	// Screen Position Init
	initScreenParams();

	// Init screen position
	gsKit_set_display_offset(gsGlobal, setting->screen_x, setting->screen_y);
}
//--------------------------------------------------------------
//End of file: draw_gs.c
//--------------------------------------------------------------
