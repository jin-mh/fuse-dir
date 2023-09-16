#define FUSE_USE_VERSION 22
#define _FILE_OFFSET_BITS 64
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>

static int pfs_getattr(const char *path, struct stat *stbuf)
{ // set attribute
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));					   // stat 구조체 초기화
	if (!strcmp(&path[strlen(path) - strlen("/cwd")], "/cwd")) // cwd파일의 속성을
	{
		stbuf->st_mode = S_IFLNK | 0444; // 심볼릭링크파일, 권한은 444로 지정
		stbuf->st_nlink = 1;			 // 링크의 개수를 1개로 지정
	}
	else if (!strcmp(&path[strlen(path) - strlen("/exe")], "/exe")) // exe파일의 속성을
	{
		char exebuf[256]; // 심볼릭 링크 exe를 얻기 위한 버퍼

		stbuf->st_mode = S_IFLNK | 0444; // 심볼릭링크파일, 권한은 444로 지정
		stbuf->st_nlink = 1;			 // 링크의 개수를 1개로 지정
										 // memset(exebuf, 0, sizeof(exebuf)); // 심볼릭 링크 exe를 얻기 위한 버퍼 초기화
										 /*pfs_readlink("/proc/1894/exe", exebuf, sizeof(exebuf));//get exec file path
										 stat(exebuf, exestat);			//get file info from path
										 stbuf->st_size = exestat->st_size;*/
										 // exe파일 크기 지정
	}
	else if (path[0] == '/')
	{									 // path가 '/'로 시작하면
		stbuf->st_mode = S_IFDIR | 0755; // 파일 종류는 디렉토리, 권한은 755로 지정
		stbuf->st_nlink = 2;			 // 링크 개수는 2개로 지정
										 // stbuf->st_size = ;			//디렉토리 크기 지정
	}
	else			   // 이외에는
		res = -ENOENT; // 파일을 찾을 수 없음

	stbuf->st_uid = getuid(); // 파일의 소유자id 지정
	stbuf->st_gid = getgid(); // 파일의 그룹id 지정
	return res;
}

static int pfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	DIR *dir;			  //  /proc/pid/ 를 가리킬 DIR* 포인터
	struct dirent *entry; // 각 파일의 inode를 통해 파일을 선택할 dirent 구조체 포인터
	struct stat fileStat; // 파일의 정보를 담는 구조체

	int pid, ppid, cpid, i; // 현재 탐색하는 디렉토리의 pid, 부모 pid,
							// 현재 있는 디렉토리의 pid
	char pname[256], pathbuf[256], pidname[256], tmp;
	FILE *fp;

	(void)offset;
	(void)fi;

	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;

	filler(buf, ".", NULL, 0);	// 디렉토리 자신을 가리키는 파일 생성
	filler(buf, "..", NULL, 0); // 상위 디렉토리를 가리키는 파일 생성

	filler(buf, "cwd", NULL, 0); // cwd파일 생성
	filler(buf, "exe", NULL, 0); // exe파일 생성

	dir = opendir("/proc"); // /proc 디렉토리 open

	cpid = 1; // cpid를 1(init 프로세스)로 초기화
	sscanf(strrchr(path, '/') + 1, "%d", &cpid);
	// path에서 현재 쉘에서 탐색중인 디렉토리의 pid(cpid)를 구함

	while ((entry = readdir(dir)) != NULL) //   /proc에 존재하는 파일들을 차례대로 읽음
	{
		lstat(entry->d_name, &fileStat); // DIR* 포인터의 state 정보를 가져옴

		if (!S_ISDIR(fileStat.st_mode)) // 디렉토리면
			continue;
		pid = atoi(entry->d_name); // 디렉토리 이름이 pid이므로 pid를 얻음
		if (pid <= 0)			   // 0보다 작으면(양수가 아니면)
			continue;			   // 다시 시작

		sprintf(pathbuf, "/proc/%d/stat", pid); // pid로 stat파일의 경로를 구함
		fp = fopen(pathbuf, "r");				// stat 파일 오픈
		memset(pname, 0, sizeof(pname));		// 프로세스 이름을 얻기위한 버퍼 초기화
		fscanf(fp, "%d%s%s%d", (int *)&tmp, pname, &tmp, &ppid);
		// 프로세스 이름, 프로세스의 부모id 얻기
		fclose(fp);								 // stat파일 닫음
		pname[strlen(pname) - 1] = 0;			 // 프로세스 이름의 맨 뒤 삭제
		memcpy(pname, &pname[1], strlen(pname)); // pname 맨 앞 삭제

		for (i = 0; i < strlen(pname); i++) // 프로세스 이름에 /가 있으면 –로 대체
			if (pname[i] == '/')
				pname[i] = '-';

		memset(pidname, 0, sizeof(pidname));   // 디렉토리 이름인 pid-name을 구하기 위한 버퍼 초기화
		sprintf(pidname, "%d-%s", pid, pname); // pid와 pname으로 pidname 구함

		if (ppid == cpid) // 쉘에서 탐색중인 pid인 cpid와 while에서 탐색중인 ppid가 같은 것을 목록으로 출력
			filler(buf, pidname, NULL, 0);
	}
	closedir(dir); // 디렉토리 닫기
	return 0;
}

static int pfs_open(const char *path, struct fuse_file_info *fi)
{
	return -ENOENT;
}

static int pfs_readlink(const char *path, char *dest, size_t len)
{
	FILE *fp;
	char pathbuf[256], buff[256];
	int i, cnt, symlink;
	cnt = 0;
	for (i = strlen(path); i > 0; i--)
		if (path[i] == '/')
		{
			cnt++;
			if (cnt >= 2)
				break;
		}
	sprintf(pathbuf, "/proc/%s", path + i + 1);
	char *pos;
	pos = strrchr(pathbuf, '-');
	if (pos != NULL && pos < strrchr(pathbuf, '/'))
		strcpy(strrchr(pathbuf, '-'), strrchr(pathbuf, '/'));

	memset(buff, 0, sizeof(buff));
	symlink = readlink(pathbuf, buff, sizeof(buff));

	if (symlink > 0)
	{
		strcpy(dest, buff);
		return 0;
	}
	else
		return -ENOENT;

	return 0;
}

static int pfs_rmdir(const char *path)
{
	int cpid;
	sscanf(strrchr(path, '/') + 1, "%d", &cpid);
	return kill(cpid, SIGKILL);
}

static struct fuse_operations pfs_oper = {
	.getattr = pfs_getattr,
	.readdir = pfs_readdir,
	.open = pfs_open,
	.readlink = pfs_readlink,
	.rmdir = pfs_rmdir};

int main(int argc, char **argv)
{
	return fuse_main(argc, argv, &pfs_oper);
}