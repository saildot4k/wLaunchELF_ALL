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

void guiAssetsUpload(void);
void guiAssetsInvalidate(void);
int guiAssetsReady(void);
int guiDrawBackground(void);
int guiDrawSplash(void);
int guiDrawFileIcon(GuiIconId icon_id, int x, int y);

#endif
