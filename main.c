/* Copyright (C) 2026 Storm21

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include "notify.h"
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

int notify_user;
char* cDir;
size_t sizeCurrent, total_bytes_copied;


void copy_file(char* src_path, char* dst_path)
{
    int src = open(src_path, O_RDONLY, 0);
    if (src != -1)
    {
        int out = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (out != -1)
        {

            char* buffer = malloc(4194304);
            if (buffer != NULL)
            {
                size_t bytes, bytes_size, bytes_copied = 0;
                lseek(src, 0L, SEEK_END);
                bytes_size = lseek(src, 0, SEEK_CUR);
                lseek(src, 0L, SEEK_SET);
                while ((bytes = read(src, buffer, 4194304)) > 0)
                {
                    write(out, buffer, bytes);
                    bytes_copied += bytes;
                    if (bytes_copied > bytes_size)
                        bytes_copied = bytes_size;
                    total_bytes_copied += bytes_copied;
                }
                free(buffer);
            }
        }
        close(out);
    }
    close(src);
}


int dir_exists(char* dname)
{
    DIR* dir = opendir(dname);
    if (dir)
    {
        closedir(dir);
        return 1;
    }
    return 0;
}


void copy_dir(char* dir_current, char* out_dir)
{
    DIR* dir = opendir(dir_current);
    struct dirent* dp;
    struct stat info;
    char src_path[256], dst_path[256];
    if (!dir)
    {
        return;
    }

    mkdir(out_dir, 0777);

    if (dir_exists(out_dir))
    {
        while ((dp = readdir(dir)) != NULL)
        {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            {
                continue;
            }
            else
            {
                sprintf(src_path, "%s/%s", dir_current, dp->d_name);
                sprintf(dst_path, "%s/%s", out_dir, dp->d_name);

                if (!stat(src_path, &info))
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
    closedir(dir);
}


int nthread_run, sav, isxfer;

size_t size_file(char* src_file)
{
    FILE* file;
    size_t size = 0;
    file = fopen(src_file, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
    }
    fclose(file);
    return size;
}


void check_size_folder_current(char* dir_current)
{
    DIR* dir = opendir(dir_current);
    struct dirent* dp;
    struct stat info;
    char src_file[1024];
    size_t size;
    if (!dir)
        return;
    while ((dp = readdir(dir)) != NULL)
    {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;
        else
        {
            sprintf(src_file, "%s/%s", dir_current, dp->d_name);
            if (!stat(src_file, &info))
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
    closedir(dir);
}


void* nthread_func(void* arg)
{
    time_t t1, t2;
    t1 = 0;
    while (nthread_run)
    {
        if (isxfer)
        {
            t2 = time(NULL);
            if ((t2 - t1) >= 7)
            {
                t1 = t2;

                if (notify_user) {
                    notify("Copy in progress please wait...\n%i%%", total_bytes_copied * 10 / sizeCurrent);
                }
            }
        }
        else
            t1 = 0;
        sleep(5); 
    }

    return NULL;
}


int file_stat(char* fname)
{
    FILE* file = fopen(fname, "rb");
    if (file)
    {
        fclose(file);
        return 1;
    }
    return 0;
}


void touch_file(char* destfile)
{

    int fd = open(destfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    if (fd != -1)
        close(fd);
}


char* getusbpath()
{
    char tmppath[64];
    char tmpusb[64];
    tmpusb[0] = '\0';
    char* retval = malloc(sizeof(char) * 10);

    for (int x = 0; x <= 7; x++)
    {
        sprintf(tmppath, "/mnt/usb%i/.probe", x);
        touch_file(tmppath);
        if (file_stat(tmppath))
        {
            remove(tmppath);
            sprintf(tmpusb, "/mnt/usb%i", x);
            strcpy(retval, tmpusb);
            return retval;
        }
        tmpusb[0] = '\0';
    }
    return NULL;
}


void touch(const char* filename) {

    int file_descriptor = open(filename, O_CREAT, S_IRUSR | S_IWUSR);

    if (file_descriptor == -1) {

        if (notify_user) {
            notify("Error creating file on USB");
        }

        exit(EXIT_FAILURE);

    }

    close(file_descriptor);
}


bool check_file_exists(const char* filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0 ? true : false;
}


int start_backup()
{
    touch("/mnt/usb0/.probe");

    if (!check_file_exists("/mnt/usb0/.probe")) {

        touch("/mnt/usb1/.probe");

        if (!check_file_exists("/mnt/usb1/.probe")) {

            if (notify_user) {
                notify("Insert USB for database backup");
            }

        }
        else {

            if (notify_user) {
                notify("Dumping to USB1");
            }

            if (notify_user) {
                notify("Dumping Database files");
            }

            mkdir("/mnt/usb1/PS5", 0777);
            mkdir("/mnt/usb1/PS5/Backup", 0777);

            mkdir("/mnt/usb1/PS5/Backup/system_data", 0777);
            mkdir("/mnt/usb1/PS5/Backup/system_data/priv", 0777);
            mkdir("/mnt/usb1/PS5/Backup/system_data/priv/mms", 0777);

            copy_file("/system_data/priv/mms/app.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/app.db");
            copy_file("/system_data/priv/mms/appinfo.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/appinfo.db");
            copy_file("/system_data/priv/mms/addcont.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/addcont.db");
            copy_file("/system_data/priv/mms/av_content_bg.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/av_content_bg.db");
            copy_file("/system_data/priv/mms/av_content.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/av_content.db");
            copy_file("/system_data/priv/mms/notification.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/notification.db");
            copy_file("/system_data/priv/mms/notification2.db", "/mnt/usb1/PS5/Backup/system_data/priv/mms/notification2.db");

            mkdir("/mnt/usb1/PS5/Backup/user", 0777);
            mkdir("/mnt/usb1/PS5/Backup/user/license", 0777);
            copy_file("/user/license/act.dat", "/mnt/usb1/PS5/Backup/user/license/act.dat");

            if (notify_user) {
                notify("Dumping Database files done\nPlease wait...");
            }

            sleep(7); 

            #define OCT_TO_MO / 1024 / 1024

            if (notify_user) {
                notify("Dumping Savegame Database files");
            }

            char* src_path = "/system_data/savedata_prospero";
            char* dst_path = "/mnt/usb1/PS5/Backup/system_data/savedata_prospero";

            mkdir("/mnt/usb1/PS5/Backup/system_data/savedata_prospero", 0777);

            if (dir_exists(src_path))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path);

                if (notify_user) {
                    notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path, (double)sizeCurrent OCT_TO_MO, sav);
                }

                sleep(7); 
                isxfer = 1;
                copy_dir(src_path, dst_path);
                isxfer = 0;
                sleep(7); 
                sizeCurrent = 0;

                check_size_folder_current(src_path);

                if (total_bytes_copied == sizeCurrent)
                {
                    if (notify_user) {
                        notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    }
                    sleep(7); 
                }
                else
                {
                    if (notify_user) {
                        notify("Some files and folders were not copied!");
                    }
                }

            }

            if (notify_user) {
                notify("Dumping Savegame Database files done\nPlease wait...");
            }

            sleep(7); 

            if (notify_user) {
                notify("Dumping User files");
            }

            char* src_path0 = "/user/home";
            char* dst_path0 = "/mnt/usb1/PS5/Backup/user/home";

            mkdir("/mnt/usb1/PS5/Backup/user/home", 0777);

            if (dir_exists(src_path0))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path0);

                if (notify_user) {
                    notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path0, (double)sizeCurrent OCT_TO_MO, sav);
                }

                sleep(7); 
                isxfer = 1;
                copy_dir(src_path0, dst_path0);
                isxfer = 0;
                sleep(7); 
                sizeCurrent = 0;

                check_size_folder_current(src_path0);

                if (total_bytes_copied == sizeCurrent)
                {
                    if (notify_user) {
                        notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path0, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    }
                    sleep(7); 
                }
                else
                {
                    if (notify_user) {
                        notify("Some files and folders were not copied!");
                    }
                }

            }

            if (notify_user) {
                notify("Dumping User files done\nPlease wait...");
            }


            sleep(7); 

            if (notify_user) {
                notify("Dumping User Audio and Video files");
            }

            char* src_path1 = "/user/av_contents";
            char* dst_path1 = "/mnt/usb1/PS5/Backup/user/av_contents";

            mkdir("/mnt/usb1/PS5/Backup/user/av_contents", 0777);

            if (dir_exists(src_path1))
            {
                total_bytes_copied = 0;
                sizeCurrent = 0;
                sav = 0;

                check_size_folder_current(src_path1);

                if (notify_user) {
                    notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path1, (double)sizeCurrent OCT_TO_MO, sav);
                }

                sleep(7); 
                isxfer = 1;
                copy_dir(src_path1, dst_path1);
                isxfer = 0;
                sleep(7); 
                sizeCurrent = 0;

                check_size_folder_current(src_path1);

                if (total_bytes_copied == sizeCurrent)
                {
                    if (notify_user) {
                        notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path1, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                    }
                    sleep(7); 
                }
                else
                {
                    if (notify_user) {
                        notify("Some files and folders were not copied!");
                    }
                }

            }

            if (notify_user) {
                notify("Dumping User Audio and Video files done");
            }

            sleep(7); 

            remove("/mnt/usb1/.probe");

            if (notify_user) {
                notify("Dump to USB1 done!");
            }

        }

    }

    else {

        if (notify_user) {
            notify("Dumping to USB0");
        }

        if (notify_user) {
            notify("Dumping Database files");
        }

        mkdir("/mnt/usb0/PS5", 0777);
        mkdir("/mnt/usb0/PS5/Backup", 0777);

        mkdir("/mnt/usb0/PS5/Backup/system_data", 0777);
        mkdir("/mnt/usb0/PS5/Backup/system_data/priv", 0777);
        mkdir("/mnt/usb0/PS5/Backup/system_data/priv/mms", 0777);

        copy_file("/system_data/priv/mms/app.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/app.db");
        copy_file("/system_data/priv/mms/appinfo.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/appinfo.db");
        copy_file("/system_data/priv/mms/addcont.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/addcont.db");
        copy_file("/system_data/priv/mms/av_content_bg.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/av_content_bg.db");
        copy_file("/system_data/priv/mms/av_content.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/av_content.db");
        copy_file("/system_data/priv/mms/notification.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/notification.db");
        copy_file("/system_data/priv/mms/notification2.db", "/mnt/usb0/PS5/Backup/system_data/priv/mms/notification2.db");

        mkdir("/mnt/usb0/PS5/Backup/user", 0777);
        mkdir("/mnt/usb0/PS5/Backup/user/license", 0777);
        copy_file("/user/license/act.dat", "/mnt/usb0/PS5/Backup/user/license/act.dat");

        if (notify_user) {
            notify("Dumping Database files done\nPlease wait...");
        }

        sleep(7); 

        #define OCT_TO_MO / 1024 / 1024

        if (notify_user) {
            notify("Dumping Savegame Database files");
        }

        char* src_path = "/system_data/savedata_prospero";
        char* dst_path = "/mnt/usb0/PS5/Backup/system_data/savedata_prospero";

        mkdir("/mnt/usb0/PS5/Backup/system_data/savedata_prospero", 0777);

        if (dir_exists(src_path))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path);

            if (notify_user) {
                notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path, (double)sizeCurrent OCT_TO_MO, sav);
            }

            sleep(7); 
            isxfer = 1;
            copy_dir(src_path, dst_path);
            isxfer = 0;
            sleep(7); 
            sizeCurrent = 0;

            check_size_folder_current(src_path);

            if (total_bytes_copied == sizeCurrent)
            {
                if (notify_user) {
                    notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                }
                sleep(7); 
            }
            else
            {
                if (notify_user) {
                    notify("Some files and folders were not copied!");
                }
            }

        }

        if (notify_user) {
            notify("Dumping Savegame Database files done\nPlease wait...");
        }

        sleep(7); 

        if (notify_user) {
            notify("Dumping User files");
        }

        char* src_path0 = "/user/home";
        char* dst_path0 = "/mnt/usb0/PS5/Backup/user/home";

        mkdir("/mnt/usb0/PS5/Backup/user/home", 0777);

        if (dir_exists(src_path0))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path0);

            if (notify_user) {
                notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path0, (double)sizeCurrent OCT_TO_MO, sav);
            }

            sleep(7); 
            isxfer = 1;
            copy_dir(src_path0, dst_path0);
            isxfer = 0;
            sleep(7); 
            sizeCurrent = 0;

            check_size_folder_current(src_path0);

            if (total_bytes_copied == sizeCurrent)
            {
                if (notify_user) {
                    notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path0, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                }
                sleep(7); 
            }
            else
            {
                if (notify_user) {
                    notify("Some files and folders were not copied!");
                }
            }

        }

        if (notify_user) {
            notify("Dumping User files done\nPlease wait...");
        }

        sleep(7); 

        if (notify_user) {
            notify("Dumping User Audio and Video files");
        }

        char* src_path1 = "/user/av_contents";
        char* dst_path1 = "/mnt/usb0/PS5/Backup/user/av_contents";

        mkdir("/mnt/usb0/PS5/Backup/user/av_contents", 0777);

        if (dir_exists(src_path1))
        {
            total_bytes_copied = 0;
            sizeCurrent = 0;
            sav = 0;

            check_size_folder_current(src_path1);

            if (notify_user) {
                notify("Total size: %s\n%.2fMb\nFolders: %d\nCopy start. Please wait...", src_path1, (double)sizeCurrent OCT_TO_MO, sav);
            }

            sleep(7); 
            isxfer = 1;
            copy_dir(src_path1, dst_path1);
            isxfer = 0;
            sleep(7); 
            sizeCurrent = 0;

            check_size_folder_current(src_path1);

            if (total_bytes_copied == sizeCurrent)
            {
                if (notify_user) {
                    notify("Copy of: %s\nSuccessfully\n%.2f/%.2fMb", src_path1, (double)total_bytes_copied OCT_TO_MO, (double)sizeCurrent OCT_TO_MO);
                }
                sleep(7); 
            }
            else
            {
                if (notify_user) {
                    notify("Some files and folders were not copied!");
                }
            }

        }

        if (notify_user) {
            notify("Dumping User Audio and Video files done");
        }

        sleep(7); 

        remove("/mnt/usb0/.probe");

        if (notify_user) {
            notify("Dump to USB0 done!");
        }

    }

    return 0;
}


int main() {

    notify_user = 1;

    if (notify_user) {
        notify("PS5 Database and User Backup\nVersion 1.1 (ElfLoader) by Storm");
    }

    sleep(7); 

    nthread_run = 1;
    pthread_t nthread;
    if (!pthread_create(&nthread, NULL, nthread_func, NULL)) {
        pthread_detach(nthread);
    }

    char* usb_mnt_path = getusbpath();
    if (usb_mnt_path == NULL)
    {
        do
        {
            if (notify_user) {
                notify("Please insert USB media in exfat/fat32 format");
            }

            sleep(7); 

            usb_mnt_path = getusbpath();

        } while (usb_mnt_path == NULL);
    }
    free(usb_mnt_path);

    start_backup(); 

    nthread_run = 0; 
    notify_user = 0; 

}

