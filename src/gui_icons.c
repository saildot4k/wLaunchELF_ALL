#include "gui_icons.h"
#include "gui_hdd0_format.h"
#include "psu_types.h"

#define PSU_ICON_CACHE_SIZE 32
#define PSU_HEADER_SIZE ((int)sizeof(psu_header))

typedef struct {
	u32 key;
	u32 size_low;
	u32 size_high;
	u64 modified;
	int result;
	int valid;
} PsuIconCacheEntry;

static PsuIconCacheEntry psu_icon_cache[PSU_ICON_CACHE_SIZE];
static int psu_icon_cache_next;

static const char *icon_asset_names[GUI_ICON_COUNT] = {
    "dir",
    "dir_system",
    "file",
    "file_archive",
    "file_archive_sas",
    "file_executable_ps2",
    "file_executable_windows",
    "file_font",
    "file_game_disc",
    "file_game_disc_compressed",
    "file_game_disc_sheet",
    "file_game_disk",
    "file_game_floppy",
    "file_game_rom",
    "file_game_tape",
    "file_icon_ps2",
    "file_language",
    "file_music",
    "file_patch",
    "file_picture",
    "file_save_other",
    "file_save_ps1",
    "file_save_ps2",
    "file_script",
    "file_system",
    "file_text",
    "file_video",
    "file_video_subtitles",
    "file_vmc_other",
    "file_vmc_ps1",
    "file_vmc_ps2",
};

static const char *archive_exts[] = {
    "7Z", "APK", "ARJ", "BZ", "CAB", "DEB", "DMG", "GZ",
    "JAR", "LHA", "LZ", "LZ4", "LZH", "LZMA", "LZO", "LZX", "MSI",
    "PAK", "PBP", "PKG", "RAR", "RPM", "SIT", "SITX", "TAR",
    "TGZ", "WAD", "WIM", "XZ", "Z", "ZIP", "ZST", NULL};

static const char *executable_ps2_exts[] = {
    "ELF", "ERX", "IRX", "KELF", "KIRX", "PSX", "XIN", "XLF", NULL};

static const char *executable_windows_exts[] = {
    "COM", "DLL", "EXE", "SFX", NULL};

static const char *font_exts[] = {
    "FNT", "OTF", "TTF", NULL};

static const char *game_disc_exts[] = {
    "BIN", "BWT", "CDI", "GCM", "GI", "IMG", "ISO", "MDF",
    "NRG", "PDI", "VCD", "WUA", "XGD", "XISO", NULL};

static const char *game_disc_compressed_exts[] = {
    "CCI", "CHD", "CISO", "CSO", "DAX", "ECM", "GCZ", "JSO",
    "NKIT", "RVZ", "ZISO", "ZSO", NULL};

static const char *game_disc_sheet_exts[] = {
    "BWS", "CCD", "CU2", "CUE", "GDI", "MDS", NULL};

static const char *game_disk_exts[] = {
    "APA", "DD", "DSK", "HVF", "IMGC", "QCOW", "QCOW2", "VHD",
    "VHDX", "WBFS", NULL};

static const char *game_floppy_exts[] = {
    "ADF", "FDS", "FLP", "IMA", "IPF", NULL};

static const char *game_rom_exts[] = {
    "68K", "CIA", "GB", "GBA", "GBC", "GEN", "GG", "MS",
    "N64", "NDS", "NES", "NGC", "NGP", "NSP", "PCE", "SFC",
    "SMC", "SMD", "SMS", "V64", "VB", "WS", "XCI", "Z3DSX",
    "Z64", "ZCCI", "ZCXI", NULL};

static const char *icon_ps2_exts[] = {
    "ICN", "ICO", NULL};

static const char *language_exts[] = {
    "LANG", "LNG", NULL};

static const char *music_exts[] = {
    "AAC", "ADP", "ADPCM", "AIFF", "AT3", "ATRAC", "FLAC", "M4A",
    "MP3", "OGA", "OGG", "OPUS", "PCM", "SS2", "TTA", "VAG",
    "WAV", "WAVE", "WMA", "XA", NULL};

static const char *patch_exts[] = {
    "CHT", "IPS", "PPF", "UPS", "XDELTA", NULL};

static const char *picture_exts[] = {
    "AVIF", "BMP", "GIF", "GIM", "HEIF", "JP2", "JPEG", "JPG",
    "MPO", "PCX", "PNG", "SVG", "TGA", "TIF", "TIFF", "TIM",
    "TIM2", "TIM3", "WEBP", NULL};

static const char *save_other_exts[] = {
    "GCI", "SAV", "SRM", NULL};

static const char *save_ps1_exts[] = {
    "MCS", NULL};

static const char *save_ps2_exts[] = {
    "CBS", "MAX", "NPO", "PSU", "SPS", "XPS", NULL};

static const char *script_exts[] = {
    "CMD", "JS", "LUA", "PBT", "PS", "RSH", "SH", NULL};

static const char *system_exts[] = {
    "ARG", "CFG", "CNF", "IFO", "INI", "SFO", "SYS", "TOML", "YAML", "YML", NULL};

static const char *text_exts[] = {
    "C", "CPP", "CS", "DOC", "DOCX", "H", "HTM", "HTML", "MD", "MHT", "NFO",
    "PDF", "RTF", "TXT", "XHTML", "XML", NULL};

static const char *video_exts[] = {
    "3GP", "AVI", "BIK", "DIVX", "FLV", "GVI", "M2TS", "M4P",
    "MKV", "MOV", "MP1", "MP2", "MP4", "MPC", "MPEG", "MPG",
    "MTS", "PSS", "QT", "RM", "RMVB", "TS", "VOB", "WEBM",
    "WMV", "XMV", "XVID", NULL};

static const char *video_subtitles_exts[] = {
    "ASS", "SAA", "SBV", "SRT", "SUB", "TTML", "VTT", NULL};

static const char *vmc_other_exts[] = {
    "DCM", "DSV", "EEP", "GCP", "MPK", "VMU", NULL};

static const char *vmc_ps1_exts[] = {
    "DDF", "GME", "MC1", "MCR", "VGS", "VM1", "VMC", "VMP", NULL};

static const char *vmc_ps2_exts[] = {
    "MC2", "MCD", "PS2", "VM2", "VME", NULL};

static int entryIsDirectory(const FILEINFO *file)
{
	return (file != NULL && (file->stats.AttrFile & sceMcFileAttrSubdir));
}

static int matchesAnyExt(const char *name, const char *const *exts)
{
	int i;

	for (i = 0; exts[i] != NULL; i++) {
		if (genCmpFileExt(name, exts[i]))
			return TRUE;
	}
	return FALSE;
}

static int isSystemDirectoryName(const char *name)
{
	static const char *system_dirs[] = {
	    "BADATA-SYSTEM",
	    "BAEXEC-DVDPLAYER",
	    "BAEXEC-OPENTUNA",
	    "BAEXEC-SYSTEM",
	    "BCDATA-SYSTEM",
	    "BCEXEC-SYSTEM",
	    "BEDATA-SYSTEM",
	    "BEEXEC-DVDPLAYER",
	    "BEEXEC-OPENTUNA",
	    "BEEXEC-SYSTEM",
	    "BIDATA-SYSTEM",
	    "BIEXEC-DVDPLAYER",
	    "BIEXEC-OPENTUNA",
	    "BIEXEC-SYSTEM",
	    "BM",
	    "BOOT",
	    "BREXEC-SYSTEM",
	    "FORTUNA",
	    "MATRIX",
	    "OPENTUNA",
	    "SYS-CONF",
	    "TOXIC",
	    NULL};
	int i;

	if (name == NULL || !strcmp(name, ".."))
		return FALSE;
	if (strlen(name) >= 4 &&
	    wle_ascii_tolower((unsigned char)name[0]) == 's' &&
	    wle_ascii_tolower((unsigned char)name[1]) == 'y' &&
	    wle_ascii_tolower((unsigned char)name[2]) == 's' &&
	    name[3] == '_')
		return TRUE;
	for (i = 0; system_dirs[i] != NULL; i++) {
		if (!stricmp(name, system_dirs[i]))
			return TRUE;
	}
	return FALSE;
}

static u32 hashString(u32 hash, const char *text)
{
	while (text != NULL && *text != '\0') {
		hash ^= (unsigned char)*text++;
		hash *= 16777619u;
	}
	return hash;
}

static u32 psuCacheKey(const char *path, const FILEINFO *file)
{
	u32 hash = 2166136261u;

	hash = hashString(hash, path);
	hash = hashString(hash, "/");
	hash = hashString(hash, file->name);
	return hash;
}

static u64 fileModifiedKey(const FILEINFO *file)
{
	u64 modified = 0;

	memcpy(&modified, &file->stats._Modify, sizeof(modified));
	return modified;
}

static int psuCacheLookup(u32 key, const FILEINFO *file, int *result)
{
	int i;

	for (i = 0; i < PSU_ICON_CACHE_SIZE; i++) {
		if (psu_icon_cache[i].valid &&
		    psu_icon_cache[i].key == key &&
		    psu_icon_cache[i].size_low == file->stats.FileSizeByte &&
		    psu_icon_cache[i].size_high == file->stats.Reserve2 &&
		    psu_icon_cache[i].modified == fileModifiedKey(file)) {
			*result = psu_icon_cache[i].result;
			return TRUE;
		}
	}
	return FALSE;
}

static void psuCacheStore(u32 key, const FILEINFO *file, int result)
{
	PsuIconCacheEntry *entry;

	entry = &psu_icon_cache[psu_icon_cache_next];
	entry->key = key;
	entry->size_low = file->stats.FileSizeByte;
	entry->size_high = file->stats.Reserve2;
	entry->modified = fileModifiedKey(file);
	entry->result = result;
	entry->valid = TRUE;
	psu_icon_cache_next = (psu_icon_cache_next + 1) % PSU_ICON_CACHE_SIZE;
}

static int buildFileAccessPath(const char *path, const FILEINFO *file, char *out, size_t out_size)
{
	if (path == NULL || file == NULL || out == NULL || out_size == 0)
		return -1;
	if (path[0] == '\0' || !strcmp(file->name, ".."))
		return -1;

	if (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':' && path[5] == '/' && path[6] != '\0') {
		char party[MAX_NAME];
		int pfs_ix;

		if (getHddParty(path, file, party, out) < 0)
			return -1;
		pfs_ix = mountParty(party);
		if (pfs_ix < 0)
			return -1;
		out[3] = pfs_ix + '0';
		return 0;
	}

#ifdef DVRP
	if (console_is_PSX && !strncmp(path, "dvr_hdd", 7) && strcmp(path, "dvr_hdd0:/")) {
		char party[MAX_NAME];
		int pfs_ix;

		if (getHddDVRPParty(path, file, party, out) < 0)
			return -1;
		pfs_ix = mountDVRPParty(party);
		if (pfs_ix < 0)
			return -1;
		out[7] = pfs_ix + '0';
		return 0;
	}
#endif

	if (snprintf(out, out_size, "%s%s", path, file->name) >= (int)out_size)
		return -1;
	return 0;
}

static int psuContainsTitleCfgAtPath(const char *path)
{
	psu_header header;
	char name[sizeof(header.name) + 1];
	int fd, i, count, read_size, payload_skip;
	int result = FALSE;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return FALSE;

	read_size = genRead(fd, (void *)&header, sizeof(header));
	if (read_size != PSU_HEADER_SIZE)
		goto finish;

	count = header.size;
	for (i = 0; i < count; i++) {
		read_size = genRead(fd, (void *)&header, sizeof(header));
		if (read_size != PSU_HEADER_SIZE)
			goto finish;

		memcpy(name, header.name, sizeof(header.name));
		name[sizeof(header.name)] = '\0';
		if (!stricmp(name, "title.cfg")) {
			result = TRUE;
			goto finish;
		}

		if (header.size != 0) {
			payload_skip = (header.size + 0x3FF) & -0x400;
			genLseek(fd, payload_skip, SEEK_CUR);
		}
	}

finish:
	genClose(fd);
	return result;
}

static int psuContainsTitleCfg(const char *path, const FILEINFO *file)
{
	char access_path[MAX_PATH];
	u32 key;
	int result;

	if (!genCmpFileExt(file->name, "PSU"))
		return FALSE;

	key = psuCacheKey(path, file);
	if (psuCacheLookup(key, file, &result))
		return result;

	result = FALSE;
	if (buildFileAccessPath(path, file, access_path, sizeof(access_path)) == 0)
		result = psuContainsTitleCfgAtPath(access_path);
	psuCacheStore(key, file, result);
	return result;
}

const char *guiIconAssetName(GuiIconId icon_id)
{
	if (icon_id < 0 || icon_id >= GUI_ICON_COUNT)
		return icon_asset_names[GUI_ICON_FILE];
	return icon_asset_names[icon_id];
}

GuiIconId guiIconForFileEntry(const char *path, const FILEINFO *file)
{
	const char *name;

	if (file == NULL)
		return GUI_ICON_FILE;

	name = file->name;
	if (entryIsDirectory(file))
		return isSystemDirectoryName(name) ? GUI_ICON_DIR_SYSTEM : GUI_ICON_DIR;

	if (genCmpFileExt(name, "PSU"))
		return psuContainsTitleCfg(path, file) ? GUI_ICON_FILE_ARCHIVE_SAS : GUI_ICON_FILE_SAVE_PS2;
	if (matchesAnyExt(name, executable_ps2_exts))
		return GUI_ICON_FILE_EXECUTABLE_PS2;
	if (matchesAnyExt(name, executable_windows_exts))
		return GUI_ICON_FILE_EXECUTABLE_WINDOWS;
	if (matchesAnyExt(name, archive_exts))
		return GUI_ICON_FILE_ARCHIVE;
	if (matchesAnyExt(name, font_exts))
		return GUI_ICON_FILE_FONT;
	if (matchesAnyExt(name, game_disc_compressed_exts))
		return GUI_ICON_FILE_GAME_DISC_COMPRESSED;
	if (matchesAnyExt(name, game_disc_sheet_exts))
		return GUI_ICON_FILE_GAME_DISC_SHEET;
	if (matchesAnyExt(name, game_disc_exts))
		return GUI_ICON_FILE_GAME_DISC;
	if (matchesAnyExt(name, game_disk_exts))
		return GUI_ICON_FILE_GAME_DISK;
	if (matchesAnyExt(name, game_floppy_exts))
		return GUI_ICON_FILE_GAME_FLOPPY;
	if (matchesAnyExt(name, game_rom_exts))
		return GUI_ICON_FILE_GAME_ROM;
	if (genCmpFileExt(name, "TAP"))
		return GUI_ICON_FILE_GAME_TAPE;
	if (matchesAnyExt(name, icon_ps2_exts))
		return GUI_ICON_FILE_ICON_PS2;
	if (matchesAnyExt(name, language_exts))
		return GUI_ICON_FILE_LANGUAGE;
	if (matchesAnyExt(name, music_exts))
		return GUI_ICON_FILE_MUSIC;
	if (matchesAnyExt(name, patch_exts))
		return GUI_ICON_FILE_PATCH;
	if (matchesAnyExt(name, picture_exts))
		return GUI_ICON_FILE_PICTURE;
	if (matchesAnyExt(name, save_ps1_exts))
		return GUI_ICON_FILE_SAVE_PS1;
	if (matchesAnyExt(name, save_ps2_exts))
		return GUI_ICON_FILE_SAVE_PS2;
	if (matchesAnyExt(name, save_other_exts))
		return GUI_ICON_FILE_SAVE_OTHER;
	if (matchesAnyExt(name, script_exts))
		return GUI_ICON_FILE_SCRIPT;
	if (matchesAnyExt(name, system_exts))
		return GUI_ICON_FILE_SYSTEM;
	if (matchesAnyExt(name, video_subtitles_exts))
		return GUI_ICON_FILE_VIDEO_SUBTITLES;
	if (matchesAnyExt(name, video_exts))
		return GUI_ICON_FILE_VIDEO;
	if (matchesAnyExt(name, vmc_ps1_exts))
		return GUI_ICON_FILE_VMC_PS1;
	if (matchesAnyExt(name, vmc_ps2_exts))
		return GUI_ICON_FILE_VMC_PS2;
	if (matchesAnyExt(name, vmc_other_exts))
		return GUI_ICON_FILE_VMC_OTHER;
	if (matchesAnyExt(name, text_exts))
		return GUI_ICON_FILE_TEXT;

	return GUI_ICON_FILE;
}

void guiIconPrimeDirectory(const char *path, const FILEINFO *files, int nfiles)
{
	int i;

	if (files == NULL)
		return;
	for (i = 0; i < nfiles; i++) {
		if (!entryIsDirectory(&files[i]) && genCmpFileExt(files[i].name, "PSU"))
			(void)guiIconForFileEntry(path, &files[i]);
	}
}

GuiLegacyFontIcon guiLegacyFontIconForGuiIcon(GuiIconId icon_id, int marked)
{
	GuiLegacyFontIcon icon;

	if (icon_id == GUI_ICON_DIR || icon_id == GUI_ICON_DIR_SYSTEM) {
		icon.iconbase = ICON_FOLDER;
		icon.color_id = COLOR_GRAPH1;
	} else {
		icon.iconbase = ICON_FILE;
		switch (icon_id) {
			case GUI_ICON_FILE_EXECUTABLE_PS2:
				icon.color_id = COLOR_GRAPH2;
				break;
			case GUI_ICON_FILE_LANGUAGE:
			case GUI_ICON_FILE_PATCH:
			case GUI_ICON_FILE_SCRIPT:
			case GUI_ICON_FILE_SYSTEM:
			case GUI_ICON_FILE_TEXT:
				icon.color_id = COLOR_GRAPH4;
				break;
			default:
				icon.color_id = COLOR_GRAPH3;
				break;
		}
	}

	if (marked)
		icon.iconbase += 2;
	return icon;
}
