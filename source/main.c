#include "resolve.h"

#define PAYLOAD_NAME "PS5 Database and User Backup v1.0"

void printf_notification(const char* fmt, ...) {
    SceNotificationRequest noti_buffer;

    va_list args;
    va_start(args, fmt);
    f_vsprintf(noti_buffer.message, fmt, args);
    va_end(args);

    noti_buffer.type = 0;
    noti_buffer.unk3 = 0;
    noti_buffer.use_icon_image_uri = 1;
    noti_buffer.target_id = -1;
    f_strcpy(noti_buffer.uri, "cxml://psnotification/tex_icon_system");

    f_sceKernelSendNotificationRequest(0, (SceNotificationRequest * ) & noti_buffer, sizeof(noti_buffer), 0);
}

char* cDir;
size_t sizeCurrent, total_bytes_copied;

void copy_file(char* src_path, char* dst_path)
{
    int src = f_open(src_path, O_RDONLY, 0);
    if (src != -1)
    {
        int out = f_open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (out != -1)
        {

            char* buffer = f_malloc(4194304);
            if (buffer != NULL)
            {
                size_t bytes, bytes_size, bytes_copied = 0;
                f_lseek(src, 0L, SEEK_END);
                bytes_size = f_lseek(src, 0, SEEK_CUR);
                f_lseek(src, 0L, SEEK_SET);
                while ((bytes = f_read(src, buffer, 4194304)) > 0)
                {
                    f_write(out, buffer, bytes);
                    bytes_copied += bytes;
                    if (bytes_copied > bytes_size)
                        bytes_copied = bytes_size;
                    total_bytes_copied += bytes_copied;
                }
                f_free(buffer);
            }
        }
        f_close(out);
    }
    f_close(src);
}

int dir_exists(char* dname)
{
    DIR* dir = f_opendir(dname);
    if (dir)
    {
        f_closedir(dir);
        return 1;
    }
    return 0;
}

void copy_dir(char* dir_current, char* out_dir)
{
    DIR* dir = f_opendir(dir_current);
    struct dirent* dp;
    struct stat info;
    char src_path[256], dst_path[256];
    if (!dir)
    {
        return;
    }

    f_mkdir(out_dir, 0777);

    if (dir_exists(out_dir))
    {
        while ((dp = f_readdir(dir)) != NULL)
        {
            if (!f_strcmp(dp->d_name, ".") || !f_strcmp(dp->d_name, ".."))
            {
                continue;
            }
            else
            {
                f_sprintf(src_path, "%s/%s", dir_current, dp->d_name);
                f_sprintf(dst_path, "%s/%s", out_dir, dp->d_name);

                if (!f_stat(src_path, &info))
                {
                    if (S_ISDIR(info.st_mode))
                    {
                        cDir = src_path;
                        copy_dir(src_path, dst_path);
                    }
                    else if (S_ISREG(info.st_mode))
                    {
                        copy_file(src_path, dst_path);
                    }
                }
            }
        }
    }
    f_closedir(dir);
}

int nthread_run, sav, isxfer;

size_t size_file(char* src_file)
{
    FILE* file;
    size_t size = 0;
    file = f_fopen(src_file, "rb");
    if (file)
    {
        f_fseek(file, 0, SEEK_END);
        size = f_ftell(file);
    }
    f_fclose(file);
    return size;
}

void check_size_folder_current(char* dir_current)
{
    DIR* dir = f_opendir(dir_current);
    struct dirent* dp;
    struct stat info;
    char src_file[1024];
    size_t size;
    if (!dir)
        return;
    while ((dp = f_readdir(dir)) != NULL)
    {
        if (!f_strcmp(dp->d_name, ".") || !f_strcmp(dp->d_name, ".."))
            continue;
        else
        {
            f_sprintf(src_file, "%s/%s", dir_current, dp->d_name);
            if (!f_stat(src_file, &info))
            {
                if (S_ISDIR(info.st_mode))
                {
                    sav++;
                    check_size_folder_current(src_file);
                }
                else if (S_ISREG(info.st_mode))
                {
                    size = size_file(src_file);
                    sizeCurrent += size;
                }
            }
        }
    }
    f_closedir(dir);
}

void* nthread_func(void* arg)
{
    UNUSED(arg);
    time_t t1, t2;
    t1 = 0;
    while (nthread_run)
    {
        if (isxfer)
        {
            t2 = f_time(NULL);
            if ((t2 - t1) >= 7)
            {
                t1 = t2;
                printf_notification("Copy in progress please wait...\n%i%%", total_bytes_copied * 10 / sizeCurrent);
            }
        }
        else
            t1 = 0;
        f_sceKernelSleep(5);
    }

    return NULL;
}

int file_stat(char* fname)
{
    FILE* file = f_fopen(fname, "rb");
    if (file)
    {
        f_fclose(file);
        return 1;
    }
    return 0;
}

void touch_file(char* destfile)
{
    int fd = f_open(destfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd != -1)
        f_close(fd);
}

int sock;

#define printfsocket(format, ...)\
do {\
  char __printfsocket_buffer[512];\
  int __printfsocket_size = f_sprintf(__printfsocket_buffer, format, ##__VA_ARGS__);\
  f_sceNetSend(sock, __printfsocket_buffer, __printfsocket_size, 0);\
} while(0)

char* getusbpath()
{
    char tmppath[64];
    char tmpusb[64];
    tmpusb[0] = '\0';
    char* retval = f_malloc(sizeof(char) * 10);

    for (int x = 0; x <= 7; x++)
    {
        f_sprintf(tmppath, "/mnt/usb%i/.probe", x);
        touch_file(tmppath);
        if (file_stat(tmppath))
        {
            f_unlink(tmppath);
            f_sprintf(tmpusb, "/mnt/usb%i", x);
            printfsocket("[USB MOUNT PATH]: %s\n", tmpusb);

            f_strcpy(retval, tmpusb);
            return retval;
        }
        tmpusb[0] = '\0';
    }
    printfsocket("[NO USB FOUND.Wait...]\n");
    return NULL;
}

void touch(const char* filename) {

    int file_descriptor = f_open(filename, O_CREAT, S_IRUSR | S_IWUSR);

    if (file_descriptor == -1) {

        printf_notification("Error creating file on USB");

        f_exit(EXIT_FAILURE);

    }

    f_close(file_descriptor);
}

int check_file_exists(const char* filename) {
    struct stat buffer;
    int exist = f_stat(filename, &buffer);
    if (exist == 0)
        return 1;
    else
        return 0;
}

int start_backup()
{

    touch("/mnt/usb0/.probe");

    if (!check_file_exists("/mnt/usb0/.probe")) {

        touch("/mnt/usb1/.probe");

        if (!check_file_exists("/mnt/usb1/.probe")) {

            printf_notification("Insert USB for database backup");

        }
        else {

            printf_notification("Dumping to USB1");

            printf_notification("Dumping Database files");

            f_mkdir("/mnt/usb1/PS5", 0777);
            f_mkdir("/mnt/usb1/PS5/Backup", 0777);

            f_mkdir("/mnt/usb1/PS5/Backup/system_data", 0777);
            f_mkdir("/mnt/usb1/PS5/Backup/system_data/priv", 0777);
            f_mkdir("/mnt/usb1/PS5/Backup/system_data/priv/mms", 0777);

            copy_file("/system_data/priv/mms/app.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/app.db");
            copy_file("/system_data/priv/mms/appinfo.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/appinfo.db");
            copy_file("/system_data/priv/mms/addcont.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/addcont.db");
            copy_file("/system_data/priv/mms/av_content_bg.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/av_content_bg.db");
            copy_file("/system_data/priv/mms/av_content.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/av_content.db");
            copy_file("/system_data/priv/mms/notification.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/notification.db");
            copy_file("/system_data/priv/mms/notification2.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/notification2.db");

            f_mkdir("/mnt/usb1/PS5/Backup/user", 0777);
            f_mkdir("/mnt/usb1/PS5/Backup/user/license", 0777);
            copy_file("/user/license/act.dat", "/mnt/usb1/PS5/Backup/user/license/act.dat");

            printf_notification("Dumping Database files done\nPlease wait...");

            f_sceKernelSleep(7);

            #define OCT_TO_MO / 1024 / 1024

            printf_notification("Dumping Savegame Database files");

            char* src_path = "/system_data/savedata_prospero";
            char* dst_path = "/mnt/usb1/PS5/Backup/system_data/savedata_prospero";

            f_mkdir("/mnt/usb1/PS5/Backup/system_data/savedata_prospero", 0777);

            if (dir_exists(src_path))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path);

                printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path, (double)sizeCurrent OCT_TO_MO, sav);

                f_sceKernelSleep(7);
                isxfer = 1;
                copy_dir(src_path, dst_path);
                isxfer = 0;
                f_sceKernelSleep(7);
                sizeCurrent = 0;

                check_size_folder_current(src_path);

                if (total_bytes_copied == sizeCurrent)
                {
                    printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    f_sceKernelSleep(7);
                }
                else
                {
                    printf_notification("Some files and folders were not copied!");
                }

            }

            printf_notification("Dumping Savegame Database files done\nPlease wait...");

            f_sceKernelSleep(7);

            printf_notification("Dumping User files");

            char* src_path0 = "/user/home";
            char* dst_path0 = "/mnt/usb1/PS5/Backup/user/home";

            f_mkdir("/mnt/usb1/PS5/Backup/user/home", 0777);

            if (dir_exists(src_path0))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path0);

                printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path0, (double)sizeCurrent OCT_TO_MO, sav);

                f_sceKernelSleep(7);
                isxfer = 1;
                copy_dir(src_path0, dst_path0);
                isxfer = 0;
                f_sceKernelSleep(7);
                sizeCurrent = 0;

                check_size_folder_current(src_path0);

                if (total_bytes_copied == sizeCurrent)
                {
                    printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path0, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    f_sceKernelSleep(7);
                }
                else
                {
                    printf_notification("Some files and folders were not copied!");
                }

            }

            printf_notification("Dumping User files done\nPlease wait...");

            f_sceKernelSleep(7);

            printf_notification("Dumping User Audio and Video files");

            char* src_path1 = "/user/av_contents";
            char* dst_path1 = "/mnt/usb1/PS5/Backup/user/av_contents";

            f_mkdir("/mnt/usb1/PS5/Backup/user/av_contents", 0777);

            if (dir_exists(src_path1))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path1);

                printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path1, (double)sizeCurrent OCT_TO_MO, sav);

                f_sceKernelSleep(7);
                isxfer = 1;
                copy_dir(src_path1, dst_path1);
                isxfer = 0;
                f_sceKernelSleep(7);
                sizeCurrent = 0;

                check_size_folder_current(src_path1);

                if (total_bytes_copied == sizeCurrent)
                {
                    printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path1, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    f_sceKernelSleep(7);
                }
                else
                {
                    printf_notification("Some files and folders were not copied!");
                }

            }

            printf_notification("Dumping User Audio and Video files done");

            f_sceKernelSleep(7);

            f_unlink("/mnt/usb1/.probe");

            printf_notification("Dump to USB1 done!");

        }

    }

    else {

        printf_notification("Dumping to USB0");

        printf_notification("Dumping Database files");

        f_mkdir("/mnt/usb0/PS5", 0777);
        f_mkdir("/mnt/usb0/PS5/Backup", 0777);

        f_mkdir("/mnt/usb0/PS5/Backup/system_data", 0777);
        f_mkdir("/mnt/usb0/PS5/Backup/system_data/priv", 0777);
        f_mkdir("/mnt/usb0/PS5/Backup/system_data/priv/mms", 0777);

        copy_file("/system_data/priv/mms/app.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/app.db");
        copy_file("/system_data/priv/mms/appinfo.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/appinfo.db");
        copy_file("/system_data/priv/mms/addcont.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/addcont.db");
        copy_file("/system_data/priv/mms/av_content_bg.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/av_content_bg.db");
        copy_file("/system_data/priv/mms/av_content.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/av_content.db");
        copy_file("/system_data/priv/mms/notification.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/notification.db");
        copy_file("/system_data/priv/mms/notification2.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/notification2.db");

        f_mkdir("/mnt/usb0/PS5/Backup/user", 0777);
        f_mkdir("/mnt/usb0/PS5/Backup/user/license", 0777);
        copy_file("/user/license/act.dat", "/mnt/usb0/PS5/Backup/user/license/act.dat");

        printf_notification("Dumping Database files done\nPlease wait...");

        f_sceKernelSleep(7);

        #define OCT_TO_MO / 1024 / 1024

        printf_notification("Dumping Savegame Database files");

        char* src_path = "/system_data/savedata_prospero";
        char* dst_path = "/mnt/usb0/PS5/Backup/system_data/savedata_prospero";

        f_mkdir("/mnt/usb0/PS5/Backup/system_data/savedata_prospero", 0777);

        if (dir_exists(src_path))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path);

            printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path, (double)sizeCurrent OCT_TO_MO, sav);

            f_sceKernelSleep(7);
            isxfer = 1;
            copy_dir(src_path, dst_path);
            isxfer = 0;
            f_sceKernelSleep(7);
            sizeCurrent = 0;

            check_size_folder_current(src_path);

            if (total_bytes_copied == sizeCurrent)
            {
                printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                f_sceKernelSleep(7);
            }
            else
            {
                printf_notification("Some files and folders were not copied!");
            }

        }

        printf_notification("Dumping Savegame Database files done\nPlease wait...");

        f_sceKernelSleep(7);

        printf_notification("Dumping User files");

        char* src_path0 = "/user/home";
        char* dst_path0 = "/mnt/usb0/PS5/Backup/user/home";

        f_mkdir("/mnt/usb0/PS5/Backup/user/home", 0777);

        if (dir_exists(src_path0))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path0);

            printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path0, (double)sizeCurrent OCT_TO_MO, sav);

            f_sceKernelSleep(7);
            isxfer = 1;
            copy_dir(src_path0, dst_path0);
            isxfer = 0;
            f_sceKernelSleep(7);
            sizeCurrent = 0;

            check_size_folder_current(src_path0);

            if (total_bytes_copied == sizeCurrent)
            {
                printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path0, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                f_sceKernelSleep(7);
            }
            else
            {
                printf_notification("Some files and folders were not copied!");
            }

        }

        printf_notification("Dumping User files done\nPlease wait...");

        f_sceKernelSleep(7);

        printf_notification("Dumping User Audio and Video files");

        char* src_path1 = "/user/av_contents";
        char* dst_path1 = "/mnt/usb0/PS5/Backup/user/av_contents";

        f_mkdir("/mnt/usb0/PS5/Backup/user/av_contents", 0777);

        if (dir_exists(src_path1))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path1);

            printf_notification("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path1, (double)sizeCurrent OCT_TO_MO, sav);

            f_sceKernelSleep(7);
            isxfer = 1;
            copy_dir(src_path1, dst_path1);
            isxfer = 0;
            f_sceKernelSleep(7);
            sizeCurrent = 0;

            check_size_folder_current(src_path1);

            if (total_bytes_copied == sizeCurrent)
            {
                printf_notification("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path1, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                f_sceKernelSleep(7);
            }
            else
            {
                printf_notification("Some files and folders were not copied!");
            }

        }

        printf_notification("Dumping User Audio and Video files done");

        f_sceKernelSleep(7);

        f_unlink("/mnt/usb0/.probe");

        printf_notification("Dump to USB0 done!");

    }

    return 0;
}


int payload_main(struct payload_args *args) {

    dlsym_t *dlsym = args->dlsym;

    int libKernel = 0x2001;

    dlsym(libKernel, "sceKernelSleep", &f_sceKernelSleep);
    dlsym(libKernel, "sceKernelLoadStartModule", &f_sceKernelLoadStartModule);
    dlsym(libKernel, "sceKernelDebugOutText", &f_sceKernelDebugOutText);
    dlsym(libKernel, "sceKernelSendNotificationRequest", &f_sceKernelSendNotificationRequest);
    dlsym(libKernel, "sceKernelUsleep", &f_sceKernelUsleep);
    dlsym(libKernel, "scePthreadMutexLock", &f_scePthreadMutexLock);
    dlsym(libKernel, "scePthreadMutexUnlock", &f_scePthreadMutexUnlock);
    dlsym(libKernel, "scePthreadExit", &f_scePthreadExit);
    dlsym(libKernel, "scePthreadMutexInit", &f_scePthreadMutexInit);
    dlsym(libKernel, "scePthreadCreate", &f_scePthreadCreate);
    dlsym(libKernel, "scePthreadMutexDestroy", &f_scePthreadMutexDestroy);
    dlsym(libKernel, "scePthreadJoin", &f_scePthreadJoin);
    dlsym(libKernel, "socket", &f_socket);
    dlsym(libKernel, "bind", &f_bind);
    dlsym(libKernel, "listen", &f_listen);
    dlsym(libKernel, "accept", &f_accept);
    dlsym(libKernel, "open", &f_open);
    dlsym(libKernel, "read", &f_read);
    dlsym(libKernel, "write", &f_write);
    dlsym(libKernel, "close", &f_close);
    dlsym(libKernel, "stat", &f_stat);
    dlsym(libKernel, "fstat", &f_fstat);
    dlsym(libKernel, "rename", &f_rename);
    dlsym(libKernel, "rmdir", &f_rmdir);
    dlsym(libKernel, "mkdir", &f_mkdir);
    dlsym(libKernel, "getdents", &f_getdents);
    dlsym(libKernel, "unlink", &f_unlink);
    dlsym(libKernel, "readlink", &f_readlink);
    dlsym(libKernel, "lseek", &f_lseek);
    dlsym(libKernel, "puts", &f_puts);
    dlsym(libKernel, "mmap", &f_mmap);
    dlsym(libKernel, "munmap", &f_munmap);
    dlsym(libKernel, "_read", &f__read);

    int libNet = f_sceKernelLoadStartModule("libSceNet.sprx", 0, 0, 0, 0, 0);
    dlsym(libNet, "sceNetSocket", &f_sceNetSocket);
    dlsym(libNet, "sceNetConnect", &f_sceNetConnect);
    dlsym(libNet, "sceNetHtons", &f_sceNetHtons);
    dlsym(libNet, "sceNetAccept", &f_sceNetAccept);
    dlsym(libNet, "sceNetSend", &f_sceNetSend);
    dlsym(libNet, "sceNetInetNtop", &f_sceNetInetNtop);
    dlsym(libNet, "sceNetSocketAbort", &f_sceNetSocketAbort);
    dlsym(libNet, "sceNetBind", &f_sceNetBind);
    dlsym(libNet, "sceNetListen", &f_sceNetListen);
    dlsym(libNet, "sceNetSocketClose", &f_sceNetSocketClose);
    dlsym(libNet, "sceNetHtonl", &f_sceNetHtonl);
    dlsym(libNet, "sceNetInetPton", &f_sceNetInetPton);
    dlsym(libNet, "sceNetGetsockname", &f_sceNetGetsockname);
    dlsym(libNet, "sceNetRecv", &f_sceNetRecv);
    dlsym(libNet, "sceNetErrnoLoc", &f_sceNetErrnoLoc);
    dlsym(libNet, "sceNetSetsockopt", &f_sceNetSetsockopt);

    int libC = f_sceKernelLoadStartModule("libSceLibcInternal.sprx", 0, 0, 0, 0, 0);
    dlsym(libC, "vsprintf", &f_vsprintf);
    dlsym(libC, "memset", &f_memset);
    dlsym(libC, "sprintf", &f_sprintf);
    dlsym(libC, "snprintf", &f_snprintf);
    dlsym(libC, "snprintf_s", &f_snprintf_s);
    dlsym(libC, "strcat", &f_strcat);
    dlsym(libC, "free", &f_free);
    dlsym(libC, "memcpy", &f_memcpy);
    dlsym(libC, "strcpy", &f_strcpy);
    dlsym(libC, "strncpy", &f_strncpy);
    dlsym(libC, "sscanf", &f_sscanf);
    dlsym(libC, "malloc", &f_malloc);
    dlsym(libC, "calloc", &f_calloc);
    dlsym(libC, "strlen", &f_strlen);
    dlsym(libC, "strcmp", &f_strcmp);
    dlsym(libC, "strchr", &f_strchr);
    dlsym(libC, "strrchr", &f_strrchr);
    dlsym(libC, "gmtime_s", &f_gmtime_s);
    dlsym(libC, "time", &f_time);
    dlsym(libC, "localtime", &f_localtime);

    dlsym(libC, "fclose", &f_fclose);
    dlsym(libC, "fopen", &f_fopen);
    dlsym(libC, "fseek", &f_fseek);
    dlsym(libC, "ftell", &f_ftell);

    int libNetCtl = f_sceKernelLoadStartModule("libSceNetCtl.sprx", 0, 0, 0, 0, 0);
    dlsym(libNetCtl, "sceNetCtlInit", &f_sceNetCtlInit);
    dlsym(libNetCtl, "sceNetCtlTerm", &f_sceNetCtlTerm);
    dlsym(libNetCtl, "sceNetCtlGetInfo", &f_sceNetCtlGetInfo);

    dlsym(libC, "opendir", &f_opendir);
    dlsym(libC, "readdir", &f_readdir);
    dlsym(libC, "closedir", &f_closedir);
 
    printf_notification("PS5 Database and User Backup\nVersion 1.0 by Storm");

    f_sceKernelSleep(7);

    ScePthread nthread;
    f_scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

    nthread_run = 1;

    char* usb_mnt_path = getusbpath();
    if (usb_mnt_path == NULL)
    {
        do
        {
            printf_notification("Please insert USB media in exfat/fat32 format");
            f_sceKernelSleep(7);
            usb_mnt_path = getusbpath();
        } while (usb_mnt_path == NULL);
    }
    f_free(usb_mnt_path);

    start_backup();

    nthread_run = 0;

    return 0;
}
