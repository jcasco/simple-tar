/* jtar.c 
*
*  Josue Casco
*  CS360
*  10/10/14
*
*  Creates tars given files and creates files given a tar file.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <fields.h>
#include <dllist.h>
#include <jrb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <limits.h>

//reads directories and files given as stdin and outputs file info to stdout
void process_files(char * fileNm, JRB inodes, JRB rPaths, int prnt){
	struct stat sb;
	int status;
	DIR *d;
	struct dirent *de;
	Dllist dirTmp, tmp;
	char* tmpNm;
	FILE *file;
	char tmpPath[PATH_MAX];
	char * actPath;
	dirTmp = new_dllist();

	lstat(fileNm, &sb);
	
	actPath = realpath(fileNm, tmpPath);

	//skips softlinks and duplicates
	if( !S_ISLNK(sb.st_mode) && (jrb_find_str(rPaths, actPath) == NULL)){
		jrb_insert_str(rPaths, strdup(actPath), JNULL);
		
		//prints filename, stat struct, and contents to stdout
		if(S_ISREG(sb.st_mode) ){
			fwrite(fileNm, sizeof(char), strlen(fileNm), stdout); 
			fputc('\0',stdout);
			
			//write struct to stdout
			fwrite(&sb, sizeof(struct stat), 1, stdout);
		
			//reg file not hard linked to another file
			if(jrb_find_int(inodes, sb.st_ino) == NULL){
				jrb_insert_int(inodes, sb.st_ino, new_jval_s(fileNm));
				//cv option
				if(prnt == 1)
					fprintf(stderr, "File %s    %jd bytes\n",fileNm, sb.st_size);
				
				//read and write contents
				file = fopen(fileNm, "r");
				if(file == NULL){
					fprintf(stderr, "Could not open %s for writing\n", fileNm);
					exit(1);
				}
				char *fileCont = malloc(sb.st_size);
				fread(fileCont, sb.st_size, 1, file);
				//write contents to stdout
				fwrite(fileCont, sb.st_size, 1, stdout);
				fclose(file);
				
			}
			// hardlink 
			else if(jrb_find_int(inodes, sb.st_ino) != NULL){
				JRB rnode = jrb_find_int(inodes, sb.st_ino);
				char * lnkNm = rnode->val.s;
				//cv output
				if(prnt == 1)
					fprintf(stderr, "%s link to %s\n",fileNm, lnkNm);
			}
		}
		//insert dir contents into list
		else if(S_ISDIR(sb.st_mode)){
			//open directory
			d = opendir(fileNm);
			if( d == NULL){
				fprintf(stderr, "Could not open %s\n", fileNm);
				exit(1);
			}
			
			//write dir name and stat struct to stdout
			if(prnt ==1)
				fprintf(stderr, "Directory %s\n",fileNm);
			fwrite(fileNm, sizeof(char), strlen(fileNm), stdout); 
			fputc('\0',stdout);
			//write stat to stdout
			fwrite(&sb, sizeof(struct stat), 1, stdout);
			
			//read dir contentents
			for(de = readdir(d); de != NULL; de = readdir(d)){
				//check for self dir and parent dir
				if(strcmp(de->d_name, ".") && strcmp(de->d_name, "..")){
					//construct directory entry
					tmpNm = malloc(MAXLEN);
					tmpNm = strdup(fileNm);
					strcat(tmpNm, "/");
					strcat(tmpNm, de->d_name);
					
					//add files to dir list
					dll_append(dirTmp, new_jval_s(strdup(tmpNm)));
				}
			}
			closedir(d);
			//process each file in dir
			dll_traverse(tmp, dirTmp){
				process_files(tmp->val.s, inodes, rPaths, prnt);
			}
		}
	}
	//file is soft link
	else{
		if(prnt == 1)
			fprintf(stderr,"Ignoring Soft Link %s\n", fileNm);
	}

}

//reads tar contents and creates files
void create_files(int prnt){
	char c;
	JRB inodes = make_jrb(); 
	JRB dirStructs = make_jrb();
	JRB atimes = make_jrb();
	JRB mtimes = make_jrb();
	struct utimbuf utme;
	
	//reads tar file
	while((c=getchar()) != EOF){
		char *fileNm;
		fileNm = malloc (MAXLEN);
		int i=0;
		struct stat tmpStat;
		FILE *file;

		//read filename
		while(c != '\0'){
			fileNm[i] = c;
			i++;
			c=getchar();
		}
		fileNm[i] = '\0';

		//read struct
		fread(&tmpStat, sizeof(struct stat), 1, stdin);
		//fwrite(&tmpStat, sizeof(struct stat), 1, stdout);
			

		//make directory
		if(S_ISDIR(tmpStat.st_mode)){
			mkdir(fileNm, 0777);
			if(prnt == 1)
				fprintf(stderr,"Directory: %s\n", fileNm);

			jrb_insert_str(dirStructs, fileNm, new_jval_i(tmpStat.st_mode));
			jrb_insert_str(atimes, fileNm, new_jval_i(tmpStat.st_atime));	
			jrb_insert_str(mtimes, fileNm, new_jval_i(tmpStat.st_mtime));	
		}

		//make file
		else if(S_ISREG(tmpStat.st_mode)){
			//get access and mod times
			utme.actime = tmpStat.st_atime;
			utme.modtime = tmpStat.st_atime;
			
			//regfile
			if(jrb_find_int(inodes, tmpStat.st_ino) == NULL){
				jrb_insert_int(inodes, tmpStat.st_ino, new_jval_s(fileNm));
				
				//opens file for writing
				file = fopen(fileNm, "w");
				if(file == NULL){
					fprintf(stderr, "Could not open %s for writing\n", fileNm);
					exit(1);
				}
				if(prnt == 1)
					fprintf(stderr, "File: %s    %jd bytes\n", fileNm, tmpStat.st_size);
				
				//read file contents
				char *fileCont = malloc(tmpStat.st_size);
				fread(fileCont, tmpStat.st_size, 1, stdin);
				//write file contents
				fwrite(fileCont, tmpStat.st_size, 1, file);
				fclose(file);
				//set file permissions and access times
				chmod(fileNm, tmpStat.st_mode);
				utime(fileNm, &utme);
			}
			//hardlink
			else{
				//link is found
				JRB rnode = jrb_find_int(inodes, tmpStat.st_ino);	
				//create link
				link(rnode->val.s, fileNm);
				//set file permissions and access times
				utime(fileNm, &utme);
				if(prnt == 1)
					fprintf(stderr, "Link: %s to %s\n", fileNm, rnode->val.s);
			}
		}
	}
	//traverse dir tree to set permissions and access times
	JRB dirNode;
	jrb_traverse(dirNode, dirStructs){
		chmod(dirNode->key.s, dirNode->val.i);
		time_t atme, mtme;
		atme = jrb_find_str(atimes, dirNode->key.s)->val.i;
		mtme = jrb_find_str(mtimes, dirNode->key.s)->val.i;
		utme.actime = atme;
		utme.modtime = mtme;
		utime(dirNode->key.s, &utme);
	}

	jrb_free_tree(inodes);	
	jrb_free_tree(dirStructs);	
	jrb_free_tree(atimes);	
	jrb_free_tree(mtimes);	
}


int main(int argc, char **argv){
	int i;
	int prnt = 0;
	Dllist dirList;
	
	JRB inodes = make_jrb();
	JRB rPaths = make_jrb();

	//check arguments
	if(argc < 2){
		fprintf(stderr, "Usage: jtar c,x [filename] \n");
		exit(1);
	}
	
	//process on option
	if (!strcmp(argv[1],"c")){
		for(i = 2; i < argc ; i++){
			process_files(argv[i],inodes,rPaths, prnt);
		}
	}
	else if (!strcmp(argv[1],"cv")){
		prnt = 1;
		for(i = 2; i < argc ; i++){
			process_files(argv[i],inodes, rPaths, prnt);
		}
	}
	else if (!strcmp(argv[1],"x")){
			create_files(prnt);
	}
	else if (!strcmp(argv[1],"xv")){
		printf("going to print\n");
		prnt = 1;
			create_files(prnt);
	}
	else{
		fprintf(stderr, "Usage: jtar c,x [filename] \n");
		exit(1);
	}
	jrb_free_tree(inodes);	
	jrb_free_tree(rPaths);	
}
