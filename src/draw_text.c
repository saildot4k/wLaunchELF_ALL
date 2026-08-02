//--------------------------------------------------------------
//File name:   draw_text.c
//--------------------------------------------------------------
#include "draw_private.h"
#include "gui_assets.h"

static GuiButtonId guiButtonForLegacyHint(unsigned int character)
{
	switch (character) {
		case '0': return GUI_BUTTON_CIRCLE;
		case '1': return GUI_BUTTON_CROSS;
		case '2': return GUI_BUTTON_SQUARE;
		case '3': return GUI_BUTTON_TRIANGLE;
		case ':': return GUI_BUTTON_RIGHT;
		case ';': return GUI_BUTTON_DOWN;
		case '<': return GUI_BUTTON_LEFT;
		case '=': return GUI_BUTTON_UP;
		default: return GUI_BUTTON_COUNT;
	}
}

static int guiButtonForTextHint(const char *text, GuiButtonId *button_id, int *length)
{
	static const struct {
		const char *text;
		int consumed_length;
		GuiButtonId button_id;
	} hints[] = {
		{"L1/", 2, GUI_BUTTON_L1},
		{"L2/", 2, GUI_BUTTON_L2},
		{"L1:", 2, GUI_BUTTON_L1},
		{"L2:", 2, GUI_BUTTON_L2},
		{"L3:", 2, GUI_BUTTON_L3},
		{"R1:", 2, GUI_BUTTON_R1},
		{"R2:", 2, GUI_BUTTON_R2},
		{"R3:", 2, GUI_BUTTON_R3},
		{"Select:", 6, GUI_BUTTON_SELECT},
		{"Sel:", 3, GUI_BUTTON_SELECT},
		{"Start:", 5, GUI_BUTTON_START},
	};
	int index;

	for (index = 0; index < (int)(sizeof(hints) / sizeof(hints[0])); index++) {
		if (!strncmp(text, hints[index].text, strlen(hints[index].text))) {
			*button_id = hints[index].button_id;
			*length = hints[index].consumed_length;
			return 1;
		}
	}

	return 0;
}

static int drawGuiButtonHint(GuiButtonId button_id, int x, int y, int draw)
{
	return !draw || guiDrawButton(button_id, x, y);
}

//The font file ELISA100.FNT is needed to display MC save titles in japanese
//and the arrays defined here are needed to find correct data in that file
const u16 font404[] = {
    0xA2AF, 11,
    0xA2C2, 8,
    0xA2D1, 11,
    0xA2EB, 7,
    0xA2FA, 4,
    0xA3A1, 15,
    0xA3BA, 7,
    0xA3DB, 6,
    0xA3FB, 4,
    0xA4F4, 11,
    0xA5F7, 8,
    0xA6B9, 8,
    0xA6D9, 38,
    0xA7C2, 15,
    0xA7F2, 13,
    0xA8C1, 720,
    0xCFD4, 43,
    0xF4A5, 1030,
    0, 0};

// ASCII��SJIS�̕ϊ��p�z��
static const u8 sjis_lookup_81[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x10
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x20
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x30
    ' ', ',', '.', ',', '.', 0xFF, ':', ';', '?', '!', 0xFF, 0xFF, '\xff', '`', 0xFF, '^',              // 0x40
    0xFF, '_', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '0', 0xFF, '-', '-', 0xFF, 0xFF,      // 0x50
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\'', '\'', '"', '"', '(', ')', 0xFF, 0xFF, '[', ']', '{',         // 0x60
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '+', '-', 0xFF, '*', 0xFF,     // 0x70
    '/', '=', 0xFF, '<', '>', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\xff', 0xFF, 0xFF, '\xff', 0xFF,        // 0x80
    '$', 0xFF, 0xFF, '%', '#', '&', '*', '@', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,        // 0x90
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xA0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xB0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '&', '|', '!', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,     // 0xC0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xD0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xE0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xF0
};
static const u8 sjis_lookup_82[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x10
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x20
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x30
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '0',   // 0x40
    '1', '2', '3', '4', '5', '6', '7', '8', '9', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,           // 0x50
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',                  // 0x60
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,            // 0x70
    0xFF, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',                 // 0x80
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,             // 0x90
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xA0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xB0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xC0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xD0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xE0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xF0
};
int loadFont(char *path_arg)
{
	int fd;

	if (strlen(path_arg) != 0) {
		char FntPath[MAX_PATH];
		genFixPath(path_arg, FntPath);
		fd = genOpen(FntPath, FIO_O_RDONLY);
		if (fd < 0) {
			genClose(fd);
			goto use_default;
		}  // end if failed open file
		genLseek(fd, 0, SEEK_SET);
		if (genLseek(fd, 0, SEEK_END) > 4700) {
			genClose(fd);
			goto use_default;
		}
		genLseek(fd, 0, SEEK_SET);
		u8 FontHeader[100];
		genRead(fd, FontHeader, 100);
		if ((FontHeader[0] == 0x00) &&
		    (FontHeader[1] == 0x02) &&
		    (FontHeader[70] == 0x60) &&
		    (FontHeader[72] == 0x60) &&
		    (FontHeader[83] == 0x90)) {
			genLseek(fd, 1018, SEEK_SET);
			memset(FontBuffer, 0, 32 * 16);
			genRead(fd, FontBuffer + 32 * 16, 224 * 16);  //.fnt files skip 1st 32 chars
			genClose(fd);
			return 1;
		} else {  // end if good fnt file
			genClose(fd);
			goto use_default;
		}     // end else bad fnt file
	} else {  // end if external font file
	use_default:
		memcpy(FontBuffer, &font_uLE, 4096);
	}  // end else build-in font
	return 0;
}
void drawChar(unsigned int c, int x, int y, u64 colour)
{
	int i, j, pixBase, pixMask;
	u8 *cm;

	updateScr_1 = 1;

	if (c >= FONT_COUNT)
		c = '_';
	if (c > 0xFF)                  //if char is beyond normal ascii range
		cm = &font_uLE[c * 16];    //  cm points to special char def in default font
	else                           //else char is inside normal ascii range
		cm = &FontBuffer[c * 16];  //  cm points to normal char def in active font

	pixMask = 0x80;
	for (i = 0; i < 8; i++) {  //for i == each pixel column
		pixBase = -1;
		for (j = 0; j < 16; j++) {                     //for j == each pixel row
			if ((pixBase < 0) && (cm[j] & pixMask)) {  //if start of sequence
				pixBase = j;
			} else if ((pixBase > -1) && !(cm[j] & pixMask)) {  //if end of sequence
				gsKit_prim_sprite(gsGlobal, x + i, y + pixBase - 1, x + i + 1, y + j - 1, 1, colour);
				pixBase = -1;
			}
		}                  //ends for j == each pixel row
		if (pixBase > -1)  //if end of sequence including final row
			gsKit_prim_sprite(gsGlobal, x + i, y + pixBase - 1, x + i + 1, y + j - 1, 1, colour);
		pixMask >>= 1;
	}  //ends for i == each pixel column
}
void drawChar2(int n, int x, int y, u64 colour)
{
	unsigned int i, j;
	u8 b;

	updateScr_1 = 1;

	for (i = 0; i < 8; i++) {
		b = elisaFnt[n + i];
		for (j = 0; j < 8; j++) {
			if (b & 0x80) {
				gsKit_prim_sprite(gsGlobal, x + j, y + i * 2 - 2, x + j + 1, y + i * 2, 1, colour);
			}
			b = b << 1;
		}
	}
}
int printXY(const char *s, int x, int y, u64 colour, int draw, int space)
{
	unsigned int c1, c2;
	GuiButtonId button_id;
	int button_length;
	int i;
	int text_spacing = 8;

	if (space > 0) {
		while ((strlen(s) * text_spacing) > space)
			if (--text_spacing <= 5)
				break;
	} else {
		while ((strlen(s) * text_spacing) > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH * 2)
			if (--text_spacing <= 5)
				break;
	}

	i = 0;
	while ((c1 = (unsigned char)s[i++]) != 0) {
		if (c1 != 0xFF) {  // Normal character
			if (guiButtonForTextHint(&s[i - 1], &button_id, &button_length) &&
			    drawGuiButtonHint(button_id, x, y, draw)) {
				x += guiButtonDrawWidth(button_id);
				i += button_length - 1;
				if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
					break;
				continue;
			}
			if (draw)
				drawChar(c1, x, y, colour);
			x += text_spacing;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
			continue;
		}  //End if for normal character
		// Here we got a sequence starting with 0xFF ('\xff')
		if ((c2 = (unsigned char)s[i++]) == 0)
			break;
		if ((c2 < '0') || (c2 > '='))
			continue;
		button_id = guiButtonForLegacyHint(c2);
		if (button_id != GUI_BUTTON_COUNT && drawGuiButtonHint(button_id, x, y, draw)) {
			x += guiButtonDrawWidth(button_id);
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
			continue;
		}
		c1 = (c2 - '0') * 2 + 0x100;
		if (draw) {
			//expand sequence �0=Circle  �1=Cross  �2=Square  �3=Triangle  �4=FilledBox
			//"\xff:"=Pad_Right  "\xff;"=Pad_Down  "\xff<"=Pad_Left  "\xff="=Pad_Up
			drawChar(c1, x, y, colour);
			x += 8;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
			drawChar(c1 + 1, x, y, colour);
			x += 8;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
		}
	}  // ends while(1)
	return x;
}
int printXY_sjis(const unsigned char *s, int x, int y, u64 colour, int draw)
{
	int n;
	u8 ascii;
	u16 code;
	GuiButtonId button_id;
	int i, j, tmp;

	i = 0;
	while (s[i]) {
		if ((s[i] & 0x80) && s[i + 1]) {  //we have top bit and some more char ?
			// SJIS
			code = s[i++];
			code = (code << 8) + s[i++];

			switch (code) {
				// Circle == "��"
				case 0x819B:
					button_id = GUI_BUTTON_CIRCLE;
					if (drawGuiButtonHint(button_id, x, y, draw)) {
						x += guiButtonDrawWidth(button_id);
						break;
					}
					if (draw) {
						drawChar(0x100, x, y, colour);
						drawChar(0x101, x + 8, y, colour);
					}
					x += 16;
					break;
				// Cross == "\xff~"
				case 0x817E:
					button_id = GUI_BUTTON_CROSS;
					if (drawGuiButtonHint(button_id, x, y, draw)) {
						x += guiButtonDrawWidth(button_id);
						break;
					}
					if (draw) {
						drawChar(0x102, x, y, colour);
						drawChar(0x103, x + 8, y, colour);
					}
					x += 16;
					break;
				// Square == "��"
				case 0x81A0:
					button_id = GUI_BUTTON_SQUARE;
					if (drawGuiButtonHint(button_id, x, y, draw)) {
						x += guiButtonDrawWidth(button_id);
						break;
					}
					if (draw) {
						drawChar(0x104, x, y, colour);
						drawChar(0x105, x + 8, y, colour);
					}
					x += 16;
					break;
				// Triangle == "��"
				case 0x81A2:
					button_id = GUI_BUTTON_TRIANGLE;
					if (drawGuiButtonHint(button_id, x, y, draw)) {
						x += guiButtonDrawWidth(button_id);
						break;
					}
					if (draw) {
						drawChar(0x106, x, y, colour);
						drawChar(0x107, x + 8, y, colour);
					}
					x += 16;
					break;
				// FilledBox == "��"
				case 0x81A1:
					if (draw) {
						drawChar(0x108, x, y, colour);
						drawChar(0x109, x + 8, y, colour);
					}
					x += 16;
					break;
				default:
					if (elisaFnt != NULL) {  // elisa font is available ?
						tmp = y;
						if (code <= 0x829A)
							tmp++;
						// SJIS����EUC�ɕϊ�
						if (code >= 0xE000)
							code -= 0x4000;
						code = ((((code >> 8) & 0xFF) - 0x81) << 9) + (code & 0x00FF);
						if ((code & 0x00FF) >= 0x80)
							code--;
						if ((code & 0x00FF) >= 0x9E)
							code += 0x62;
						else
							code -= 0x40;
						code += 0x2121 + 0x8080;

						// EUC����b�����t�H���g�̔ԍ��𐶐�
						n = (((code >> 8) & 0xFF) - 0xA1) * (0xFF - 0xA1) + (code & 0xFF) - 0xA1;
						j = 0;
						while (font404[j]) {
							if (code >= font404[j]) {
								if (code <= font404[j] + font404[j + 1] - 1) {
									n = -1;
									break;
								} else {
									n -= font404[j + 1];
								}
							}
							j += 2;
						}
						n *= 8;

						if (n >= 0 && n <= 55008) {
							if (draw)
								drawChar2(n, x, tmp, colour);
							x += 9;
						} else {
							if (draw)
								drawChar('_', x, y, colour);
							x += 8;
						}
					} else {  //elisa font is not available
						ascii = 0xFF;
						if (code >> 8 == 0x81)
							ascii = sjis_lookup_81[code & 0x00FF];
						else if (code >> 8 == 0x82)
							ascii = sjis_lookup_82[code & 0x00FF];
						if (ascii != 0xFF) {
							if (draw)
								drawChar((char)ascii, x, y, colour);
						} else {
							if (draw)
								drawChar('_', x, y, colour);
						}
						x += 8;
					}
					break;
			}
		} else {  //First char does not have top bit set or no following char
			if (draw)
				drawChar(s[i], x, y, colour);
			i++;
			x += 8;
		}
		if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH) {
			//x=16; y=y+8;
			return x;
		}
	}
	return x;
}
char *transcpy_sjis(char *d, const unsigned char *s)
{
	u8 ascii;
	u16 code1, code2;
	int i, j;

	for (i = 0, j = 0; s[i];) {
		code1 = s[i++];
		if ((code1 & 0x80) && s[i]) {  //we have top bit and some more char (SJIS) ?
			// SJIS
			code2 = s[i++];
			ascii = 0xFF;
			if (code1 == 0x81)
				ascii = sjis_lookup_81[code2];
			else if (code1 == 0x82)
				ascii = sjis_lookup_82[code2];
			if (ascii != 0xFF) {
				d[j++] = (char)ascii;
			} else {
				d[j++] = '_';
			}
		} else {  //First char lacks top bit set or no following char (non-SJIS)
			d[j++] = (char)code1;
		}
	}             //ends for
	d[j] = '\0';  //terminate result string
	return d;
}
//--------------------------------------------------------------
//End of file: draw_text.c
//--------------------------------------------------------------
