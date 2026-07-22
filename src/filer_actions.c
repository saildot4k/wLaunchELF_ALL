#include "launchelf.h"
#include "filer_actions.h"
#include "filer_shared.h"
#include "gui_hdd0_format.h"

#define IOCTL_RENAME 0xFEEDC0DE

#ifdef XFROM
static const char *getXfromRelativePath(const char *path)
{
	const char *relative_path = strchr(path, ':');

	if (relative_path == NULL)
		return path;
	relative_path++;
	if (*relative_path == '/')
		relative_path++;
	return relative_path;
}
#endif

u64 getFileSize(const char *path, const FILEINFO *file)
{
	iox_stat_t stat;
	u64 size, filesize;
	FILEINFO files[MAX_ENTRY];
	char dir[MAX_PATH], party[MAX_NAME];
	int nfiles, i, ret;

	if (!ensurePathDeviceStackReady(path))
		return 0;

	if (file->stats.AttrFile & sceMcFileAttrSubdir) {  //Folder object to size up
		sprintf(dir, "%s%s/", path, file->name);
		nfiles = getDir(dir, files);
		for (i = size = 0; i < nfiles; i++) {
			filesize = getFileSize(dir, &files[i]);  //recurse for each object in folder
			if (filesize < 0)
				return -1;
			else
				size += filesize;
		}
	} else {  //File object to size up
		if (!strncmp(path, "hdd", 3)) {
			getHddParty(path, file, party, dir);
			ret = mountParty(party);
			if (ret < 0)
				return 0;
			dir[3] = ret + '0';
#ifdef DVRP
		} else if (!strncmp(path, "dvr_hdd", 7)) {
			getHddDVRPParty(path, file, party, dir);
			ret = mountDVRPParty(party);
			if (ret < 0)
				return 0;
			dir[7] = ret + '0';
#endif
		} else
			sprintf(dir, "%s%s", path, file->name);
#if defined(ETH) || defined(UDPFS)
		if (!strncmp(dir, "host:/", 6))
			makeHostPath(dir, dir);
#endif
		fileXioGetStat(dir, &stat);
		size = stat.size;
	}
	return size;
}
//------------------------------
//endfunc getFileSize
//--------------------------------------------------------------
//
//this function will allow you to force the date of any memory-card save file...
//... into the highest date available for a ps2 (1 second before year 2100)
// ----------=====args=====----------
// path: mc0:/ or mc1:/
// const FILEINFO *file = the FILEINFO struct for that save, however, this function only cares about folder name
//_msg0 = pointer to msg0 to report what happened to the user (uLaunchELF only)
//#ifdef TMANIP
	void time_manip(const char *path, const FILEINFO *file, char *_msg0)
	{
		int rett;  //this var will be used to store the result of mcSetFileInfo()
		int slot;
		slot = path[2] - '0';
		#define ARRAY_ENTRIES 64
		static sceMcTblGetDir mcDirAAA[ARRAY_ENTRIES] __attribute__((aligned(64)));  // save file properties
		static sceMcStDateTime new_mtime;                                            //manipulated struct for savefile properties, this will be used to change the date of the save file properties
																					//char *result,*end;
																					/*=====================================================================================================*/
	/*
	#ifdef TMANIP_MORON
		McGetDir(slot, 0, HACK_FOLDER, 0x2, ARRAY_ENTRIES, &mcDirAAA);
	#else
		McGetDir(slot, 0,  file->name, 0x2, ARRAY_ENTRIES, &mcDirAAA);
	#endif*/ //till i find the real name of this func on ps2dev:1.0
		new_mtime.Resv2 = 0;
		new_mtime.Sec = 59;
		new_mtime.Min = 59;
		new_mtime.Hour = 23;
		new_mtime.Day = 31;
		new_mtime.Month = 12;
		new_mtime.Year = 2099;
		mcDirAAA->_Modify = new_mtime;
		mcDirAAA->_Create = new_mtime;
		/*=====================================================================================================*/
	
	#ifdef TMANIP_MORON
		rett = mcSetFileInfo(slot, 0, HACK_FOLDER, mcDirAAA, 0x02);
		if (rett == 0)
			sprintf(_msg0, "success, folder [%s]  Mc Slot [%d] .", HACK_FOLDER, slot);
		if (rett < 0)
			sprintf(_msg0, "error [%d], folder[%s]  Mc Slot=[%d] .", rett, HACK_FOLDER, slot);
	#else
		rett = mcSetFileInfo(slot, 0, file->name, mcDirAAA, 0x02);
		if (rett == 0)
			sprintf(_msg0, "success, folder [%s]  Mc Slot [%d] .", file->name, slot);
		if (rett < 0)
			sprintf(_msg0, "error [%d], folder[%s]  Mc Slot=[%d] .", rett, file->name, slot);
	#endif //TMANIP_MORON
	
	
	
		mcSync(0, NULL, &rett);
	}  // TIMEMANIP
	//------------------------------
	//endfunc time_manip
	//--------------------------------------------------------------
	//
//#endif //TMANIP

void make_title_cfg(const char *path, const FILEINFO *file, char *_msg0)
{
	int fd;
	char title_cfg_buffer[2 * MAX_NAME + 16], ELF_NAME[MAX_NAME];

	snprintf(ELF_NAME, sizeof(ELF_NAME), "%s", file->name);
	ELF_NAME[strlen(ELF_NAME) - 4] = '\0';  //kill extension, we can do this freely without checking string length because feature is only enabled on .ELF files
	snprintf(title_cfg_buffer, sizeof(title_cfg_buffer), "title=%s\nboot=%s", ELF_NAME, file->name);
	char new_title_cfg[MAX_PATH];
	strcpy(new_title_cfg, path);
	strcat(new_title_cfg, "title.cfg");
	if ((fd = genOpen(new_title_cfg, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0) {
		snprintf(_msg0, MAX_PATH, "Error opening title.cfg");
		return;
	} else {
		genWrite(fd, title_cfg_buffer, strlen(title_cfg_buffer));
		genClose(fd);
	}

}
//------------------------------
//endfunc make_title_cfg
//--------------------------------------------------------------
int delete (const char *path, const FILEINFO *file)
{
	FILEINFO files[MAX_ENTRY];
	char party[MAX_NAME], dir[MAX_PATH], hdddir[MAX_PATH];
	int nfiles, i, ret;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		if (getHddParty(path, file, party, hdddir) < 0)
			return -1;
		ret = mountParty(party);
		if (ret < 0)
			return -1;
		hdddir[3] = ret + '0';
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		if (getHddDVRPParty(path, file, party, hdddir) < 0)
			return -1;
		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		hdddir[7] = ret + '0';
#endif
	}
	sprintf(dir, "%s%s", path, file->name);
	genLimObjName(dir, 0);
#if defined(ETH) || defined(UDPFS)
	if (!strncmp(dir, "host:/", 6))
		makeHostPath(dir, dir);
#endif
	if (file->stats.AttrFile & sceMcFileAttrSubdir) {  //Is the object to delete a folder ?
		strcat(dir, "/");
		nfiles = getDir(dir, files);
		for (i = 0; i < nfiles; i++) {
			ret = delete (dir, &files[i]);  //recursively delete contents of folder
			if (ret < 0)
				return -1;
		}
		if (!strncmp(dir, "mc", 2)) {
			mcSync(0, NULL, NULL);
			mcDelete(dir[2] - '0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
#ifdef XFROM
		} else if (!strncmp(dir, "xfrom", 5)) {
			xfromSync(0, NULL, NULL);
			xfromDelete(0, 0, getXfromRelativePath(dir));
			xfromSync(0, NULL, &ret);
#endif
		} else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
			ret = fileXioRmdir(hdddir);
		} else if (!strncmp(path, "vmc", 3)) {
			ret = fileXioRmdir(dir);

		} else {  //For all other devices
			sprintf(dir, "%s%s", path, file->name);
			ret = fileXioRmdir(dir);
		}
	} else {  //The object to delete is a file
		if (!strncmp(path, "mc", 2)) {
			mcSync(0, NULL, NULL);
			mcDelete(dir[2] - '0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
#ifdef XFROM
		} else if (!strncmp(path, "xfrom", 5)) {
			xfromSync(0, NULL, NULL);
			xfromDelete(0, 0, getXfromRelativePath(dir));
			xfromSync(0, NULL, &ret);
#endif
		} else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
			ret = fileXioRemove(hdddir);
		} else if (!strncmp(path, "vmc", 3)) {
			ret = fileXioRemove(dir);
		} else {  //For all other devices
			ret = fileXioRemove(dir);
		}
	}
	return ret;
}
//--------------------------------------------------------------
int Rename(const char *path, const FILEINFO *file, const char *name)
{
	char party[MAX_NAME], oldPath[MAX_PATH], newPath[MAX_PATH];
	int test, ret = 0;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		if (getHddParty(path, NULL, party, oldPath) < 0)
			return -1;
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);

		ret = mountParty(party);
		if (ret < 0)
			return -1;
		oldPath[3] = newPath[3] = ret + '0';
		ret = fileXioRename(oldPath, newPath);
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		sprintf(party, "dvr_hdd0:%s", &path[10]);
		*strchr(party, '/') = 0;
		sprintf(oldPath, "dvr_pfs0:%s", strchr(&path[10], '/') + 1);
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);

		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		oldPath[7] = newPath[7] = ret + '0';
		ret = fileXioRename(oldPath, newPath);
#endif
	} else if (!strncmp(path, "mc", 2)) {
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		if ((test = fileXioDopen(newPath)) >= 0) {  //Does folder of same name exist ?
			fileXioDclose(test);
			ret = -EEXIST;
		} else if ((test = fileXioOpen(newPath, FIO_O_RDONLY, 0)) >= 0) {  //Does file of same name exist ?
			fileXioClose(test);
			ret = -EEXIST;
		} else {  //No file/folder of the same name exists
			mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
			mcSync(0, NULL, &test);
			if (mctype_PSx == 2)  //PS2 MC ?
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
			mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  //Fix file stats
			mcSync(0, NULL, &test);
			if (ret == -4)
				ret = -EEXIST;
			else {  //PS1 MC !
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
				mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  //Fix file stats
				mcSync(0, NULL, &test);
				if (ret == -4)
					ret = -EEXIST;
			}
		}
#ifdef XFROM
	} else if (!strncmp(path, "xfrom", 5)) {
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		if ((test = fileXioDopen(newPath)) >= 0) {  //Does folder of same name exist ?
			fileXioDclose(test);
			ret = -EEXIST;
		} else if ((test = fileXioOpen(newPath, FIO_O_RDONLY, 0)) >= 0) {  //Does file of same name exist ?
			fileXioClose(test);
			ret = -EEXIST;
		} else {  //No file/folder of the same name exists
			xfromGetInfo(0, 0, &mctype_PSx, NULL, NULL);
			xfromSync(0, NULL, &test);
			if (mctype_PSx == 2)  //PS2 MC ?
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
			xfromSetFileInfo(0, 0, getXfromRelativePath(oldPath), &file->stats, 0x0010);  //Fix file stats
			xfromSync(0, NULL, &ret);
			if (ret == -4)
				ret = -EEXIST;
			else if (mctype_PSx != 2) {  //PS1 MC !
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
				xfromSetFileInfo(0, 0, getXfromRelativePath(oldPath), &file->stats, 0x0010);  //Fix file stats
				xfromSync(0, NULL, &ret);
				if (ret == -4)
					ret = -EEXIST;
			}
		}
#endif
#if defined(ETH) || defined(UDPFS)
	} else if (!strncmp(path, "host", 4) || !strncmp(path, "udpfs", 5)) {
		snprintf(oldPath, sizeof(oldPath), "%s%s", path, file->name);
		snprintf(newPath, sizeof(newPath), "%s%s", path, name);
		makeHostPath(oldPath, oldPath);
		makeHostPath(newPath, newPath);
		ret = fileXioRename(oldPath, newPath);
#endif
	} else {  //For all other devices
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		ret = fileXioRename(oldPath, newPath);
	}

	return ret;
}

//--------------------------------------------------------------
int newdir(const char *path, const char *name)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int ret = 0;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		getHddParty(path, NULL, party, dir);
		ret = mountParty(party);
		if (ret < 0)
			return -1;
		dir[3] = ret + '0';
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = fileXioMkdir(dir, fileMode);
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		getHddDVRPParty(path, NULL, party, dir);
		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		dir[7] = ret + '0';
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = fileXioMkdir(dir, fileMode);
#endif
	} else if (!strncmp(path, "vmc", 3)) {
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		printf("[VMC_NEWDIR] request path='%s' name='%s' dir='%s'\n", path, name, dir);
		if ((ret = fileXioDopen(dir)) >= 0) {
			printf("[VMC_NEWDIR] exists dir='%s' fd=%d\n", dir, ret);
			fileXioDclose(ret);
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
		} else {
			printf("[VMC_NEWDIR] dopen failed dir='%s' ret=%d; trying mkdir\n", dir, ret);
			ret = fileXioMkdir(dir, fileMode);
			printf("[VMC_NEWDIR] mkdir dir='%s' ret=%d\n", dir, ret);
		}
	} else if (!strncmp(path, "mc", 2)) {
		sprintf(dir, "%s%s", path + 4, name);
		genLimObjName(dir, 0);
		mcSync(0, NULL, NULL);
		mcMkDir(path[2] - '0', 0, dir);
		mcSync(0, NULL, &ret);
		if (ret == -4)
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
#ifdef XFROM
	} else if (!strncmp(path, "xfrom", 5)) {
		snprintf(dir, sizeof(dir), "%s%s", getXfromRelativePath(path), name);
		genLimObjName(dir, 0);
		xfromSync(0, NULL, NULL);
		xfromMkDir(0, 0, dir);
		xfromSync(0, NULL, &ret);
		if (ret == -4)
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
#endif
#if defined(ETH) || defined(UDPFS)
	} else if (!strncmp(path, "host", 4) || !strncmp(path, "udpfs", 5)) {
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		if (!strncmp(dir, "host:/", 6))
			makeHostPath(dir, dir);
		if ((ret = fileXioDopen(dir)) >= 0) {
			fileXioDclose(ret);
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
		} else {
			ret = fileXioMkdir(dir, fileMode);  //Create the new folder
		}
#endif
	} else {  //For all other devices
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = fileXioMkdir(dir, fileMode);
	}
	return ret;
}
//--------------------------------------------------------------
//End of file: filer_actions.c
//--------------------------------------------------------------
