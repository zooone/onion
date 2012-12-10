/*
	Onion HTTP server library
	Copyright (C) 2010 David Moreno Montero

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

void print_help();
char *funcname(const char *prefix, const char *filename);
void parse_file(const char *prefix, const char *filename, FILE *outfd);
void parse_directory(const char *prefix, const char *dirname, FILE *outfd);

int main(int argc, char **argv){
	if (argc==1)
		print_help(argv[0]);
	int i;
	char *outfile=NULL;
	// First pass cancel out the options, let only the files
	for (i=1;i<argc;i++){
		if (strcmp(argv[i],"--help")==0)
			print_help();
		if (strcmp(argv[i],"-o")==0){
			if (i>=argc-1){
				fprintf(stderr,"ERROR: Need an argument for -o");
				exit(2);
			}
			outfile=strdup(argv[i+1]);
			argv[i]=NULL; // cancel them out.
			argv[i+1]=NULL; 
			i++;
		}
	}
	
	FILE *outfd=stdout;
	if (outfile){
		outfd=fopen(outfile,"w");
		if (!outfd){
			perror("ERROR: Could not open output file");
			exit(2);
		}
	}
	
	// Some header...
	fprintf(outfd,"/** File autogenerated by opack **/\n\n");
  fprintf(outfd,"#include <onion/request.h>\n\n");
	fprintf(outfd,"#include <onion/response.h>\n\n");
  fprintf(outfd,"#include <string.h>\n\n");
	
	for (i=1;i<argc;i++){
		if (argv[i]){
      struct stat st;
      stat(argv[i], &st);
      if (S_ISDIR(st.st_mode)){
        parse_directory(basename(argv[1]), argv[i], outfd);
      }
      else{
        parse_file("", argv[i], outfd);
      }
		}
	}
	
	if (outfile){
		free(outfile);
		fclose(outfd);
	}
	
	return 0;
}

/// Parses a filename to a function name (changes non allowed chars to _)
char *funcname(const char *prefix, const char *filename){
  int l=8;
  if (prefix)
    l+=strlen(prefix);
  if (filename)
    l+=strlen(filename);
	char *ret=malloc(l);
	strcpy(ret,"opack_");
  if (prefix && prefix[0]!='\0')
    strcat(ret, prefix);
  if (prefix && prefix[0]!='\0' && filename && filename[0]!='\0')
    strcat(ret, "_");

  if (filename && filename[0]!='\0')
    strcat(ret, filename);
	int i=0;
	while(ret[i]!='\0'){
		if ( !((ret[i]>='a' && ret[i]<='z') || (ret[i]>='A' && ret[i]<='Z') || (ret[i]>='0' && ret[i]<='9')) )
			ret[i]='_';
		i++;
	}
	return ret;
}

/**
 * @short Generates the necesary data to the output stream.
 */
void parse_file(const char *prefix, const char *filename, FILE *outfd){
	FILE *fd=fopen(filename, "r");
	if (!fd){
		fprintf(stderr,"ERROR: Cant open file %s",filename);
		perror("");
		exit(3);
	}
	char *fname=funcname(prefix, basename((char*)filename));
	char buffer[4097];
	buffer[4096]=0;
	
	fprintf(stderr, "Parsing: %s to 'onion_connection_status %s(void *_, onion_request *req, onion_response *res);'.\n",filename, fname);
	fprintf(outfd,"onion_connection_status %s(void *_, onion_request *req, onion_response *res){\n  static const char data[]={\n",fname);
	int r, i, l=0;
	while ( (r=fread(buffer,1,sizeof(buffer)-1,fd)) !=0 ){
		for (i=0;i<r;i++){
			fprintf(outfd,"0x%02X, ", buffer[i]&0x0FF);
			if ((i%16) == 15){
				fprintf(outfd,"\n");
			}
		}
		l+=r;
	}
	fprintf(outfd,"};\n");

  fprintf(outfd,"  onion_response_set_length(res, %d);\n", l);
	fprintf(outfd,"  return onion_response_write(res, data, sizeof(data));\n}\n\n");

	fprintf(outfd,"const unsigned int %s_length = %d;\n\n",fname,l);
	
	fclose(fd);
	free(fname);
}

/**
 * @short Bulk converts all files at dirname, excepting *~, and the creates a handler for such directory.
 */
void parse_directory(const char *prefix, const char *dirname, FILE *outfd){
  DIR *dir=opendir(dirname);
  if (!dir){
    fprintf(stderr, "ERROR: Could not open directory %s, check permissions.", dirname);
    exit(4);
  }
  // First create other files/dirs
  struct dirent *de;
  char fullname[256];
  while ( (de=readdir(dir)) ){
    if (de->d_name[0]=='.' || de->d_name[strlen(de->d_name)-1]=='~')
      continue;
    snprintf(fullname, sizeof(fullname), "%s/%s", dirname, de->d_name);
    if (de->d_type==DT_DIR){
      char prefix2[256];
      snprintf(prefix2, sizeof(prefix2), "%s/%s", prefix, de->d_name);
      parse_directory(prefix2, fullname, outfd);
    }
    else
      parse_file(prefix, fullname, outfd);
  }
  closedir(dir);
  // Now create current
  char *fname=funcname(prefix, NULL);
  fprintf(stderr, "Parsing directory: %s to 'onion_connection_status %s(void *_, onion_request *req, onion_response *res);'.\n",dirname, fname);
  fprintf(outfd,"onion_connection_status %s(void *_, onion_request *req, onion_response *res){\n", fname);
  fprintf(outfd,"  const char *path=onion_request_get_path(req);\n\n");

  dir=opendir(dirname);
  while ( (de=readdir(dir)) ){
    if (de->d_name[0]=='.' || de->d_name[strlen(de->d_name)-1]=='~')
      continue;
    char *fname=funcname(prefix, de->d_name);
    if (de->d_type==DT_DIR){
      int l=strlen(de->d_name);
      fprintf(outfd, "  if (strncmp(\"%s/\", path, %d)==0){\n", de->d_name, l+1);
      fprintf(outfd, "    onion_request_advance_path(req, %d);\n", l+1);
    }
    else
      fprintf(outfd, "  if (strcmp(\"%s\", path)==0){\n", de->d_name);
    fprintf(outfd, "    return %s(_, req, res);\n", fname);
    fprintf(outfd, "  }\n");
    free(fname);
  }
  closedir(dir);
  
  
  
  fprintf(outfd,"  return OCS_NOT_PROCESSED;\n");
  fprintf(outfd,"}\n\n");
  free(fname);
}

/// Shows the help
void print_help(const char *name){
	fprintf(stderr,"%s -- Packs a given file or files into a C function that will print it out\n\n", name);
	fprintf(stderr,"Usage: %s <file1> <file2> -o <outfile.c> | --help\n\n", name);
  fprintf(stderr,"       %s <dir1> <dir2> <file3> -o <outfile.c> | --help\n\n", name);
	fprintf(stderr,"It later creates a series of functions, with the name of the file or directory, and with the following signature.\n");
	fprintf(stderr,"   int opacked_[file_extension](onion_response *response);\n\n");
	fprintf(stderr,"This way this function is very easily used from onion handlers as needed.\n");
  fprintf(stderr,"If its a directory, access is as expected using the path, but only last element: static/jquery.min.js, for example if you pack static with jquery.min.sj at src/static/. It is recursive.\n");
  fprintf(stderr,"At directory mode, files ending with ~ and starting with . are ignored.");
	exit(1);
}

