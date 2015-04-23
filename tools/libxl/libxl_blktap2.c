/*
 * Copyright (C) 2010      Advanced Micro Devices
 * Author Christoph Egger <Christoph.Egger@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include "libxl_osdeps.h" /* must come before any other headers */
#include "libxl_internal.h"

#include "tap-ctl.h"

int libxl__blktap_enabled(libxl__gc *gc)
{
    const char *msg;
    return !tap_ctl_check(&msg);
}

static int tap_ctl_find(const char *type, const char *disk, tap_list_t *tap) 
{
    int err;
    struct list_head list = LIST_HEAD_INIT(list);
    tap_list_t *entry;

    err = tap_ctl_list(&list);
    if (err)
        return err;

    err = ERROR_FAIL;

    tap_list_for_each_entry(entry, &list) {
        if (type && (!entry->type || strcmp(entry->type, type)))
            continue;
        
        if (disk && (!entry->path || strcmp(entry->path, disk)))
            continue;
        
        tap->minor = entry->minor;
        tap->pid = entry->pid;
        err = 0;
        break;
    }
    tap_ctl_list_free(&list);
    
    return err;
}

char *libxl__blktap_devpath(libxl__gc *gc,
                            const char *disk,
                            libxl_disk_format format,
                            int readwrite)
{
    const char *type;
    char *params, *devname = NULL;
    int err;
    int minor;
    int flags;

    type = libxl__device_disk_string_of_format(format);

    minor = tap_ctl_find_minor(type, disk);
    if (minor >= 0) {
        devname = libxl__sprintf(gc, "/dev/xen/blktap-2/tapdev%d", minor);
        if (devname)
            return devname;
    }

    params = libxl__sprintf(gc, "%s:%s", type, disk);
    fprintf(stderr, "DEBUG %s %d %s\n", __func__, __LINE__, params);
    flags = readwrite ? 0 : TAPDISK_MESSAGE_FLAG_RDONLY;

    err = tap_ctl_create(params, &devname, flags, -1, 0, 0);
    if (!err) {
        fprintf(stderr, "DEBUG %s %d %s\n", __func__, __LINE__, devname);
        libxl__ptr_add(gc, devname);
        return devname;
    }

    return NULL;
}


int libxl__device_destroy_tapdisk(libxl__gc *gc, const char *params)
{
    char *type, *disk;
    int err;
    struct list_head list = LIST_HEAD_INIT(list);
    tap_list_t tap = { .minor=-1, .pid=-1 };

    type = libxl__strdup(gc, params);

    disk = strchr(type, ':');
    if (!disk) {
        LOG(ERROR, "Unable to parse params %s", params);
        return ERROR_INVAL;
    }

    fprintf(stderr, "DEBUG %s %d type=%s disk=%s\n",__func__,__LINE__,type,disk);
    *disk++ = '\0';

    err = tap_ctl_find(type, disk, &tap);
    if (err) {
        /* returns -errno */
        LOGEV(ERROR, -err, "Unable to find type %s disk %s", type, disk);
        return ERROR_FAIL;
    }

    err = tap_ctl_destroy(tap.pid, tap.minor, 1, NULL);
    if (err < 0) {
        LOGEV(ERROR, -err, "Failed to destroy tap device id %d minor %d",
              tap.pid, tap.minor);
        return ERROR_FAIL;
    }

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
