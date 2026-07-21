#ifndef HDD_HEADER_INJECTOR_H
#define HDD_HEADER_INJECTOR_H

#include <stddef.h>

int InjectHddPartitionHeaderFromSource(const char *partition_name, const char *source_device, char *source_dir, size_t source_dir_size,
                                       int *pfs_files_copied, int *pfs_copy_error);

#endif
