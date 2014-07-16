/*	$OpenBSD: server.c,v 1.33 2014/07/12 03:10:03 guenther Exp $	*/

#include <dirent.h>

static int setownership(char *, int, uid_t, gid_t, int);
setownership(char *file, int fd, uid_t uid, gid_t gid, int islink)
	if (islink)
		status = lchown(file, uid, gid);
	if (fd != -1 && !islink)
		status = fchown(fd, uid, gid);

	if (status < 0 && !islink)
		status = chown(file, uid, gid);
		if (uid == (uid_t)-1)
setfilemode(char *file, int fd, int mode, int islink)
	if (islink)
		status = fchmodat(AT_FDCWD, file, mode, AT_SYMLINK_NOFOLLOW);
	if (fd != -1 && !islink)
	if (status < 0 && !islink)
	uid_t uid;
	gid_t gid;
	gid_t primegid = (gid_t)-2;
			uid = (uid_t) atoi(owner + 1);
			gid = (gid_t)atoi(group + 1);
	gid = (gid_t)-1;
		gid = (gid_t)-1;
	static struct dirent *dp;
		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
	struct dirent *dp;
		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		(void) sendcmd(QC_YES, "%lld %lld %o %s %s",
			       (long long) stb.st_size,
			       (long long) stb.st_mtime,
	debugmsg(DM_CALL, "chkparent(%s, %lo) start\n", name, opts);
					 "chkparent(%s, %lo) mkdir fail: %s\n",
	if (setfiletime(new, time(NULL), mtime) < 0)
		static const char fmt[] = "%s -> %s: rename failed: %s";
	mtime = (time_t) strtoll(cp, &cp, 10);
	atime = (time_t) strtoll(cp, &cp, 10);
		 "recvit: opts = %04lo mode = %04o size = %lld mtime = %lld",
		 opts, mode, (long long) size, (long long)mtime);
		 "dochmog: opts = %04lo mode = %04o", opts, mode);