/*
bin2h for Nintendont (Kernel)

Copyright (C) 2014 FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
int main (int argc, char **argv)
{
	if(argc < 2 || strrchr(argv[1], '.') == NULL || strcmp(".bin", strrchr(argv[1], '.')) != 0)
		return 0;
	/* read in file */
	FILE *f = fopen(argv[1], "rb");
	if(f == NULL)
		return 0;
	fseek(f,0,SEEK_END);
	size_t fsize = ftell(f);
	rewind(f);
	unsigned char *bin = (unsigned char*)malloc(fsize);
	fread(bin,fsize,1,f);
	fclose(f);
	/* generate names */
	size_t namelen = strrchr(argv[1], '.') - argv[1];
	char *newname = calloc(namelen+3, sizeof(char));
	char *basename = calloc(namelen+1, sizeof(char));
	strncpy(basename, argv[1], namelen);
	strncpy(newname, basename, namelen);
	strcpy(newname+namelen, ".h");
	/* get creation time */
	time_t curtime = time (NULL);
	struct tm *loctime = localtime (&curtime);
	/* create .h file */
	f = fopen(newname, "w");
	free(newname);
	fputs("/*\n",f);
	fprintf(f,"\tFilename    : %s\n",argv[1]);
	fprintf(f,"\tDate created: %s", asctime (loctime));
	fputs("*/\n\n",f);
	fprintf(f,"#define %s_size 0x%x\n\n",basename,fsize);
	fprintf(f,"const unsigned char %s[] = {",basename);
	size_t i = 0;
	while(i < fsize)
	{
		if((i % 16) == 0)
			fputs("\n",f);
		if((i % 4) == 0)
			fputs("\t",f);
		fprintf(f,"0x%02X", *(bin+i));
		i++;
		if(i < fsize)
			fputs(", ",f);
	}
	fprintf(f,"\n};\n");
	fclose(f);
	free(bin);
	return 0;
}
