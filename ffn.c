//
//    ffn.c -- fix file names
//
//
//    2003/08/13 finally collected together all my various program peices 
//    2017/01/23 created, rewrite from perl (fnp) to C (ffn)
//    2018/01/13 first release
//    2018/03/26 changed verbose to show files that do not need name changes
//    2020/03/06 fixed null char to null pointer at end of instructions
//

// Copyright © 2018 John Lockett lockett@nbnet.nb.ca 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define VERSION "1.00"

#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

// for regexs
#include <regex.h>

// for tolower
#include <ctype.h>

// for file attribute 
#include <sys/stat.h>
#include <sys/types.h>
 
// for basename/pathname
#include <libgen.h>

// for PATH_MAX
#include <linux/limits.h>

#define FALSE 0
#define TRUE !FALSE 

// do-while groups statements and avoid trailing semicolon problems
#define dbprintf(fmt, ...) do { if (debug) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PATHNAMEMAX PATH_MAX  
#define FILENAMEMAX 256     // I beleive this is max filname length in liunx
#define FILEMAX 4096        // max number of files from stdin

// internal file type definitions
#define REGULAR_FILE 101
#define DIRECTORY 102
#define SYMLINK 103


// global variables
char *program_name;

// Option Flags
int verbose = FALSE;
int debug   = FALSE;
int help    = FALSE;
int version = FALSE;
int dryrun  = FALSE;
int dirs    = FALSE;        // default do not rename directories
int dots    = TRUE;         // default change dots to underscores
int lower   = TRUE;         // default change all names to lowercase

struct option long_options[] =
     {
       {"help",      no_argument, &help,    TRUE},
       {"version",   no_argument, &version, TRUE},
       {"verbose",   no_argument, &verbose, TRUE},
       {"dryrun",    no_argument, &dryrun,  TRUE},
       {"debug",     no_argument, &debug,  TRUE},
       {"dirs",      no_argument, 0,        'D'},
       {"nodots",    no_argument, 0,        'd'},
       {"nolower",   no_argument, 0,        'l'},
       {NULL, 0, NULL, 0}
     };

char *instructions[] = {
    " fix file names: remove spaces and non-alphanumber characters",
    " ",
    " -D   --dirs      : rename directores, default is regular files only",
    " -d   --nodots    : do NOT remove dots in name, default is change dots to underscore",
    " -l   --nolower   : do NOT convert name to lower case, default is change name to lowercase",
    "      --dryrun    : dryrun (test), only display name changes",
    "      --verbose   : enable verbose output",
    "      --help      : display help and exit",
    "      --version   : print version information and exit",
    "      --debug     : enable debug output",
    " ",
    " Rename filenames (my rules)",
    "   1 convert all non-whitelist char to underscores",
    "       whitelist = digits (0-9), letters (a-zA-Z), underscore, dash, period/dots",
    "   2 optionally convert uppercase to lowercase",
    "   3 convert spaces to underscores",
    "   4 optionally change dots to underscores, except leading one",
    "   5 remove multiple underscores",
    "   6 remove leading dashes",
    " ",
    " Examples:",
    "   ffn *",
    "        : rename all files in the current directory",
    " ",
    "   ffn --dryrun -D -- *",
    "        : test run rename all files and directories in the current directory",
    " ",
    "   find sampledir -print | ffn --dryrun ",
    "        : recursivey rename all files in sampledir directory",
    " ",
    "                                                     john 2017/12",
    NULL
    };


/* Print Usage Instructions */
void inst(char *iptr[], int status)
{
    if(status != 0) {
        fprintf(stderr,"Try, %s --help for more information.\n", program_name);
        fprintf(stderr,"\nIf you have a filename starting with dash.\n");
        fprintf(stderr,"Try, %s [options] -- {file list} \n", program_name);
    } else {
        if (version) {
                printf("%s %s \n", program_name, VERSION);
                exit(0);
            }

        printf("Usage: %s [OPTION]... [FILE]...\n", program_name);  
        while (*iptr != NULL)
          puts(*iptr++);
    }

    exit(status == 0 ? 0 : 1);
} /* inst */



// let us hope we never get here
void print_regerror(int errcode, regex_t *compiled_expression) 
{
 char *buffer;
 size_t msg_length;

    // first,  we call regerror once to get the message size
    // second, we allocate a buffer of the right size to hold the message
    // third,  we call reerror to actual get the message

    msg_length = regerror(errcode, compiled_expression, NULL, 0);
    buffer = (char *) malloc(msg_length);

    regerror(errcode, compiled_expression, buffer, msg_length);

    fprintf(stderr, "Error: %s", buffer);
    free(buffer);
}



int file_type(char *fname)
{
struct stat fileStat;

    // notice the lstat here, allows us to pick up symlinks
    if(lstat(fname,&fileStat) < 0)
        return -1;

    if (S_ISLNK(fileStat.st_mode)) return SYMLINK;
    if (S_ISDIR(fileStat.st_mode)) return DIRECTORY;
    if (S_ISREG(fileStat.st_mode)) return REGULAR_FILE;

    // if we get here something is wrong, return fail
    return -1;
}



// reverse bubble sort the file/directory name list
// for recursive directory lists this ensures we rename lower directories
// before changing names of directories at a higher level 
void bubble( int num, char *fnames[], int *slist) 
{
 int i;
 int j;
 int temp;

    for (i=0 ; i < num-1; i++){
        for (j=0 ; j < num-i-1; j++)
            if(strcmp(fnames[slist[j]],fnames[slist[j+1]]) < 0 ){
                temp=slist[j];
                slist[j]   = slist[j+1];
                slist[j+1] = temp;
            }
    }
}


// split the filename into basename and extension
// we use a regular express to do this
void split_base_ext(char *filename, char *base, char *ext)
{
 int rc;
 char extpattern[32];
 regex_t *rg;
 regmatch_t matbuf[1];

    base[0]='\0';
    ext[0]='\0';

    // make room for the regular expression and zero it
    rg = (regex_t *) malloc(sizeof(regex_t));
    memset(rg, 0, sizeof(regex_t));

    // the filename extention pattern
    //  dot/period (.) followed by 1 to 4 letters/digits
    //  update 2018/01/16: 1 to 7 letters.... for torrent files
    strcpy(extpattern,"[.][a-zA-z0-9]{1,7}$");

    rc=regcomp(rg, extpattern, REG_EXTENDED); 
    if (rc !=0) {
        print_regerror(rc, rg);
        exit(1);
    }

    rc = regexec(rg, filename, 1, matbuf ,0);
    if(rc == 0) {
        strncpy(base, filename, matbuf[0].rm_so);
        base[matbuf[0].rm_so]='\0';
        strncpy(ext, filename+matbuf[0].rm_so, matbuf[0].rm_eo);
        ext[matbuf[0].rm_eo]='\0';
    } else {
        strcpy(base, filename);
    }

 regfree(rg);
 free(rg);
}


// replace the pattern with replacement string in target
//    sort of like perl: target =~ s/pattern/replacement/g 
void str_replace(char *target, const char *pattern, const char *replacement)
{
 char buffer[FILENAMEMAX] = { 0 };
 char *ip = &buffer[0];
 char *tp = target;
 size_t repl_len = strlen(replacement);
 regex_t *rg;
 regmatch_t match_buf[1];
 int rc = 0;
 int so;
 int eo;

    // make room for the regular expression and zero it
    rg = (regex_t *) malloc(sizeof(regex_t));
    memset(rg, 0, sizeof(regex_t));

    rc=regcomp(rg, pattern, REG_EXTENDED); 
    if (rc !=0) {
        print_regerror(rc, rg);
        exit(1);
    }

    while (rc == 0) {  // loop changing one match at a time
        rc = regexec(rg, target, 1, match_buf ,0);

        if(rc == 0) {
            so=match_buf[0].rm_so;
            eo=match_buf[0].rm_eo;

            // copy part before pattern
            memcpy(ip, tp, so);
            ip = ip + so;

            // insert replacement part 
            memcpy(ip, replacement, repl_len);
            ip = ip + repl_len;

            // copy rest of target string
            strcpy(ip, tp+eo);

            // copy temp buffer back into the target string
            strcpy(target, buffer);
            ip =&buffer[0];
        }
    }
}   // end of str_replace



// this is the magic to create new file names
void cleanupname(char *fn) 
{
 int  i;

  //   1 convert all non-whitelist char to underscores
    str_replace(fn, "[^a-zA-Z0-9 _.-]", "_");

  //   2 optionally convert uppercase to lowercase
    if(lower) {
        for(i = 0; fn[i]; i++){
          fn[i] = tolower(fn[i]);
        } 
    }

  //   3 convert spaces to underscores
    str_replace(fn, " ",  "_");

  //   4 optionally change dots to underscores, except leading one
    if(dots) {
        if (fn[0] == '.') {         // do not remove hidden file leading dot
            str_replace(fn, "[.]",  "_");
            fn[0]='.';
        } else {
            str_replace(fn, "[.]",  "_");
        }
    }

  //  5 remove multiple underscores
    str_replace(fn, "__", "_");
    str_replace(fn, "_-_", "-");


  //  6 remove leading dashes
    if(fn[0] == '-') {
        fn[0] = '_';
    } else {
        str_replace(fn, "^-", "");
    }
}


int processfile(char *infname, int ftype)
{
 int i;
 struct stat fileStat;
 char *filedup;
 char *pathdup;
 char *fname;
 char *pname;
 char bname[FILENAMEMAX];
 char extension[FILENAMEMAX];
 char fqname_out[4096];
 char fqname_in[4096];

  // if file does not exist do nothing
    if(stat(infname,&fileStat) < 0){
        dbprintf(">File %s not found\n", infname);
        return 1;
    }

    // slice the filename into path, name, extension
    // also the fully qualifed name (why?)
    filedup = strdup(infname);      // basename suggests you pass a copy
    pathdup = strdup(infname);      // dirname suggests you pass a copy
    fname = basename(filedup);
    pname=dirname(pathdup);

    if(ftype == REGULAR_FILE) {
        split_base_ext(fname, bname, extension); 
    }
    if(ftype == DIRECTORY) {
        strcpy(bname, fname);
        strcpy(extension, "");      // directories do not have extensions
    }

    fqname_in[0]='\0';
    strcat(fqname_in, pname);
    strcat(fqname_in, "/");
    strcat(fqname_in, bname);
    strcat(fqname_in, extension);
    dbprintf("\n>>%s", (ftype==DIRECTORY)?"Directory:":"Filename:");
    dbprintf(" %s   path <%s> basename <%s> extension <%s>\n", infname, pname,bname,extension);
    dbprintf(" <inNAME: %s\n",fqname_in);

    cleanupname(bname);
//  this is where we convert extension to lowercase
    if(lower) {
        for(i = 0; extension[i]; i++){
          extension[i] = tolower(extension[i]);
        } 
    }

    fqname_out[0]='\0';
    strcat(fqname_out, pname);
    strcat(fqname_out, "/");
    strcat(fqname_out, bname);
    strcat(fqname_out, extension);
    dbprintf(" >outNAME: %s\n",fqname_out);

    if ((strcmp(fqname_in, fqname_out) != 0)){ 
        if(access(fqname_out, F_OK) == 0){
            if(dryrun || verbose)   // new file name exists skip file rename
              printf("%sFile exist skipping %s --> %s\n", (dryrun)?">Dry Run: ":">", fqname_in, fqname_out);
        } else {
             if(dryrun){  // if dryrun print message and return
                printf(">Dry Run: Rename %s  %s %s\n",(ftype==DIRECTORY)?"Directory":"File", fqname_in, fqname_out);
            } else {  // otherwise rename file/directory
                if(verbose) printf(">Renaming <%s> : <%s>\n",fqname_in, fqname_out);
                if(rename(fqname_in, fqname_out))
                    fprintf(stderr, ">>Error: Failed to rename %s to %s\n", fqname_in, fqname_out);
            }
        }
    } else {
        if (dryrun||verbose) 
              printf("%sFilename exceptable, skipping %s --> %s\n", (dryrun)?">Dry Run: ":">", fqname_in, fqname_out);

    }

    free(filedup);
    free(pathdup);
    return 0;
}



int main (int argc, char **argv)
{
 int optc, option_index;    // used to process options
 int i = 0;                  // misc index
 int ft;                     // filetype indicator
 int numfiles;               // number of files on command line
 int *sortit;                // array used to sort filenames
 char **filelist;                // filelist to be renamed
 char nextfilename[FILENAMEMAX]; // next filename when reading stdin

    program_name=argv[0];

    while ((optc=getopt_long(argc, argv, "dDl", long_options, &option_index)) != -1) {
    switch (optc) {
        case 0:
            // getopt set one of the boolean options, just keep going
            break;
        case 'd':
            dots=FALSE;
            break;
        case 'l':
            lower=FALSE;
            break;
        case 'D':
            dirs=TRUE;
            break;
        case '?':
            // getopt_long already printed an error message
            inst(instructions, 1);
            break;

        default:
            fprintf(stderr,"Invalid option, probably a filename starting with dash.\n");
            fprintf(stderr,"       Try, ffn [options] -- {file list} \n");
            abort ();
        }
    } // while options

    numfiles = argc-optind;
//     argv=&argv[optind];

    if (help || version) inst(instructions,0);

    dbprintf(">>option: dirs    %s\n", dirs?"TRUE":"FALSE");
    dbprintf(">>option: dots    %s\n", dots?"TRUE":"FALSE");
    dbprintf(">>option: lower   %s\n", lower?"TRUE":"FALSE");
    dbprintf(">>option: verbose %s\n", verbose?"TRUE":"FALSE");
    dbprintf(">>option: help    %s\n", help?"TRUE":"FALSE");
    dbprintf(">>option: version %s\n", version?"TRUE":"FALSE");
    dbprintf(">>option: dryrun  %s\n", dryrun?"TRUE":"FALSE");
    if(numfiles == 0) dbprintf("no filelist... read  stdin\n"); 
    if(argv[optind]!=NULL && strcmp(argv[optind],"-") == 0 ) dbprintf("- ... read stdin\n");


  // if no files on command line read from stdin
    if (numfiles == 0 || (argv[optind] != NULL && strcmp(argv[optind], "-")==0) ) {
        filelist = malloc(FILEMAX * (sizeof  (char *)));
        while(fgets(nextfilename, FILENAMEMAX, stdin) != NULL) {
            str_replace(nextfilename, "\n$", "");
            filelist[numfiles]= malloc( strlen(nextfilename)+1 );
            dbprintf( ">>File: <%s>\n", nextfilename);
            strcpy(filelist[numfiles], nextfilename);
            numfiles++;
            if(numfiles == FILEMAX) break;
        }
        if(numfiles==FILEMAX) fprintf(stderr, "Warning: maximum number of stdin file exceeded (> %i) \n", numfiles);
    } else {  // otherwise we use files on command line
        filelist=&argv[optind];
    }

    // we sort an array index, not the actual argv strings
    sortit = (int*) malloc(numfiles * sizeof(int));
    for(i=0; i <numfiles; i++) {
        sortit[i]=i;
    }

  // reverse sort file list to put lower level directories in the list first
  // the order of regular files does not matter
    bubble( numfiles, filelist, sortit );

    dbprintf("\n");
    for (i=0; i<numfiles; i++) 
        dbprintf( ">>filelist: %i %s\n", i, filelist[sortit[i]] );

  // rename regular files first, before directory names change
    for (i=0; i < numfiles; i++) {
        ft=file_type(filelist[sortit[i]]);
        if(ft==-1) dbprintf(">>Cannot access file <%s>\n", filelist[sortit[i]]);
        if (ft == REGULAR_FILE) {
            processfile(filelist[sortit[i]], ft);
            dbprintf( ">>Regular file: %s\n", filelist[sortit[i]]);
        }
        if (ft == SYMLINK)
           fprintf(stderr, "Warning: %s symbolic link, not messing with links\n", filelist[sortit[i]]);
    }

  // optionally, rename directories
    for (i=0; i < numfiles; i++) {
        ft=file_type(filelist[sortit[i]]);
        if(strcmp(filelist[sortit[i]],".") == 0) continue;  //skip . directory
        if(strcmp(filelist[sortit[i]],"..") == 0) continue; //skip .. directory
        if (ft == DIRECTORY) {
            if(dirs) {
            processfile(filelist[sortit[i]], ft);
            } else {
               if(verbose) printf(">Not Renaming Directory %s\n", filelist[sortit[i]]);
            }
        }
    }

  free(sortit);
//  if we read files from stdin we should really clean up the memory
//  we allocated but today ... we will let linux do the cleanup
//     for(i=0; i<numfiles; i++) free(filelist[i]);
//      free(filelist);

    return 0;
}   // main
