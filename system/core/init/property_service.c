/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <sys/poll.h>

#include <cutils/misc.h>
#include <cutils/sockets.h>
#include <cutils/multiuser.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <private/android_filesystem_config.h>

#include <selinux/selinux.h>
#include <selinux/label.h>

#include "property_service.h"
#include "init.h"
#include "util.h"
#include "log.h"

#ifdef MTK_INIT
#include <sys/system_properties.h>
#include <cutils/android_reboot.h>
#endif
#define PERSISTENT_PROPERTY_DIR  "/data/property"

static int persistent_properties_loaded = 0;
static int property_area_inited = 0;

static int property_set_fd = -1;

#ifdef INIT_ENG_BUILD
/* The following structure and macro definition are only for KK debug usage! */
/* Please do not use it in your design or reuse it in other android codebase */
typedef struct {
	unsigned bytes_used;
	unsigned volatile serial;
	unsigned magic;
	unsigned version;
	unsigned reserved[28];
	char data[0];
}prop_area;

extern prop_area *__system_property_area__;
#define PA_SIZE_WARN ((PA_SIZE * 8) / 10)
/* (PA_SIZE - sizeof(sturct prop_info) - sizeof(struct prop_bt)) */
#define PA_SIZE_ERR (PA_SIZE - 128 - 20)

static int alarmed = 0;
static int warned = 0;
#endif

typedef struct {
    size_t size;
    int fd;
} workspace;

static int init_workspace(workspace *w, size_t size)
{
    void *data;
    int fd = open(PROP_FILENAME, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return -1;

    w->size = size;
    w->fd = fd;
    return 0;
}

static workspace pa_workspace;

static int init_property_area(void)
{
    if (property_area_inited)
        return -1;

    if(__system_property_area_init())
        return -1;

    if(init_workspace(&pa_workspace, 0))
        return -1;

    fcntl(pa_workspace.fd, F_SETFD, FD_CLOEXEC);

    property_area_inited = 1;
    return 0;
}

static int check_mac_perms(const char *name, char *sctx)
{
    if (is_selinux_enabled() <= 0)
        return 1;

    char *tctx = NULL;
    const char *class = "property_service";
    const char *perm = "set";
    int result = 0;

    if (!sctx)
        goto err;

    if (!sehandle_prop)
        goto err;

    if (selabel_lookup(sehandle_prop, &tctx, name, 1) != 0)
        goto err;

    if (selinux_check_access(sctx, tctx, class, perm, (void*) name) == 0)
        result = 1;

    freecon(tctx);
 err:
    return result;
}

static int check_control_mac_perms(const char *name, char *sctx)
{
    /*
     *  Create a name prefix out of ctl.<service name>
     *  The new prefix allows the use of the existing
     *  property service backend labeling while avoiding
     *  mislabels based on true property prefixes.
     */
    char ctl_name[PROP_VALUE_MAX+4];
    int ret = snprintf(ctl_name, sizeof(ctl_name), "ctl.%s", name);

    if (ret < 0 || (size_t) ret >= sizeof(ctl_name))
        return 0;

    return check_mac_perms(ctl_name, sctx);
}

/*
 * Checks permissions for setting system properties.
 * Returns 1 if uid allowed, 0 otherwise.
 */
static int check_perms(const char *name, char *sctx)
{
    int i;
    unsigned int app_id;

    if(!strncmp(name, "ro.", 3))
        name +=3;

    return check_mac_perms(name, sctx);
}

int __property_get(const char *name, char *value)
{
    return __system_property_get(name, value);
}

static void write_persistent_property(const char *name, const char *value)
{
    char tempPath[PATH_MAX];
    char path[PATH_MAX];
    int fd;

    snprintf(tempPath, sizeof(tempPath), "%s/.temp.XXXXXX", PERSISTENT_PROPERTY_DIR);
    fd = mkstemp(tempPath);
    if (fd < 0) {
        ERROR("Unable to write persistent property to temp file %s errno: %d\n", tempPath, errno);
        return;
    }
    write(fd, value, strlen(value));
    fsync(fd);
    close(fd);

    snprintf(path, sizeof(path), "%s/%s", PERSISTENT_PROPERTY_DIR, name);
    if (rename(tempPath, path)) {
        unlink(tempPath);
        ERROR("Unable to rename persistent property file %s to %s\n", tempPath, path);
    }
}

#ifdef MTK_INIT
static void print_sys_prop(const struct prop_info *pi, void *cookies)
{
	char key[PROP_NAME_MAX];
	char name[PROP_VALUE_MAX];
	unsigned int serial;
	int valuelen;
	serial = __system_property_serial(pi);
	valuelen = __system_property_read(pi, key, name);
	if (valuelen < 1 || valuelen > PROP_VALUE_MAX)
		return;
	klog_write(6, "<6> [+%3u]%-32s = %s\n",
			(serial & 0xffffff) >> 1, key, name);
}

void property_show(void)
{
	//klog_write(4, "<4> property_show start!\n");
	__system_property_foreach(print_sys_prop, NULL);
	//klog_write(4, "<4> property_show done!\n");
}
#endif
static bool is_legal_property_name(const char* name, size_t namelen)
{
    size_t i;
    if (namelen >= PROP_NAME_MAX) {
    	ERROR("PropSet Error:[%s] namelen >= %d\n", name, PROP_NAME_MAX);
    	return false;
    }
    if (namelen < 1) {
    	ERROR("PropSet Error:[%s] namelen < 1\n", name);
    	return false;
    }
    if (name[0] == '.') {
    	ERROR("PropSet Error:[%s] name shouldn't begin with .\n", name);
    	return false;
    }
    if (name[namelen - 1] == '.') {
    	ERROR("PropSet Error:[%s] name shouldn't end with .\n", name);
    	return false;
    }

    /* Only allow alphanumeric, plus '.', '-', or '_' */
    /* Don't allow ".." to appear in a property name */
    for (i = 0; i < namelen; i++) {
        if (name[i] == '.') {
            // i=0 is guaranteed to never have a dot. See above.
            if (name[i-1] == '.') {
            	ERROR("PropSet Error:[%s] don't allow .. in property name\n", name);
            	return false;
            }
            continue;
        }
        if (name[i] == '_' || name[i] == '-') continue;
        if (name[i] >= 'a' && name[i] <= 'z') continue;
        if (name[i] >= 'A' && name[i] <= 'Z') continue;
        if (name[i] >= '0' && name[i] <= '9') continue;
        else {
        	ERROR("PropSet Error:[%s] Only allow alphanumeric, plus '.', '-', or '_' \n", name);
        	return false;
        }
    }

    return true;
}

int property_set_overwirte(const char *name, const char *value, int allow_overwirte_ro) ////Lenovo [EasyImage]
{
    prop_info *pi;
    int ret;

    size_t namelen = strlen(name);
    size_t valuelen = strlen(value);

    if (!is_legal_property_name(name, namelen)) {
    	ERROR("PropSet Error:[%s:%s] property name is illegal\n", name, value);
    	return -1;
    }
    if (valuelen >= PROP_VALUE_MAX) {
    	ERROR("PropSet Error:[%s:%s] valuelen %d >= %d\n", name, value, valuelen, PROP_VALUE_MAX);
    	return -1;
    }
#ifdef MTK_INIT
    INFO("PropSet [%s:%s] Start>>\n", name, value);
#endif
    pi = (prop_info*) __system_property_find(name);

    if(pi != 0) {
        /* ro.* properties may NEVER be modified once set */
        if(allow_overwirte_ro!= 1 && !strncmp(name, "ro.", 3)){
#ifdef MTK_INIT
        	ERROR("PropSet Error:[%s:%s]  ro.* properties may NEVER be modified once set\n", name, value);
#endif
        	return -1;
        }
        __system_property_update(pi, value, valuelen);
    } else {
#ifdef INIT_ENG_BUILD
		prop_area *pa = __system_property_area__;
		if ((pa->bytes_used > PA_SIZE_ERR) && (0 == alarmed ))
		{
			alarmed = 1;
			ERROR("[Property Error]: Unable to set [%s:%s] property limit has arrived: %d\n", name, value, pa->bytes_used);
#ifdef MTK_INIT
			property_show();
#endif
		}
		else if ((pa->bytes_used > PA_SIZE_WARN) && (0 == warned ))
		{
			warned = 1;
			NOTICE("[Property Warning]: limit would be arrived:%d (Max:%d). "
					"Use getprop to review your properties!\n", pa->bytes_used, PA_SIZE);
#ifdef MTK_INIT
			property_show();
#endif
		}
#endif
        ret = __system_property_add(name, namelen, value, valuelen);
        if (ret < 0) {
#ifdef INIT_ENG_BUILD
            ERROR("Failed to set '%s'='%s' bytes_used=%u\n", name, value, pa->bytes_used);
#else
			ERROR("Failed to set '%s'='%s'\n", name, value);
#endif
            return ret;
        }
    }
    /* If name starts with "net." treat as a DNS property. */
    if (strncmp("net.", name, strlen("net.")) == 0)  {
        if (strcmp("net.change", name) == 0) {
#ifdef MTK_INIT
            INFO("PropSet [%s:%s] Done\n", name, value);
#endif
            return 0;
        }
       /*
        * The 'net.change' property is a special property used track when any
        * 'net.*' property name is updated. It is _ONLY_ updated here. Its value
        * contains the last updated 'net.*' property.
        */
        property_set("net.change", name);
    } else if (persistent_properties_loaded &&
            strncmp("persist.", name, strlen("persist.")) == 0) {
        /*
         * Don't write properties to disk until after we have read all default properties
         * to prevent them from being overwritten by default values.
         */
        write_persistent_property(name, value);
    } else if (strcmp("selinux.reload_policy", name) == 0 &&
               strcmp("1", value) == 0) {
        selinux_reload_policy();
    }
    property_changed(name, value);
#ifdef MTK_INIT
    INFO("PropSet [%s:%s] Done\n", name, value);
#endif
    return 0;
}

//Lenovo [EasyImage] Start
int property_set(const char *name, const char *value)
{
	return property_set_overwirte(name,value,0);
}
//Lenovo [EasyImage] End

#ifdef MTK_INIT
int reboot_pid(int pid)
{
	int fd = open("/proc/mtprof/reboot_pid", O_RDWR);
	char buf[100];
	int cnt;
	
	cnt = sprintf(buf, "%d", pid);
	
	fprintf(stderr, "reboot  pid is %d, %s.\n", pid, buf);	
	if(fd > 0){
		write(fd, buf, cnt);		
		close(fd);
		return 1;
	}
	else{
		fprintf(stderr, "open /proc/mtprof/reboot_pid error");
	}

	return 0;
}
#endif

void handle_property_set_fd()
{
    prop_msg msg;
    int s;
    int r;
    int res;
    struct ucred cr;
    struct sockaddr_un addr;
    socklen_t addr_size = sizeof(addr);
    socklen_t cr_size = sizeof(cr);
    char * source_ctx = NULL;
    struct pollfd ufds[1];
    const int timeout_ms = 2 * 1000;  /* Default 2 sec timeout for caller to send property. */
    int nr;

    if ((s = accept(property_set_fd, (struct sockaddr *) &addr, &addr_size)) < 0) {
        return;
    }

    /* Check socket options here */
    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cr, &cr_size) < 0) {
        close(s);
        ERROR("Unable to receive socket options\n");
        return;
    }

    ufds[0].fd = s;
    ufds[0].events = POLLIN;
    ufds[0].revents = 0;
    nr = TEMP_FAILURE_RETRY(poll(ufds, 1, timeout_ms));
    if (nr == 0) {
        ERROR("sys_prop: timeout waiting for pid=%d uid=%d gid=%d to send property message.\n", cr.pid, cr.uid, cr.gid);
        close(s);
        return;
    } else if (nr < 0) {
        ERROR("sys_prop: error waiting for pid=%d uid=%d gid=%d to send property message. err=%d %s\n", cr.pid, cr.uid, cr.gid, errno, strerror(errno));
        close(s);
        return;
    }

    r = TEMP_FAILURE_RETRY(recv(s, &msg, sizeof(msg), MSG_DONTWAIT));
    if(r != sizeof(prop_msg)) {
        ERROR("sys_prop: mis-match msg size received: %d from pid=%d uid=%d gid=%d expected: %zu errno: %d\n",
              r, cr.pid, cr.uid, cr.gid, sizeof(prop_msg), errno);
        close(s);
        return;
    }

    switch(msg.cmd) {
    case PROP_MSG_SETPROP:
        msg.name[PROP_NAME_MAX-1] = 0;
        msg.value[PROP_VALUE_MAX-1] = 0;

        if (!is_legal_property_name(msg.name, strlen(msg.name))) {
            ERROR("sys_prop: illegal property name. Got: \"%s\"\n", msg.name);
            close(s);
            return;
        }

        getpeercon(s, &source_ctx);

        if(memcmp(msg.name,"ctl.",4) == 0) {
            // Keep the old close-socket-early behavior when handling
            // ctl.* properties.
            close(s);
            if (check_control_mac_perms(msg.value, source_ctx)) {
#ifdef MTK_INIT
				INFO("[PropSet]: pid:%u uid:%u gid:%u %s %s\n", cr.pid, cr.uid, cr.gid, msg.name, msg.value);
#endif
                handle_control_message((char*) msg.name + 4, (char*) msg.value);
            } else {
                ERROR("sys_prop: Unable to %s service ctl [%s] uid:%d gid:%d pid:%d\n",
                        msg.name + 4, msg.value, cr.uid, cr.gid, cr.pid);
            }
        } else {
            if (check_perms(msg.name, source_ctx)) {
#ifdef MTK_INIT
				INFO("[PropSet]: pid:%u uid:%u gid:%u set %s=%s\n", cr.pid, cr.uid, cr.gid, msg.name, msg.value);
				if(strcmp(msg.name, ANDROID_RB_PROPERTY) == 0)
				{
					INFO("pid %d set %s=%s\n", cr.pid, msg.name, msg.value);
					reboot_pid(cr.pid);
				}
#endif
                property_set((char*) msg.name, (char*) msg.value);
            } else {
                ERROR("sys_prop: permission denied uid:%d  name:%s\n",
                      cr.uid, msg.name);
            }

            // Note: bionic's property client code assumes that the
            // property server will not close the socket until *AFTER*
            // the property is written to memory.
            close(s);
        }
        freecon(source_ctx);
        break;

    default:
        close(s);
        break;
    }
}

void get_property_workspace(int *fd, int *sz)
{
    *fd = pa_workspace.fd;
    *sz = pa_workspace.size;
}

static void load_properties_from_file(const char *, const char *);
static void load_properties_from_file_overwrite(const char *, const char *,const int); //Lenovo [EasyImage] 

/*
 * Filter is used to decide which properties to load: NULL loads all keys,
 * "ro.foo.*" is a prefix match, and "ro.foo.bar" is an exact match.
 */
static void load_properties_overwrite(char *data, const char *filter,const int allow_overwrite_ro) //Lenovo [EasyImage]
{
    char *key, *value, *eol, *sol, *tmp, *fn;
    size_t flen = 0;

    if (filter) {
        flen = strlen(filter);
    }

    sol = data;
    while ((eol = strchr(sol, '\n'))) {
        key = sol;
        *eol++ = 0;
        sol = eol;

        while (isspace(*key)) key++;
        if (*key == '#') continue;

        tmp = eol - 2;
        while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;

        if (!strncmp(key, "import ", 7) && flen == 0) {
            fn = key + 7;
            while (isspace(*fn)) fn++;

            key = strchr(fn, ' ');
            if (key) {
                *key++ = 0;
                while (isspace(*key)) key++;
            }

#ifdef MTK_INIT
			NOTICE("import properties %s from %s\n", key, fn);
#endif
            load_properties_from_file_overwrite(fn, key,allow_overwrite_ro);

        } else {
            value = strchr(key, '=');
            if (!value) continue;
            *value++ = 0;

            tmp = value - 2;
            while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;

            while (isspace(*value)) value++;

            if (flen > 0) {
                if (filter[flen - 1] == '*') {
                    if (strncmp(key, filter, flen - 1)) continue;
                } else {
                    if (strcmp(key, filter)) continue;
                }
            }

            property_set_overwirte(key, value,allow_overwrite_ro); //Lenovo [EasyImage]
        }
    }
}

//Lenovo [EasyImage] Start
static void load_properties(char *data,const char *filter)
{
	load_properties_overwrite(data,filter,0);
}
//Lenovo [EasyImage] End
/*
 * Filter is used to decide which properties to load: NULL loads all keys,
 * "ro.foo.*" is a prefix match, and "ro.foo.bar" is an exact match.
 */
static void load_properties_from_file_overwrite(const char *fn, const char *filter, const int allow_overwrite_ro) //Lenovo [EasyImage] 
{
    char *data;
    unsigned sz;

    data = read_file(fn, &sz);

    if(data != 0) {
        load_properties_overwrite(data, filter,allow_overwrite_ro);
        free(data);
    }
#ifdef MTK_INIT
    else
        ERROR("can not load properties %s from %s\n", filter, fn);
#endif
}

//Lenovo [EasyImage] Start
static void load_properties_from_file(const char *fn,const char *filter)
{
	load_properties_from_file_overwrite(fn,filter,0);
}
//Lenovo [EasyImage] End

static void load_persistent_properties()
{
    DIR* dir = opendir(PERSISTENT_PROPERTY_DIR);
    int dir_fd;
    struct dirent*  entry;
    char value[PROP_VALUE_MAX];
    int fd, length;
    struct stat sb;

    if (dir) {
        dir_fd = dirfd(dir);
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp("persist.", entry->d_name, strlen("persist.")))
                continue;
#if HAVE_DIRENT_D_TYPE
            if (entry->d_type != DT_REG)
                continue;
#endif
            /* open the file and read the property value */
            fd = openat(dir_fd, entry->d_name, O_RDONLY | O_NOFOLLOW);
            if (fd < 0) {
                ERROR("Unable to open persistent property file \"%s\" errno: %d\n",
                      entry->d_name, errno);
                continue;
            }
            if (fstat(fd, &sb) < 0) {
                ERROR("fstat on property file \"%s\" failed errno: %d\n", entry->d_name, errno);
                close(fd);
                continue;
            }

            // File must not be accessible to others, be owned by root/root, and
            // not be a hard link to any other file.
            if (((sb.st_mode & (S_IRWXG | S_IRWXO)) != 0)
                    || (sb.st_uid != 0)
                    || (sb.st_gid != 0)
                    || (sb.st_nlink != 1)) {
                ERROR("skipping insecure property file %s (uid=%u gid=%u nlink=%d mode=%o)\n",
                      entry->d_name, (unsigned int)sb.st_uid, (unsigned int)sb.st_gid,
                      sb.st_nlink, sb.st_mode);
                close(fd);
                continue;
            }

            length = read(fd, value, sizeof(value) - 1);
            if (length >= 0) {
                value[length] = 0;
                property_set(entry->d_name, value);
            } else {
                ERROR("Unable to read persistent property file %s errno: %d\n",
                      entry->d_name, errno);
            }
            close(fd);
        }
        closedir(dir);
    } else {
        ERROR("Unable to open persistent property directory %s errno: %d\n", PERSISTENT_PROPERTY_DIR, errno);
    }

    persistent_properties_loaded = 1;
}

void property_init(void)
{
    init_property_area();
}

void property_load_boot_defaults(void)
{
    load_properties_from_file(PROP_PATH_RAMDISK_DEFAULT, NULL);
}

int properties_inited(void)
{
    return property_area_inited;
}

static void load_override_properties() {
#ifdef ALLOW_LOCAL_PROP_OVERRIDE
    char debuggable[PROP_VALUE_MAX];
    int ret;

    ret = property_get("ro.debuggable", debuggable);
    if (ret && (strcmp(debuggable, "1") == 0)) {
        load_properties_from_file(PROP_PATH_LOCAL_OVERRIDE, NULL);
    }
#endif /* ALLOW_LOCAL_PROP_OVERRIDE */
}


/* When booting an encrypted system, /data is not mounted when the
 * property service is started, so any properties stored there are
 * not loaded.  Vold triggers init to load these properties once it
 * has mounted /data.
 */
void load_persist_props(void)
{
    load_override_properties();
    /* Read persistent properties after all default values have been loaded. */
    load_persistent_properties();
}

void load_all_props(void)
{
    load_properties_from_file(PROP_PATH_SYSTEM_BUILD, NULL);
    load_properties_from_file(PROP_PATH_SYSTEM_DEFAULT, NULL);
    load_properties_from_file(PROP_PATH_FACTORY, "ro.*");

    load_override_properties();

#ifdef LENOVO_EASYIMAGE_SUPPORT

//Lenovo [EasyImage] Start
#define EASYIMAGE_OVERLAY_PROP "/system/easyimage/preload/prop.overlay"
	if( access(EASYIMAGE_OVERLAY_PROP,R_OK) !=-1 )
	{
		load_properties_from_file_overwrite(EASYIMAGE_OVERLAY_PROP,NULL,1); //use 1 to overwrite ro prop
	}

#define EASYIMAGE_OVERLAY_2_PROP "/system/easyimage/preload/prop.overlay.1"

    if( access(EASYIMAGE_OVERLAY_2_PROP,R_OK) !=-1 )
    {
        load_properties_from_file_overwrite(EASYIMAGE_OVERLAY_2_PROP,NULL,1); //use 1 to overwrite ro prop
    }

#endif
//Lenovo [EasyImage] End

    /* Read persistent properties after all default values have been loaded. */
    load_persistent_properties();
}

void start_property_service(void)
{
    int fd;


    fd = create_socket(PROP_SERVICE_NAME, SOCK_STREAM, 0666, 0, 0, NULL);
    if(fd < 0) return;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    listen(fd, 8);
    property_set_fd = fd;
}

int get_property_set_fd()
{
    return property_set_fd;
}
