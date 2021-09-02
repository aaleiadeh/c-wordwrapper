#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include "strbuf.h"

int wrap(unsigned width, int inputfd, int outputfd){
	assert(width > 0);
	int bytesread;
	char buf[256];
	int length = 0;
	strbuf_t wordbuf;
	sb_init(&wordbuf, 256);
	int newlinecount = 0;
	int status = 0;

	//Notes to self
	//2 Scenarios
	//1)We being iteration looking to complete partial word from last iteration
	//2)No partial word, iteration starts at beginning of new word
	while((bytesread = read(inputfd, buf, 10)) > 0){
		for(int i = 0; i < bytesread; i++){
			if(isspace(buf[i])){
				if(wordbuf.used > 0)//Contains a full word ready to write
				{
					if(length+wordbuf.used >= width && length != 0)//Length != 0 so words already exist on line. >= instead of > to account for space for last word
					{
						write(outputfd, "\n", 1);
						length = 0;
					}
					if(length != 0)//Printed a word before and need a space before next one
					{
						write(outputfd, " ", 1);
						length++;
					}
					if(wordbuf.used > width) status = 1;
					write(outputfd, wordbuf.data, wordbuf.used);
					length += wordbuf.used;
					sb_reset(&wordbuf);
				}
				if(buf[i] == '\n') newlinecount++;
				if(newlinecount == 2){
					write(outputfd, "\n\n", 2);
					newlinecount = 0;
					length = 0;
				}
			}
			else
			{
				sb_append(&wordbuf, buf[i]);
				newlinecount = 0;
			}
		}
	}
	write(outputfd, "\n", 1);
	sb_destroy(&wordbuf);
	return status;
}

int isfileordir(char *path){//1 = file. 2 = dir
	struct stat data;
	int err = stat(path, &data);
	if(err){
		perror(path);
		return 0;
	}

	if(S_ISREG(data.st_mode)) return 1;
	if(S_ISDIR(data.st_mode)) return 2;
	return 0;
}

int main(int argc, char *argv[]){
	int length = atoi(argv[1]);
	int status = 0;
	if(argc == 2){
		status = wrap(length, 0, 1);
		if(status) return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	int fod = isfileordir(argv[2]);
	if(fod == 1){
		int fd = open(argv[2], O_RDONLY);
		if(fd == -1){
			perror(argv[2]);
			return EXIT_FAILURE;
		}
		status = wrap(length, fd, 1);
		close(fd);
		if(status) return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if(fod == 2){
		int final = 0;
		DIR *dir = opendir(argv[2]);
		struct dirent *de;
		strbuf_t path;
		sb_init(&path, 256);
		sb_concat(&path, argv[2]);
		sb_append(&path, '/');

		strbuf_t create;
		sb_init(&create, 256);
		sb_concat(&create, path.data);
		sb_concat(&create, "wrap.");

		char *name;
		while((de = readdir(dir))){
			if(de->d_name[0] == '.' || strncmp(de->d_name, "wrap.", 5) == 0) continue;
			name = de->d_name;
			sb_concat(&path, name);
			if(isfileordir(path.data) == 2){
				sb_removen(&path, strlen(name));
				continue;
			}
			int ifd = open(path.data, O_RDONLY);
			if(ifd == -1){
				perror(path.data);
				return EXIT_FAILURE;
			}
			sb_concat(&create, name);
			int ofd = open(create.data, O_WRONLY|O_TRUNC|O_CREAT, 0644);
			if(ofd == -1){
				perror(create.data);
				return EXIT_FAILURE;
			}
			status = wrap(length, ifd, ofd);
			if(status) final = 1;
			sb_removen(&path, strlen(name));
			sb_removen(&create, strlen(name));
			close(ifd);
			close(ofd);
		}
		closedir(dir);
		sb_destroy(&path);
		sb_destroy(&create);
		if(final) return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}
