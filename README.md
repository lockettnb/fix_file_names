ffn -- Fix File Names
=====================

This linux utility will fix/remove spaces and other strange characters from file
names on Linux.  



Usage: ./ffn [OPTION]... [FILE]...
 fix file names: remove spaces and non-alphanumber characters  
 
 -D   --dirs      : rename directores, default is regular files only  
 -d   --nodots    : do NOT remove dots in name, default is change dots to underscore  
 -l   --nolower   : do NOT convert name to lower case, default is change name to lowercase  
      --dryrun    : dryrun (test), only display name changes  
      --verbose   : enable verbose output  
      --help      : display help and exit  
      --version   : print version information and exit  
      --debug     : enable debug output  
   
 Rename filenames (my rules)  
   1 convert all non-whitelist char to underscores  
       whitelist = digits (0-9), letters (a-zA-Z), underscore, dash, period/dots  
   2 optionally convert uppercase to lowercase  
   3 convert spaces to underscores  
   4 optionally change dots to underscores, except leading one  
   5 remove multiple underscores  
   6 remove leading dashes  
 
 Examples:  
   ffn *  
        : rename all files in the current directory  
 
   ffn --dryrun -D -- *  
        : test run rename all files and directories in the current directory  
   
   find sampledir -print | ffn --dryrun   
        : recursivey rename all files in sampledir directory  

    find . -print0 | xargs -0 ffn --dryrun 


