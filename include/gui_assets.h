#ifndef GUI_ASSETS_H
#define GUI_ASSETS_H

#include "gui_icons.h"

/* Higher Z values render in front when the GS depth test is enabled. */
typedef enum {
	GUI_Z_BACKGROUND = 0,
	GUI_Z_CONTENT = 1,
	GUI_Z_PROMPT = 2,
	GUI_Z_DIALOG = 3,
} GuiZLayer;

typedef enum {
	GUI_BUTTON_UP = 0,
	GUI_BUTTON_DOWN,
	GUI_BUTTON_LEFT,
	GUI_BUTTON_RIGHT,
	GUI_BUTTON_CIRCLE,
	GUI_BUTTON_CROSS,
	GUI_BUTTON_SQUARE,
	GUI_BUTTON_TRIANGLE,
	GUI_BUTTON_SELECT,
	GUI_BUTTON_START,
	GUI_BUTTON_L1,
	GUI_BUTTON_L2,
	GUI_BUTTON_L3,
	GUI_BUTTON_R1,
	GUI_BUTTON_R2,
	GUI_BUTTON_R3,
	GUI_BUTTON_AUTO,
	GUI_BUTTON_COUNT,
} GuiButtonId;

void guiAssetsUpload(void);
void guiAssetsInvalidate(void);
int guiAssetsReady(void);
int guiDrawBackground(void);
int guiDrawBackgroundRegion(int x1, int y1, int x2, int y2, GuiZLayer z);
int guiDrawSplash(void);
int guiDrawFileIcon(GuiIconId icon_id, int x, int y);
int guiButtonDrawWidth(GuiButtonId button_id);
int guiDrawButton(GuiButtonId button_id, int x, int y);

#endif
