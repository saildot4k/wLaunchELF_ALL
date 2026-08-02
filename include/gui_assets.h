#ifndef GUI_ASSETS_H
#define GUI_ASSETS_H

#include "gui_icons.h"

void guiAssetsUpload(void);
void guiAssetsInvalidate(void);
int guiAssetsReady(void);
int guiDrawBackground(void);
int guiDrawSplash(void);
int guiDrawFileIcon(GuiIconId icon_id, int x, int y);

#endif
