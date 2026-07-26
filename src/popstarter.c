//--------------------------------------------------------------
// File name:   popstarter.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "main_gameid.h"
#include "main_history.h"

enum POPSTARTER_RESULT {
	POPSTARTER_OK = 1,
	POPSTARTER_ERR_UNSUPPORTED_PATH = -10,
	POPSTARTER_ERR_INVALID_VCD = -11,
	POPSTARTER_ERR_NOT_VCD = -12,
	POPSTARTER_ERR_OPEN_VCD = -13,
	POPSTARTER_ERR_OPEN_POPSTARTER = -15,
	POPSTARTER_ERR_INVALID_POPSTARTER = -17
};

#define POPSTARTER_MAX_CANDIDATES 4
#define POPSTARTER_VCD_IMAGE_OFFSET 0x100000
#define POPSTARTER_VCD_RAW_SECTOR_SIZE 2352
#define POPSTARTER_CD_SECTOR_DATA_SIZE 2048
#define POPSTARTER_ISO_PVD_LBA 16
#define POPSTARTER_ISO_PVD_SEARCH_SECTORS 16
#define POPSTARTER_ISO_ROOT_RECORD_OFFSET 156
#define POPSTARTER_ISO_VOLUME_TIMESTAMP_OFFSET 0x32D
#define POPSTARTER_ISO_VOLUME_TIMESTAMP_LEN 16
#define POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS 64

typedef struct
{
	const char *volume_timestamp;
	const char *game_id;
} POPSTARTER_PS1_GENERIC_GAME_ID;

/*
 * PVD timestamp mappings for PS1 discs whose boot executable is generic
 * PSX.EXE or whose SYSTEM.CNF is missing.
 * Derived from OSDMenu common/include/game_id_table.h (AFL-3.0),
 * which credits TonyHax International, GameDB-PSX, and manual verification.
 */
static const POPSTARTER_PS1_GENERIC_GAME_ID ps1_generic_game_ids[] = {
    {"1994111009000000", "SLPS_000.01"},
    {"1994110702000000", "SLPS_000.02"},
    {"1994102615231700", "SLPS_000.03"},
    {"1994110218594700", "SLPS_000.04"},
    {"1995030218052000", "SLPS_000.04"},
    {"1994110722360400", "SLPS_000.05"},
    {"1994120610494900", "SLPS_000.05"},
    {"1994110407000000", "SLPS_000.06"},
    {"1994111419300000", "SLPS_000.07"},
    {"1994121808190700", "SLPS_000.08"},
    {"1994121917000000", "SLPS_000.09"},
    {"1995052918000000", "SLPS_000.10"},
    {"1994110220020600", "SLPS_000.11"},
    {"1994121518000000", "SLPS_000.13"},
    {"1994103000000000", "SLPS_000.14"},
    {"1994101813262400", "SLPS_000.15"},
    {"1994112617300000", "SLPS_000.16"},
    {"1994121517300000", "SLPS_000.16"},
    {"1994111013000000", "SLPS_000.17"},
    {"1994111522183200", "SLPS_000.18"},
    {"1994112918000000", "SLPS_000.19"},
    {"1994111721302100", "SLPS_000.20"},
    {"1994100617242100", "SLPS_000.21"},
    {"1995030215000000", "SLPS_000.22"},
    {"1994122718351900", "SLPS_000.23"},
    {"1994092920284600", "SLPS_000.24"},
    {"1994113012000000", "SLPS_000.25"},
    {"1995012512000000", "SLPS_000.25"},
    {"1995041921063500", "SLPS_000.26"},
    {"1994121500000000", "SLPS_000.27"},
    {"1994121017582300", "SLPS_000.28"},
    {"1995022623000000", "SLPS_000.29"},
    {"1995050116000000", "SLPS_000.30"},
    {"1995060613000000", "SLPS_000.30"},
    {"1995021802000000", "SLPS_000.31"},
    {"1995021615022900", "SLPS_000.32"},
    {"1995080809000000", "SLPS_000.33"},
    {"1995100209000000", "SLPS_000.33"},
    {"1995071821394900", "SLPS_000.34"},
    {"1995042506300000", "SLPS_000.35"},
    {"1995011411551700", "SLPS_000.37"},
    {"1995041311392800", "SLPS_000.38"},
    {"1995031205000000", "SLPS_000.40"},
    {"1995061612000000", "SLPS_000.40"},
    {"1995040509000000", "SLPS_000.41"},
    {"1995052612000000", "SLPS_000.43"},
    {"1995042500000000", "SLPS_000.44"},
    {"1995033100003000", "SLPS_000.47"},
    {"1995041400000000", "SLPS_000.48"},
    {"1995050413421800", "SLPS_000.50"},
    {"1995040509595900", "SLPS_000.51"},
    {"1995030103150000", "SLPS_000.52"},
    {"1995100409235300", "SLPS_000.53"},
    {"1995060504013600", "SLPS_000.55"},
    {"1995060319142200", "SLPS_000.55"},
    {"1995060402110800", "SLPS_000.55"},
    {"1995081612000000", "SLPS_000.59"},
    {"1995051201000000", "SLPS_000.60"},
    {"1995051700000000", "SLPS_000.61"},
    {"1995051002471900", "SLPS_000.63"},
    {"1995083112000000", "SLPS_000.65"},
    {"1995111700000000", "SLPS_000.65"},
    {"1996033100000000", "SLPS_000.65"},
    {"1995051816000000", "SLPS_000.66"},
    {"1995061418000000", "SLPS_000.67"},
    {"1995061911303400", "SLPS_000.68"},
    {"1995072800300000", "SLPS_000.68"},
    {"1995061207000000", "SLPS_000.69"},
    {"1995062922000000", "SLPS_000.70"},
    {"1995040719355400", "SLPS_000.71"},
    {"1995061806364400", "SLPS_000.73"},
    {"1995051015300000", "SLPS_000.77"},
    {"1995070302000000", "SLPS_000.78"},
    {"1995070523450000", "SLPS_000.83"},
    {"1995072522004900", "SLPS_000.85"},
    {"1995070613170000", "SLPS_000.88"},
    {"1995082517551900", "SLPS_000.89"},
    {"1995082109402500", "SLPS_000.90"},
    {"1995053117000000", "SLPS_000.91"},
    {"1995081100000000", "SLPS_000.92"},
    {"1995071011035200", "SLPS_000.93"},
    {"1995090510000000", "SLPS_000.94"},
    {"1995083123000000", "SLPS_000.94"},
    {"1995100601300000", "SLPS_000.99"},
    {"1995081001450000", "SLPS_001.01"},
    {"1995080316000000", "SLPS_001.03"},
    {"1995081020000000", "SLPS_001.04"},
    {"1995090722000000", "SLPS_001.08"},
    {"1995090516062841", "SLPS_001.13"},
    {"1995082016003000", "SLPS_001.28"},
    {"1995102101350000", "SLPS_001.33"},
    {"1995102102521200", "SLPS_001.33"},
    {"1995102105003200", "SLPS_001.33"},
    {"1995100910002200", "SLPS_001.37"},
    {"1995101801325900", "SLPS_001.42"},
    {"1995113010450000", "SLPS_001.46"},
    {"1995092205430500", "SLPS_001.52"},
    {"1995121620000000", "SLPS_001.73"},
    {"1995122811000000", "SLPS_001.90"},
    {"1995111622323000", "SLPS_002.01"},
    {"1995121418400300", "SLPS_002.30"},
    {"1996010800000000", "SLPS_002.61"},
    {"1996022700000000", "SLPS_003.21"},
    {"1996020413401600", "SLPS_003.36"},
    {"1996030619500500", "SLPS_003.37"},
    {"1996072211000000", "SLPS_005.49"},
    {"1997011500000000", "SLPS_007.19"},
    {"1997031012200700", "SLPS_008.78"},
    {"1997050817540700", "SLPS_008.95"},
    {"1998061000000000", "SLPS_013.34"},
    {"1998040820350000", "SLPS_015.58"},
    {"1994112112000000", "SCPS_100.01"},
    {"1995011010000000", "SCPS_100.01"},
    {"1995030717020700", "SCPS_100.02"},
    {"1994103110000000", "SCPS_100.03"},
    {"1995022100000000", "SCPS_100.04"},
    {"1995032500000000", "SCPS_100.06"},
    {"1995032400000000", "SCPS_100.07"},
    {"1995052420065100", "SCPS_100.08"},
    {"1995061723590000", "SCPS_100.09"},
    {"1995080914422700", "SCPS_100.10"},
    {"1995071219364500", "SCPS_100.12"},
    {"1995092719000000", "SCPS_100.14"},
    {"1995103122331500", "SCPS_100.16"},
};

typedef struct
{
	char path[MAX_PATH];
} POPSTARTER_CANDIDATE;

typedef struct
{
	POPSTARTER_CANDIDATE entry[POPSTARTER_MAX_CANDIDATES];
	int count;
} POPSTARTER_CANDIDATE_LIST;

static void initPopstarterCandidates(POPSTARTER_CANDIDATE_LIST *candidates)
{
	if (candidates != NULL)
		candidates->count = 0;
}

static int addPopstarterCandidate(POPSTARTER_CANDIDATE_LIST *candidates, const char *path)
{
	if (candidates == NULL || path == NULL || path[0] == '\0' ||
	    candidates->count >= POPSTARTER_MAX_CANDIDATES)
		return 0;

	snprintf(candidates->entry[candidates->count].path, MAX_PATH, "%s", path);
	candidates->count++;
	return 1;
}

static int isHddDevicePath(const char *path)
{
	return (path != NULL &&
	        !strncmp(path, "hdd", 3) &&
	        path[3] >= '0' && path[3] <= '9' &&
	        path[4] == ':' &&
	        path[5] == '/');
}

static const char *findHddPfsSeparator(const char *path)
{
	const char *p;

	if (path == NULL)
		return NULL;

	for (p = path; *p != '\0'; p++) {
		if (p[0] == ':' &&
		    p[1] != '\0' &&
		    p[2] != '\0' &&
		    p[3] != '\0' &&
		    wle_ascii_tolower((unsigned char)p[1]) == 'p' &&
		    wle_ascii_tolower((unsigned char)p[2]) == 'f' &&
		    wle_ascii_tolower((unsigned char)p[3]) == 's' &&
		    p[4] == ':')
			return p;
	}

	return NULL;
}

static int splitHddPfsPath(const char *path, char *hdd_device, size_t hdd_device_size,
                           char *partition, size_t partition_size, const char **subpath)
{
	const char *partition_start;
	const char *pfs_sep;
	size_t partition_len;

	if (path == NULL || hdd_device == NULL || hdd_device_size < 6 ||
	    partition == NULL || partition_size == 0 || subpath == NULL)
		return 0;
	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9')
		return 0;

	if (path[4] != ':' || path[5] == '/')
		return 0;
	partition_start = path + 5;

	pfs_sep = findHddPfsSeparator(partition_start);
	if (pfs_sep == NULL)
		return 0;

	partition_len = pfs_sep - partition_start;
	if (partition_len == 0 || partition_len >= partition_size)
		return 0;

	snprintf(hdd_device, hdd_device_size, "hdd%c:", path[3]);
	memcpy(partition, partition_start, partition_len);
	partition[partition_len] = '\0';
	*subpath = pfs_sep + 5;
	return 1;
}

static int isHddPfsPath(const char *path)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	const char *subpath;

	return splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
}

static int splitPopstarterHddLaunchPath(const char *path, char *hdd_device, size_t hdd_device_size,
                                        char *partition, size_t partition_size, const char **subpath)
{
	const char *partition_start;
	size_t partition_len;

	if (path == NULL || hdd_device == NULL || hdd_device_size < 6 ||
	    partition == NULL || partition_size == 0 || subpath == NULL)
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	if (isHddDevicePath(path)) {
		memcpy(hdd_device, path, 5);
		hdd_device[5] = '\0';
		partition_start = path + 6;
		*subpath = strchr(partition_start, '/');
		if (*subpath == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;

		partition_len = *subpath - partition_start;
		if (partition_len == 0 || partition_len >= partition_size)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		memcpy(partition, partition_start, partition_len);
		partition[partition_len] = '\0';
		return POPSTARTER_OK;
	}

	if (splitHddPfsPath(path, hdd_device, hdd_device_size, partition, partition_size, subpath))
		return POPSTARTER_OK;

	return 0;
}

static int isHddPartyPath(const char *party)
{
	return (party != NULL &&
	        !strncmp(party, "hdd", 3) &&
	        party[3] >= '0' && party[3] <= '9' &&
	        party[4] == ':');
}

static int splitDevicePath(const char *path, char *device, size_t device_size, const char **suffix)
{
	const char *colon;
	size_t len;

	if (path == NULL || device == NULL || device_size == 0 || suffix == NULL)
		return 0;

	colon = strchr(path, ':');
	if (colon == NULL)
		return 0;

	len = colon - path + 1;
	if (len >= device_size)
		return 0;

	memcpy(device, path, len);
	device[len] = '\0';
	*suffix = colon + 1;
	return 1;
}

static int deviceMatchesBase(const char *device, const char *base)
{
	size_t len;

	if (device == NULL || base == NULL)
		return 0;

	len = strlen(base);
	if (strncmp(device, base, len))
		return 0;
	if (device[len] == ':')
		return 1;
	if (device[len] >= '0' && device[len] <= '9' && device[len + 1] == ':')
		return 1;
	return 0;
}

static int prefixCaseCmp(const char *value, const char *prefix, size_t prefix_len)
{
	size_t i;

	for (i = 0; i < prefix_len; i++) {
		if (value[i] == '\0')
			return 1;
		if (wle_ascii_tolower((unsigned char)value[i]) != wle_ascii_tolower((unsigned char)prefix[i]))
			return 1;
	}
	return 0;
}

static const char *getFinalNameComponent(const char *path)
{
	const char *name;
	const char *sep;

	if (path == NULL)
		return NULL;

	name = path;
	sep = strrchr(path, '/');
	if (sep != NULL)
		name = sep + 1;
	sep = strrchr(path, '\\');
	if (sep != NULL && sep + 1 > name)
		name = sep + 1;
	sep = strrchr(path, ':');
	if (sep != NULL && sep + 1 > name)
		name = sep + 1;

	return name;
}

static int buildGameIDFromPopstarterName(const char *name, char *gameID, size_t gameID_len)
{
	const char *id_name;

	id_name = getFinalNameComponent(name);
	if (id_name == NULL || id_name[0] == '\0')
		return 0;

	if (!prefixCaseCmp(id_name, "XX.", 3) ||
	    !prefixCaseCmp(id_name, "__.", 3) ||
	    !prefixCaseCmp(id_name, "PP.", 3))
		id_name += 3;

	return buildLaunchGameID(id_name, gameID, gameID_len);
}

static int isLikelyTitleID(const char *gameID)
{
	return (gameID != NULL &&
	        strlen(gameID) >= 11 &&
	        gameID[4] == '_' &&
	        (gameID[7] == '.' || gameID[8] == '.'));
}

static int isFatStylePopstarterDevice(const char *device)
{
	if (deviceMatchesBase(device, "mass") || deviceMatchesBase(device, "usb"))
		return 1;
#ifdef MMCE
	if (deviceMatchesBase(device, "mmce"))
		return 1;
#endif
#ifdef MX4SIO
	if (deviceMatchesBase(device, "mx4sio"))
		return 1;
#endif
#ifdef EXFAT
	if (deviceMatchesBase(device, "ata"))
		return 1;
#endif
	return 0;
}

static int startsWithPopsDir(const char *suffix, const char **relative_vcd)
{
	if (suffix == NULL || relative_vcd == NULL)
		return 0;

	if (*suffix == '/')
		suffix++;
	if (prefixCaseCmp(suffix, "POPS/", 5))
		return 0;
	if (suffix[5] == '\0')
		return 0;

	*relative_vcd = suffix + 5;
	return 1;
}

static void replaceFinalExtWithElf(char *path)
{
	size_t len;

	len = strlen(path);
	if (len >= 4)
		strcpy(path + len - 4, ".elf");
}

static int buildArg(char *arg, size_t arg_size, const char *prefix, const char *name)
{
	int len;

	len = snprintf(arg, arg_size, "%s%s", prefix, name);
	if (len < 0 || len >= (int)arg_size)
		return 0;
	replaceFinalExtWithElf(arg);
	return 1;
}

static int buildHddPartitionArg(char *arg, size_t arg_size, const char *partition)
{
	int len;

	len = snprintf(arg, arg_size, "uLE:%s.elf", partition);
	return (len > 0 && len < (int)arg_size);
}

static int openHddPfsPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char party[MAX_NAME];
	char fixed_path[MAX_PATH];
	const char *subpath;
	int pfs_ix;
	int fd;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return -1;

	if (*subpath == '\0')
		subpath = "/";
	if (*subpath != '/')
		return -1;

	loadHddModules();
	snprintf(party, sizeof(party), "%s%s", hdd_device, partition);
	pfs_ix = mountParty(party);
	if (pfs_ix < 0)
		return -1;

	snprintf(fixed_path, sizeof(fixed_path), "pfs%d:%s", pfs_ix, subpath);
	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd >= 0 && opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", fixed_path);
	return fd;
}

static int openPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char fixed_path[MAX_PATH];
	int fd;

	if (path == NULL || path[0] == '\0')
		return -1;

	fd = openHddPfsPathForRead(path, opened_path, opened_path_size);
	if (fd >= 0)
		return fd;

	if (genFixPath(path, fixed_path) >= 0) {
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(fixed_path, sizeof(fixed_path), "mc0:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}

		snprintf(fixed_path, sizeof(fixed_path), "mc1:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", path);
	return -1;
}

static int validateVcd(const char *path)
{
	unsigned char datacheck[32] __attribute__((aligned(16)));
	s64 size;
	int fd;

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_VCD;

	size = genLseek(fd, 0, SEEK_END);
	if (size < 1102672) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genLseek(fd, 0, SEEK_SET);
	if (genRead(fd, datacheck, 4) != 4) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x41 || datacheck[1] != 0x00 || datacheck[2] != 0xA0 || datacheck[3] != 0x00) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	if (genLseek(fd, 1024, SEEK_SET) < 0) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (genRead(fd, datacheck, 16) != 16) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x6B || datacheck[1] != 0x48 || datacheck[2] != 0x6E ||
	    datacheck[8] != datacheck[12] ||
	    datacheck[9] != datacheck[13] ||
	    datacheck[10] != datacheck[14] ||
	    datacheck[11] != datacheck[15]) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genClose(fd);
	return POPSTARTER_OK;
}

static int prepareHddLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *fallbacks)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char candidate[MAX_PATH];
	const char *subpath;
	const char *name;
	int is_pops_partition;
	int ret;

	ret = splitPopstarterHddLaunchPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
	if (ret <= 0)
		return ret;

	if (subpath == NULL || subpath[0] == '\0' || subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	is_pops_partition = !strncmp(partition, "__.POPS", 7);
	if (is_pops_partition) {
		name = strrchr(subpath, '/');
		if (name == NULL || !buildArg(arg, arg_size, "uLE:", name))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	} else {
		if (strncmp(partition, "__.", 3) && strncmp(partition, "PP.", 3))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (stricmp(subpath, "/IMAGE0.VCD"))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (!buildHddPartitionArg(arg, arg_size, partition))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	}

	snprintf(candidate, sizeof(candidate), "%s__common:pfs:/POPS/POPSTARTER.ELF", hdd_device);
	addPopstarterCandidate(fallbacks, candidate);
	snprintf(candidate, sizeof(candidate), "%s__system:pfs:/launcher/POPSTARTER.ELF", hdd_device);
	addPopstarterCandidate(fallbacks, candidate);
	return POPSTARTER_OK;
}

static int prepareFatStyleLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *fallbacks)
{
	char device[16];
	char candidate[MAX_PATH];
	const char *suffix;
	const char *relative_vcd;

	if (!splitDevicePath(path, device, sizeof(device), &suffix))
		return 0;
	if (!isFatStylePopstarterDevice(device))
		return 0;
	if (!startsWithPopsDir(suffix, &relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
	if (!buildArg(arg, arg_size, "uLE:XX.", relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(candidate, sizeof(candidate), "%s/POPS/POPSTARTER.ELF", device);
	addPopstarterCandidate(fallbacks, candidate);
	return POPSTARTER_OK;
}

static u32 readIsoLe32(const unsigned char *value)
{
	return ((u32)value[0]) |
	       ((u32)value[1] << 8) |
	       ((u32)value[2] << 16) |
	       ((u32)value[3] << 24);
}

static size_t minSize(size_t lhs, size_t rhs)
{
	return (lhs < rhs) ? lhs : rhs;
}

static int readVcdIsoSector(int fd, u32 lba, unsigned char *data)
{
	unsigned char raw[POPSTARTER_VCD_RAW_SECTOR_SIZE] __attribute__((aligned(16)));
	s64 offset;
	int data_offset;

	if (data == NULL)
		return 0;

	offset = (s64)POPSTARTER_VCD_IMAGE_OFFSET + ((s64)lba * POPSTARTER_VCD_RAW_SECTOR_SIZE);
	if (genLseek(fd, offset, SEEK_SET) < 0)
		return 0;
	if (genRead(fd, raw, sizeof(raw)) != (int)sizeof(raw))
		return 0;

	if (raw[15] == 1) {
		data_offset = 16;
	} else if (raw[15] == 2) {
		if (raw[18] & 0x20)
			return 0;
		data_offset = 24;
	} else {
		return 0;
	}

	memcpy(data, raw + data_offset, POPSTARTER_CD_SECTOR_DATA_SIZE);
	return 1;
}

static int readVcdIsoPrimaryVolumeDescriptor(int fd, unsigned char *pvd)
{
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	u32 lba;
	int i;

	if (pvd == NULL)
		return 0;

	for (i = 0; i < POPSTARTER_ISO_PVD_SEARCH_SECTORS; i++) {
		lba = POPSTARTER_ISO_PVD_LBA + i;
		if (!readVcdIsoSector(fd, lba, sector))
			return 0;
		if (!memcmp(sector + 1, "CD001", 5)) {
			if (sector[0] == 1) {
				memcpy(pvd, sector, POPSTARTER_CD_SECTOR_DATA_SIZE);
				return 1;
			}
			if (sector[0] == 255)
				return 0;
		}
	}

	return 0;
}

static int buildVcdPvdTimestampGameID(const char *path, char *gameID, size_t gameID_len)
{
	unsigned char pvd[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	int fd;
	size_t i;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return 0;

	if (!readVcdIsoPrimaryVolumeDescriptor(fd, pvd)) {
		genClose(fd);
		return 0;
	}
	genClose(fd);

	for (i = 0; i < sizeof(ps1_generic_game_ids) / sizeof(ps1_generic_game_ids[0]); i++) {
		if (!strncmp((const char *)pvd + POPSTARTER_ISO_VOLUME_TIMESTAMP_OFFSET,
		             ps1_generic_game_ids[i].volume_timestamp,
		             POPSTARTER_ISO_VOLUME_TIMESTAMP_LEN)) {
			snprintf(gameID, gameID_len, "%s", ps1_generic_game_ids[i].game_id);
			return 1;
		}
	}

	return 0;
}

static int isoNameMatches(const unsigned char *name, int name_len, const char *target)
{
	int target_len;
	int i;

	if (name == NULL || target == NULL || name_len <= 0)
		return 0;

	target_len = strlen(target);
	if (name_len < target_len)
		return 0;

	for (i = 0; i < target_len; i++) {
		if (wle_ascii_tolower(name[i]) != wle_ascii_tolower((unsigned char)target[i]))
			return 0;
	}

	return (name_len == target_len || name[target_len] == ';');
}

static int readVcdIsoFileContents(int fd, u32 extent_lba, u32 file_size, char *buffer, size_t buffer_size)
{
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	size_t copied;
	size_t copy_len;
	size_t remaining_file;
	size_t remaining_buffer;
	u32 sector_index;

	if (buffer == NULL || buffer_size == 0 || file_size == 0)
		return 0;

	copied = 0;
	sector_index = 0;
	while (copied < (buffer_size - 1) && copied < file_size) {
		if (!readVcdIsoSector(fd, extent_lba + sector_index, sector))
			break;

		remaining_file = file_size - copied;
		remaining_buffer = buffer_size - 1 - copied;
		copy_len = minSize(POPSTARTER_CD_SECTOR_DATA_SIZE, remaining_file);
		copy_len = minSize(copy_len, remaining_buffer);
		memcpy(buffer + copied, sector, copy_len);
		copied += copy_len;
		sector_index++;
	}
	buffer[copied] = '\0';

	return copied > 0;
}

static int readVcdSystemCnf(int fd, char *buffer, size_t buffer_size)
{
	unsigned char pvd[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	unsigned char sector[POPSTARTER_CD_SECTOR_DATA_SIZE] __attribute__((aligned(16)));
	const unsigned char *root_record;
	u32 root_lba;
	u32 root_size;
	u32 root_sectors;
	u32 sector_index;
	int pos;

	if (buffer == NULL || buffer_size == 0)
		return 0;

	buffer[0] = '\0';
	if (!readVcdIsoPrimaryVolumeDescriptor(fd, pvd))
		return 0;

	root_record = pvd + POPSTARTER_ISO_ROOT_RECORD_OFFSET;
	if (root_record[0] < 34)
		return 0;

	root_lba = readIsoLe32(root_record + 2);
	root_size = readIsoLe32(root_record + 10);
	root_sectors = (root_size + POPSTARTER_CD_SECTOR_DATA_SIZE - 1) / POPSTARTER_CD_SECTOR_DATA_SIZE;
	if (root_sectors > POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS)
		root_sectors = POPSTARTER_ISO_MAX_ROOT_DIR_SECTORS;

	for (sector_index = 0; sector_index < root_sectors; sector_index++) {
		if (!readVcdIsoSector(fd, root_lba + sector_index, sector))
			return 0;

		pos = 0;
		while (pos < POPSTARTER_CD_SECTOR_DATA_SIZE) {
			int record_len;
			int name_len;
			int file_flags;

			record_len = sector[pos];
			if (record_len == 0)
				break;
			if (record_len < 34 || pos + record_len > POPSTARTER_CD_SECTOR_DATA_SIZE)
				break;

			file_flags = sector[pos + 25];
			name_len = sector[pos + 32];
			if (!(file_flags & 0x02) &&
			    record_len >= 33 + name_len &&
			    isoNameMatches(sector + pos + 33, name_len, "SYSTEM.CNF")) {
				u32 file_lba;
				u32 file_size;

				file_lba = readIsoLe32(sector + pos + 2);
				file_size = readIsoLe32(sector + pos + 10);
				return readVcdIsoFileContents(fd, file_lba, file_size, buffer, buffer_size);
			}

			pos += record_len;
		}
	}

	return 0;
}

static int lineStartsWithBootKey(const char *line)
{
	const char *p;

	if (line == NULL)
		return 0;

	p = line;
	while (*p == ' ' || *p == '\t')
		p++;
	if (prefixCaseCmp(p, "BOOT", 4))
		return 0;
	p += 4;
	return (*p == '=' || *p == ' ' || *p == '\t');
}

static int parseSystemCnfBootGameID(const char *cnf, char *gameID, size_t gameID_len)
{
	char boot_path[MAX_PATH];
	const char *line;
	const char *line_end;
	const char *value;
	size_t value_len;

	if (cnf == NULL || gameID == NULL || gameID_len == 0)
		return 0;

	gameID[0] = '\0';
	line = cnf;
	while (*line != '\0') {
		line_end = line;
		while (*line_end != '\0' && *line_end != '\r' && *line_end != '\n')
			line_end++;

		if (lineStartsWithBootKey(line)) {
			value = line + strcspn(line, "=");
			if (value < line_end && *value == '=') {
				value++;
				while (value < line_end && (*value == ' ' || *value == '\t'))
					value++;

				value_len = 0;
				while (value + value_len < line_end &&
				       value[value_len] != ' ' &&
				       value[value_len] != '\t')
					value_len++;
				if (value_len > 0 && value_len < sizeof(boot_path)) {
					memcpy(boot_path, value, value_len);
					boot_path[value_len] = '\0';
					if (buildGameIDFromPopstarterName(boot_path, gameID, gameID_len) &&
					    isLikelyTitleID(gameID))
						return 1;
					gameID[0] = '\0';
				}
			}
		}

		line = line_end;
		while (*line == '\r' || *line == '\n')
			line++;
	}

	return 0;
}

static int buildVcdSystemCnfGameID(const char *path, char *gameID, size_t gameID_len)
{
	char cnf[POPSTARTER_CD_SECTOR_DATA_SIZE + 1];
	int fd;
	int ret;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return 0;

	ret = 0;
	if (readVcdSystemCnf(fd, cnf, sizeof(cnf)))
		ret = parseSystemCnfBootGameID(cnf, gameID, gameID_len);
	genClose(fd);

	return ret;
}

static int buildPopstarterVcdGameID(const char *path, char *gameID, size_t gameID_len)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	const char *subpath;
	int ret;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';

	if (buildVcdSystemCnfGameID(path, gameID, gameID_len))
		return 1;
	if (buildVcdPvdTimestampGameID(path, gameID, gameID_len))
		return 1;

	ret = splitPopstarterHddLaunchPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath);
	if (ret > 0) {
		if (!strncmp(partition, "__.POPS", 7))
			return buildGameIDFromPopstarterName(subpath, gameID, gameID_len);
		if ((!strncmp(partition, "__.", 3) || !prefixCaseCmp(partition, "PP.", 3)) &&
		    subpath != NULL && !stricmp(subpath, "/IMAGE0.VCD"))
			return buildGameIDFromPopstarterName(partition, gameID, gameID_len);
	}

	return buildGameIDFromPopstarterName(path, gameID, gameID_len);
}

static int prepareLaunch(const char *path, char *arg, size_t arg_size, POPSTARTER_CANDIDATE_LIST *candidates)
{
	int ret;

	if (!IsPopstarterVcdPath(path))
		return POPSTARTER_ERR_NOT_VCD;

	initPopstarterCandidates(candidates);
	if (setting != NULL && setting->popstarter_file[0] != '\0')
		addPopstarterCandidate(candidates, setting->popstarter_file);

	ret = prepareHddLaunch(path, arg, arg_size, candidates);
	if (ret == 0)
		ret = prepareFatStyleLaunch(path, arg, arg_size, candidates);
	if (ret <= 0)
		return (ret == 0) ? POPSTARTER_ERR_UNSUPPORTED_PATH : ret;

	return POPSTARTER_OK;
}

static int checkExecutablePath(const char *path, int *exec_kind)
{
	char tmp[MAX_PATH];
	int kind;

	if (path == NULL || path[0] == '\0' || exec_kind == NULL)
		return 0;

	snprintf(tmp, sizeof(tmp), "%s", path);
	kind = checkELFheader(tmp);
	if (kind <= 0)
		return 0;

	*exec_kind = kind;
	return 1;
}

static int prepareHddPfsElfLaunch(const char *path, char *fullpath, size_t fullpath_size,
                                  char *party, size_t party_size, int *exec_kind)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char browser_path[MAX_PATH];
	const char *subpath;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return 0;
	if (subpath == NULL || subpath[0] == '\0')
		subpath = "/";
	if (subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(browser_path, sizeof(browser_path), "%s/%s%s", hdd_device, partition, subpath);
	if (!checkExecutablePath(browser_path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(party, party_size, "%s%s", hdd_device, partition);
	snprintf(fullpath, fullpath_size, "pfs0:%s", subpath);
	return POPSTARTER_OK;
}

static int preparePopstarterElfLaunch(const char *path, char *fullpath, size_t fullpath_size, char *party, size_t party_size, int *exec_kind)
{
	char tmp[MAX_PATH];
	char *p;
	int fd;
	int ret;

	if (path == NULL || path[0] == '\0' || fullpath == NULL || fullpath_size == 0 ||
	    party == NULL || party_size == 0 || exec_kind == NULL)
		return POPSTARTER_ERR_OPEN_POPSTARTER;

	fullpath[0] = '\0';
	party[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_POPSTARTER;
	genClose(fd);

	ret = prepareHddPfsElfLaunch(path, fullpath, fullpath_size, party, party_size, exec_kind);
	if (ret != 0)
		return ret;

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(tmp, sizeof(tmp), "mc0:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}

		snprintf(tmp, sizeof(tmp), "mc1:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}
		return POPSTARTER_ERR_INVALID_POPSTARTER;
	}

	if (isHddDevicePath(path)) {
		loadHddModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(party, party_size, "hdd%c:%s", path[3], path + 6);
		p = strchr(party, '/');
		if (p == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		snprintf(fullpath, fullpath_size, "pfs0:%s", p);
		*p = '\0';
		return POPSTARTER_OK;
	}

#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6)) {
		if (!mx4sio_driver_running && !loadMx4sioModules())
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

#ifdef MMCE
	if (!strncmp(path, "mmce", 4)) {
		loadMmceModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

	if (!strncmp(path, "usb", 3)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (genFixPath(path, fullpath) < 0)
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "ata", 3)) {
		char *pathSep;

#ifdef EXFAT
		loadAtaModules();
#endif
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (!strncmp(path, "ata:", 4))
			snprintf(fullpath, fullpath_size, "ata0:%s", path + 4);
		else
			snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "mass", 4)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
		return POPSTARTER_OK;
	}

#if defined(ETH) || defined(UDPFS)
	if (!strncmp(path, "host:", 5)) {
#ifdef ETH
		initHOST();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "host:%s", (path[5] == '/') ? path + 6 : path + 5);
		makeHostPath(fullpath, fullpath);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}

	if (!strncmp(path, "udpfs:", 6)) {
#ifdef UDPFS
		load_udpfs();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}
#endif

	if (!checkExecutablePath(path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(fullpath, fullpath_size, "%s", path);
	return POPSTARTER_OK;
}

static int preparePopstarterCandidateLaunch(const POPSTARTER_CANDIDATE_LIST *candidates,
                                            char *selected_path, size_t selected_path_size,
                                            char *fullpath, size_t fullpath_size,
                                            char *party, size_t party_size, int *exec_kind)
{
	int i;
	int ret;

	if (candidates == NULL || candidates->count <= 0)
		return POPSTARTER_ERR_OPEN_POPSTARTER;

	ret = POPSTARTER_ERR_OPEN_POPSTARTER;
	for (i = 0; i < candidates->count; i++) {
		if (selected_path != NULL && selected_path_size > 0)
			snprintf(selected_path, selected_path_size, "%s", candidates->entry[i].path);

		ret = preparePopstarterElfLaunch(candidates->entry[i].path, fullpath, fullpath_size, party, party_size, exec_kind);
		if (ret == POPSTARTER_OK)
			return POPSTARTER_OK;
		if (ret != POPSTARTER_ERR_OPEN_POPSTARTER)
			return ret;
	}

	return ret;
}

static void setPopstarterMessage(char *message, size_t message_size, int result, const char *vcd_path, const char *popstarter_path)
{
	if (message == NULL || message_size == 0)
		return;

	switch (result) {
		case POPSTARTER_ERR_NOT_VCD:
		case POPSTARTER_ERR_INVALID_VCD:
			snprintf(message, message_size, "This file isn't a POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_UNSUPPORTED_PATH:
			snprintf(message, message_size, "POPStarter does not support that VCD path: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_VCD:
			snprintf(message, message_size, "Cannot open POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_POPSTARTER:
			snprintf(message, message_size, "Cannot find POPStarter ELF: %s", popstarter_path);
			break;
		case POPSTARTER_ERR_INVALID_POPSTARTER:
			snprintf(message, message_size, "POPStarter ELF is not an ELF: %s", popstarter_path);
			break;
		default:
			snprintf(message, message_size, "POPStarter launch failed: %s", vcd_path);
			break;
	}
	message[message_size - 1] = '\0';
}

int IsPopstarterVcdPath(const char *path)
{
	return (path != NULL && genCmpFileExt(path, "VCD"));
}

int LaunchPopstarterVcd(const char *path, char *message, size_t message_size)
{
	char arg0[MAX_PATH];
	char launch_gameid[12];
	POPSTARTER_CANDIDATE_LIST popstarter_candidates;
	char popstarter_path[MAX_PATH];
	char popstarter_fullpath[MAX_PATH];
	char popstarter_party[MAX_PATH];
	int exec_kind;
	int have_launch_gameid;
	int reboot_iop_elf_load;
	int show_launch_gameid;
	int ret;

	arg0[0] = '\0';
	launch_gameid[0] = '\0';
	popstarter_path[0] = '\0';
	popstarter_fullpath[0] = '\0';
	popstarter_party[0] = '\0';
	exec_kind = 1;
	have_launch_gameid = 0;
	reboot_iop_elf_load = 0;
	show_launch_gameid = 0;
	initPopstarterCandidates(&popstarter_candidates);

	ret = prepareLaunch(path, arg0, sizeof(arg0), &popstarter_candidates);
	if (ret < 0)
		goto fail;

	ret = validateVcd(path);
	if (ret < 0)
		goto fail;

	have_launch_gameid = buildPopstarterVcdGameID(path, launch_gameid, sizeof(launch_gameid));

	ret = preparePopstarterCandidateLaunch(&popstarter_candidates, popstarter_path, sizeof(popstarter_path),
	                                       popstarter_fullpath, sizeof(popstarter_fullpath),
	                                       popstarter_party, sizeof(popstarter_party), &exec_kind);
	if (ret < 0)
		goto fail;

	if (have_launch_gameid && isLikelyTitleID(launch_gameid)) {
		updateOSDHistoryFile(launch_gameid);
		applyXPARAM(launch_gameid);
	}

	if (setting != NULL) {
		reboot_iop_elf_load = setting->reboot_iop_elf_load;
		show_launch_gameid = have_launch_gameid && !setting->cdrom_disable_gameid;
	} else
		show_launch_gameid = have_launch_gameid;
	if (isHddDevicePath(path) || isHddPfsPath(path) || isHddPartyPath(popstarter_party))
		unmountAll();
	CleanUpForExec();
	if (show_launch_gameid)
		displayRetroGemGameID(launch_gameid, 2);
	RunLoaderElf(popstarter_fullpath, popstarter_party, arg0, exec_kind, reboot_iop_elf_load);
	return POPSTARTER_OK;

fail:
	setPopstarterMessage(message, message_size, ret, path, popstarter_path);
	return ret;
}

//--------------------------------------------------------------
// End of file: popstarter.c
//--------------------------------------------------------------
