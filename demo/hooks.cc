#include <syscall.h>
#include <libsyscall_intercept_hook_point.h>
#include <sys/stat.h>
#include <string>
#include <dlfcn.h>
#include <sys/types.h>
#include <assert.h>

#include "hooks.h"

extern "C" {
#include <dirent.h> // used for file types in the getdents{,64}() functions
#include <linux/kernel.h> // used for definition of alignment macros
#include <linux/const.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
}

/*
 * linux_dirent is used in getdents() but is privately defined in the linux kernel: fs/readdir.c.
 */
struct linux_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[1];
};

/*
 * linux_dirent64 is used in getdents64() and defined in the linux kernel: include/linux/dirent.h.
 * However, it is not part of the kernel-headers and cannot be imported.
 */
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[1]; // originally `char d_name[0]` in kernel, but ISO C++ forbids zero-size array 'd_name'
};

/*
 * Macro used within getdents{,64} functions.
 * __ALIGN_KERNEL defined in linux/kernel.h
 */
#define ALIGN(x, a)                     __ALIGN_KERNEL((x), (a))

// TODO: 参数的含义
int hook_openat(int dirfd, const char* cpath, int flags, mode_t mode, long *res) {
    if(flags & O_PATH || flags & O_APPEND || flags & O_EXCL) {
        *res = -ENOTSUP;
         return 1;
    }
    if(dirfd != AT_FDCWD) {
        return 1;
    }

    if(cpath[0] != '/') {
        return 1;
    }

    printf("open dirfd = %d, cpath = %s\n", dirfd, cpath);
    *res = 0;
    return 0;
}

// TODO: 读写文件如果需要sync则需要更新元数据
int hook_fsync(int fd, long *res) {
    if(fd < START_FD) {
        return 1;
    }
    printf("fsync fd = %d\n", fd);
    *res = 0;
    return 0;
}


int hook_close(int fd, long *res) {
    if(fd < START_FD) {
        return 1;
    }

    printf("close fd = %d\n", fd);
    *res = 0;
    return 0;
}

int hook_mkdirat(int dirfd, const char *path, mode_t mode, long *res) {
    if(dirfd != AT_FDCWD) {
        return 1;
    }
    long fd;
    int was_hooked = hook_openat(dirfd, path, O_CREAT | O_DIRECTORY, mode, &fd);
    if (was_hooked) {
        return 1;
    }
	// hook_close((int)fd, res);
    *res = 0;
	return 0;
}

int hook_statfs(const char *path, struct statfs *sf, long *res) {

    // TODO: RPC get fs info
    sf->f_type = 0;
    sf->f_bsize = 0;
    sf->f_blocks = 40960; // TODO: need to modify
    sf->f_bfree = 40960;
    sf->f_bavail = sf->f_bfree;
    sf->f_files = 0;
    sf->f_ffree = (unsigned long)-1;
    sf->f_fsid = {0, 0};
    sf->f_namelen = 0;
    sf->f_frsize = 0;
	sf->f_flags = ST_NOSUID | ST_NODEV;

    printf("stat path = %s\n", path);
    *res = 0;
    return 0;
}


// 删除目录和文件都使用这个函数
// 服务器判断是文件还是目录，决定是否删除
int hook_unlinkat(int dirfd, const char *cpath, int flags, long *res) {
    // cpath need has consistent prefix with mount_dir

    if(dirfd != AT_FDCWD) {
        return 1;
    }

    printf("open unlink = %d, cpath = %s\n", dirfd, cpath);
    *res = 0;
    return 0;
}

int hook_stat(const char *cpath, struct stat *st, long *res) {

    printf("stat cpath = %s\n", cpath);
    *res = 0;
    return 0;
}


int hook_fstat(int fd, struct stat *st, long *res) {
    printf("fstat fd = %d\n", fd);
    *res = 0;
    return 0;
}

int hook_access(const char *path, int mask, long *res) {
    printf("access path = %s\n", path);
    *res = 0;
    return 0;
}

int hook_read(int fd, void *buf, size_t len, long *res) {
    printf("read fd = %d", fd);
    *res = 0;
    return 0;
}

int hook_write(int fd, const char *buf, size_t len, long *res) {
    if(fd < START_FD) {
        return 1;
    }
    printf("write fd = %d\n", fd);
    *res = 0;
    return 0;
}

int hook_lseek(int fd, long off, int flag, long *res) {
    printf("lseek fd = %d", fd);
    *res = 0;
    return 0;
}

int hook_getdents(int fd, struct linux_dirent *dirp, int count, long *res) {
    printf("hook %s\n", __func__);
    return 0;
}

int hook_getdents64(int fd, struct linux_dirent64 *dirp, int count, long *res) {
    printf("hook %s\n", __func__);
    *res = 0;
    return 0;
}

// 返回1表示
// mkdir时调用的似乎是SYS_create
// opendir内部还会调用SYS_close
int hook(long syscall_number,
                long a0, long a1,
                long a2, long a3,
                long a4, long a5,
                long *res) {
    switch(syscall_number) {
        case SYS_open:
            printf("SYS_open\n");
            return hook_openat(AT_FDCWD, (char *)a0, (int)a1, (mode_t)a2, res);
        case SYS_creat:
            printf("SYS_create\n");
            return hook_openat(AT_FDCWD, (char *)a0, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)a1, res);
        case SYS_openat:
            printf("SYS_openat\n");
            return hook_openat((int)a0, (char *)a1, (int)a2, (mode_t)a3, res);
        case SYS_close:
            printf("SYS_close, fd: %d\n", (int)a0);
            return hook_close((int)a0, res);
        case SYS_write:
            printf("SYS_write, fd: %d\n", (int)a0);
            return hook_write((int)a0, (char *)a1, (size_t)a2, res);
        case SYS_read:
            // FS_LOG("SYS_read, fd: %d", (int)a0);
            return hook_read((int)a0, (void *)a1, (size_t)a2, res);
        case SYS_lseek:
            printf("SYS_lseek\n");
            return hook_lseek((int)a0, a1, (int)a2, res);
        case SYS_fsync:
            printf("SYS_fsync\n");
            return hook_fsync((int)a0, res);
        case SYS_stat:
            printf("SYS_stat\n");
            return hook_stat((const char *)a0, (struct stat *)a1, res);
        case SYS_fstat:
            printf("SYS_fstat\n");
            return hook_fstat((int)a0, (struct stat*)a1, res);
        case SYS_mkdirat:
            printf("SYS_mkdirat\n");
            return hook_mkdirat((int)a0, (const char *)a1, (mode_t)a2, res);
        case SYS_mkdir:
            printf("SYS_mkdir\n");
            return hook_mkdirat(AT_FDCWD, (const char *)a0, (mode_t)a1, res);
        case SYS_statfs:
            printf("SYS_statfs\n");
            return hook_statfs((const char *)a0, (struct statfs *)a1, res);
        case SYS_access:
            printf("SYS_access\n");
            return hook_access((const char *)a0, (int)a1, res);
        case SYS_unlink:
            printf("SYS_unlink\n");
            return hook_unlinkat(AT_FDCWD, (const char *)a0, 0, res);
        case SYS_rmdir:
            printf("SYS_rmdir\n");
            return hook_unlinkat(AT_FDCWD, (const char *)a0, AT_REMOVEDIR, res);
        case SYS_getdents:
            printf("SYS_getdents\n");
            return hook_getdents((int)a0, (linux_dirent *)a1, (int)a2, res);
        case SYS_getdents64:
            printf("SYS_getdents64\n");
            return hook_getdents64((int)a0, (linux_dirent64 *)a1, (int)a3, res);
        // case SYS_rename:
        //     return hook_renameat(AT_FDCWD, (const char))
        // case SYS_renameat:

        default:
            // FS_LOG("SYS_unhook: %d", syscall_number);
            assert(syscall_number != SYS_fork && syscall_number != SYS_vfork);
            return 1;
    }
    assert(false);
    return 0;
}

int metafs_hook(long syscall_number,
                long a0, long a1,
                long a2, long a3,
                long a4, long a5,
                long *res) {
    thread_local static int reentrance_flag = false;
	int oerrno, was_hooked;
	if (reentrance_flag)
	{
		// FS_LOG("internal sys call %ld", syscall_number);
		return 1;
	}
	reentrance_flag = true;
	oerrno = errno;
	was_hooked = hook(syscall_number, a0, a1, a2, a3, a4, a5, res);
	errno = oerrno;
	reentrance_flag = false;
    // printf("hook ok\n");
	return was_hooked;
}

// rewrite syscall function, faster than hook.=_=实际是人为减少了一些Syscall
#if HOOK_REWRITE
bool init_rewrite_flag = false;

int rmdir(const char *path)
{
	static int (*real_rmdir)(const char *path) = NULL;
	if (unlikely(real_rmdir == NULL))
	{
		real_rmdir = (typeof(real_rmdir))dlsym(RTLD_NEXT, "rmdir");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_unlink(path, &res))
		return res;
	return real_rmdir(path);
}

int unlink(const char *path)
{
	static int (*real_unlink)(const char *path) = NULL;
	if (unlikely(real_unlink == NULL))
	{
		real_unlink = (typeof(real_unlink))dlsym(RTLD_NEXT, "unlink");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_unlink(path, &res))
		return res;
	return real_unlink(path);
}

int stat(const char *path, struct stat *buf)
{
	static int (*real_stat)(const char *path, struct stat *buf) = NULL;
	if (unlikely(real_stat == NULL))
	{
		real_stat = (typeof(real_stat))dlsym(RTLD_NEXT, "stat");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_stat(path, buf, &res))
		return res;
	return real_stat(path, buf);
}

int fsync(int fd)
{
	static int (*real_sync)(int fd) = NULL;
	if (unlikely(real_sync == NULL))
	{
		real_sync = (typeof(real_sync))dlsym(RTLD_NEXT, "fsync");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_fsync(fd, &res))
		return res;
	return real_sync(fd);
}

off_t lseek(int fd, off_t offset, int whence)
{
	static int (*real_seek)(int fd, off_t offset, int whence) = NULL;
	if (unlikely(real_seek == NULL))
	{
		real_seek = (typeof(real_seek))dlsym(RTLD_NEXT, "lseek");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_lseek(fd, offset, whence, &res))
		return res;
	return real_seek(fd, offset, whence);
}

ssize_t read(int fd, void *buf, size_t siz)
{
	static int (*real_read)(int fd, void *buf, size_t siz) = NULL;
	if (unlikely(real_read == NULL))
	{
		real_read = (typeof(real_read))dlsym(RTLD_NEXT, "read");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_read(fd, (char *)buf, siz, &res))
		return res;
	return real_read(fd, buf, siz);
}

ssize_t write(int fd, const void *buf, size_t siz)
{
	static int (*real_write)(int fd, const void *buf, size_t siz) = NULL;
	if (unlikely(real_write == NULL))
	{
		real_write = (typeof(real_write))dlsym(RTLD_NEXT, "write");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_write(fd, (const char *)buf, siz, &res))
		return res;
	return real_write(fd, buf, siz);
}

int close(int fd)
{
	static int (*real_close)(int fd) = NULL;
	if (unlikely(real_close == NULL))
	{
		real_close = (typeof(real_close))dlsym(RTLD_NEXT, "close");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_close(fd, &res))
		return res;
	return real_close(fd);
}

int create(const char *path, mode_t mode)
{
	static int (*real_create)(const char *path, mode_t mode) = NULL;
	if (unlikely(real_create == NULL))
	{
		real_create = (typeof(real_create))dlsym(RTLD_NEXT, "create");
	}
	long res;
	if (likely(init_rewrite_flag) &&
			0 == hook_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, mode | S_IFREG, &res))
		return res;
	return real_create(path, mode);
}

int openat(int fd, const char *path, int oflag, ...)
{
	static int (*real_openat)(int fd, const char *path, int oflag, ...) = NULL;
	if (unlikely(real_openat == NULL))
	{
		real_openat = (typeof(real_openat))dlsym(RTLD_NEXT, "openat");
	}
	mode_t mode = 0;
	int was_hooked;
	long res;
	if (oflag & O_CREAT)
	{
		va_list argptr;
		va_start(argptr, oflag);
		mode = va_arg(argptr, mode_t);
		va_end(argptr);
	}
	if (likely(init_rewrite_flag) &&
			0 == hook_openat(fd, path, oflag, mode | S_IFREG, &res))
		return res;
	if (oflag & O_CREAT)
		return real_openat(fd, path, oflag, mode);
	else
		return real_openat(fd, path, oflag);
}

int open(const char *path, int oflag, ...)
{
	static int (*real_open)(const char *path, int oflag, ...) = NULL;
	if (unlikely(real_open == NULL))
	{
		real_open = (typeof(real_open))dlsym(RTLD_NEXT, "open");
	}
	mode_t mode = 0;
	long res;
	if (oflag & O_CREAT)
	{
		va_list argptr;
		va_start(argptr, oflag);
		mode = va_arg(argptr, mode_t);
		va_end(argptr);
	}
	if (likely(init_rewrite_flag) && 0 == hook_openat(AT_FDCWD, path, oflag, mode | S_IFREG, &res))
		return res;

	if (oflag & O_CREAT)
		return real_open(path, oflag, mode);
	else
		return real_open(path, oflag);
}

#endif
