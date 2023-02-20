#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"
//   argv[0]  argv[1]     argv[2]    argv[3]        argv[4]       argv[5]           argv[n]
//> ./minitar <operation> -f         <archive_name> <file_name_1> <file_name_2> ... <file_nam
int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    file_list_t files_in_argv;
    file_list_init(&files_in_argv);
    node_t *current;
    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations

    char arch_name[1000];
    strcpy(arch_name,argv[3]);
    if (strcmp(argv[1], "-t") != 0) { //if operation is not "-t" (list), then
        for (int i = 4 ; i < argc; i++) { //loop through the arguments in command line and add them to files list
            if (file_list_add(&files_in_argv, argv[i]) != 0) {
                perror("Failed to add file to files list\n");
                return -1;
            }
        }
    }

    if (strcmp(argv[1], "-c") == 0) {  //create
        if (create_archive(arch_name, &files_in_argv) != 0) {
            return -1;
        }
    }

    if (strcmp(argv[1], "-a") == 0) {  //append
        if (append_files_to_archive(arch_name, &files_in_argv) != 0) {
            return -1;
        }
    }

    if (strcmp(argv[1], "-t") == 0) {  //list
        if (get_archive_file_list(arch_name, &files_in_argv) != 0) {
            return -1;
        }
    }


    if (strcmp(argv[1], "-u") == 0) {  //update
        if (get_archive_file_list(arch_name, &files) != 0) {    //get the files in the archive
            perror("problem getting file names in archive_file_list call\n");
            return -1;
        }
        current = files_in_argv.head;   // make a pointer to the files read from the command line (argv)
        while (current != NULL) {       // while there are files to read
            if (file_list_contains(&files, current->name) != 1) {   // check if the file from command line is in archve
                printf("Error: One or more of the specified files is not already present in archive");
                file_list_clear(&files_in_argv);
                file_list_clear(&files);
                return -1;
            }
            current = current->next;    // go to next file if command line file is in archive
        }

        if (append_files_to_archive(arch_name, &files_in_argv) != 0) {  // append the files from the command line to the archive
            perror("problem appening files in update\n");
            return -1; //if it is, then append the current file to the end of the archive
        }
        
    }

    if (strcmp(argv[1], "-x") == 0) {  //extract
        if (extract_files_from_archive(arch_name) != 0) {
            return -1;
        }
    }

    //printing the names of the files in the archive
    if (strcmp(argv[1], "-t") == 0) {   //if operation is list
        current = files_in_argv.head;   //then get pointer to the files from archive
        while (current != NULL) {       //list the names of files 
            printf("\n%s", (current->name));
            current = current->next;
        }
        printf("\n");
    }
    file_list_clear(&files_in_argv);
    file_list_clear(&files);
    return 0;
}
