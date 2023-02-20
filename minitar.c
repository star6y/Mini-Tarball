#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include "minitar.h"

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 512

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *)header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100); // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o", stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid); // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32); // Owner  name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid); // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32); // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o", (unsigned)stat_buf.st_size); // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o", (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
    header->typeflag = REGTYPE; // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o", major(stat_buf.st_dev)); // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o", minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];
    // Note: ftruncate does not work with O_APPEND
    int fd = open(file_name, O_WRONLY);
    if (fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", file_name);
        perror(err_msg);
        return -1;
    }
    //  Seek to end of file - nbytes
    off_t current_pos = lseek(fd, -1 * nbytes, SEEK_END);
    if (current_pos == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to seek in file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    // Remove all contents of file past current position
    if (ftruncate(fd, current_pos) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to close file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}


int create_archive(const char *archive_name, const file_list_t *files) {
    node_t *current = files->head;
    FILE *file;
    FILE *destination = fopen(archive_name, "w");
    if (destination == NULL) {
        perror("Failed to open destination file in create_archive\n");
        return -1;
    }

    char buffer[512]; 
    tar_header hed;

    while (current != NULL) { //this loop goes through every file
        memset(buffer, 0, 512*sizeof(buffer[0]));
        file = fopen(current->name, "r");   // open the current file
        if (file == NULL) {
            perror("Failed to open file from current->name in create_archive\n");
            return -1;
        }

        if (fill_tar_header(&hed, current->name) != 0) {    //make the header for the current file
            perror("Failed to fill the tar header in function create_archive\n");
            return -1;
        }
        if (fwrite(&hed, sizeof(char), 512, destination) != 512) {       //write the header for the current file in archive
            perror("Failed to write the header into archive in function create_archive\n");
            return -1;
        }


        while (fread(buffer, sizeof(char), 512, file) > 0) {     // loop through contents of file and add them to archive                                    
                   
            if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
                perror("Failed to write the buffer into archive in function create_archive\n");
                return -1;
            }
            memset(buffer, 0, 512*sizeof(buffer[0]));   //buffer block is set to all 0's each iteration
        }
        if (ferror(file)) {
            perror("Failure in reading the file in create archive");
            return -1;
        }

        fclose(file);           //close the current file

        current = current->next;//go to next file
    }

    //next lines of code add footer
    memset(buffer, 0, 512*sizeof(buffer[0]));
    if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
        perror("Failed to write the footer at the end of archive in function create_archive\n");
        return -1;
    }
    if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
        perror("Failed to write the footer at the end of archive in function create_archive\n");
        return -1;
    }
    fclose(destination);
    return 0;
}


int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    if (remove_trailing_bytes(archive_name, 1024) != 0) {   // remove the footer of the archive
        perror("Failed to remove trailing bytes\n");
        return -1;
    }
    node_t *current = files->head;
    FILE *file;
    FILE *destination = fopen(archive_name, "r+");     // open archive in "r+" so it doesn't overwrite
    if (destination == NULL) {
        perror("Failed to open destination file in append\n");
        return -1;
    }
    tar_header hed;
    char buffer[512]; 
    //int elements_read = 0;
    memset(buffer, 0, 512*sizeof(buffer[0]));   //make buffer a block of zeros
    fseek(destination, 0, SEEK_END);            //go to the end of the current archive

    while (current != NULL) {   // loop through files until there's no more files to append
        memset(buffer, 0, 512*sizeof(buffer[0]));
        file = fopen(current->name, "r");
        if (file == NULL) {
            perror("Failed to open file from current->name in function append\n");
            return -1;
        }
        
        if (fill_tar_header(&hed, current->name) != 0) {    //fill the header
            return -1;
        }
        if (fwrite(&hed, sizeof(char), 512, destination) != 512) {       //write header to the archive
            perror("Failed to write the header into archive in function append\n");
            return -1;
        }       

        while (fread(buffer, sizeof(char), 512, file) > 0) {     // loop through file contents and write them as blocks to archive
            if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
                perror("Failed to write the buffer into archive in function append\n");
                return -1;
            }
        }
        if (ferror(file)) {
            perror("failure in reading the file in function append");
            return -1;
        }
        fclose(file);           // close the current file

        current = current->next;// go to next file
    }
    //next lines of code add footer
    memset(buffer, 0, 512*sizeof(buffer[0]));
    if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
        perror("Failed to write the footer at the end of archive in function append\n");
        return -1;
    }
    if (fwrite(buffer, sizeof(char), 512, destination) != 512) {
        perror("Failed to write the footer at the end of archive in function append\n");
        return -1;
    }
    fclose(destination);
    return 0;
}


int get_archive_file_list(const char *archive_name, file_list_t *files) {
    if (remove_trailing_bytes(archive_name, 1024) != 0) {   // remove the footer of the archive
        perror("Failed to remove trailing bytes\n");
        return -1;
    } 

    FILE *file = fopen(archive_name, "r+");  // then open the archive
    if (file == NULL) {
        perror("Failed to open archive file in file_list\n");
        return -1;
    }

    char buffer[512]; 
    char file_size_o[12];   //octal
    int file_size_d = 0;    //decimal
    int num_blocks = 0;     //number of blocks in the body of the file, header not included

    memset(file_size_o, 0, 12*sizeof(file_size_o[0]));  //fill octal number placeholder with 0's
    tar_header hed;

    while (fread(&hed, sizeof(char), 512, file) > 0) {  // loop through all files, first by reading each header
        if (file_list_add(files, (hed.name)) != 0) {    // get name from header and add to file list
            perror("Failed to add file to files list\n");
            return -1;
        }
        
        strcpy(file_size_o, hed.size);                  //get current size of file in octal number
        sscanf(file_size_o, "%011o", &file_size_d);     //converts octal  to decimal 
        memset(file_size_o, 0, 12*sizeof(file_size_o[0]));  //need to set file_size_o to 0 again, otherwise code fails

        num_blocks = file_size_d / 512; //gets number of blocks for current file
        if (file_size_d % 512 > 0) {    //if there are between 1 and 511 bytes left, count it as another block
            num_blocks++;               // NOTE: using ceiling made code break previously
        }
        fseek(file, 512*num_blocks, SEEK_CUR);  //skip to the next header in the archive, using calculations described above
    }
    if (ferror(file)) {
        perror("failure in reading the header in file list");
        return -1;
    }
    // next lines of code add footer to archive
    memset(buffer, 0, 512*sizeof(buffer[0]));
    if (fwrite(buffer, sizeof(char), 512, file) != 512) {
        perror("Failed to write the footer at the end of archive in function list archive\n");
        return -1;
    }
    if (fwrite(buffer, sizeof(char), 512, file) != 512) {
        perror("Failed to write the footer at the end of archive in function list archive\n");
        return -1;
    }
    fclose(file);

    return 0;
}


int extract_files_from_archive(const char *archive_name) {
    if (remove_trailing_bytes(archive_name, 1024) != 0) {   // remove the footer of the archive
        perror("Failed to remove trailing bytes\n");
        return -1;
    } 
    
    FILE *file;
    FILE *archive_file = fopen(archive_name, "r+");     // open archive in "r+" mode, since we removed footer before opening it, we need to add footer again
    if (archive_file == NULL) {
        perror("Failed to open archive file in file extract function\n");
        return -1;
    }

    int blocks_to_read = 0;
    int file_size_d = 0;    //decimal
    char file_size_o[12];   //octal
    char buffer[512];       // buffer will hold a block of 512 bytes
    char mini_buf[1];       // mini_buf will hold only one byte. This will be used to store the remaining bytes
    int n = 0;              // n will keep track of number of bytes to get to the next block of 512
    int remainder_bytes = 0;// keep count of the trailing bytes of a file, will be smaller than 512
    memset(buffer, 0, 512);

    tar_header hed;

    while (fread(&hed, sizeof(char), 512, archive_file) > 0) {
        file = fopen((hed.name), "w");  //make a new file with the name we get from the header block in the archive
        if (file == NULL) {
            perror("Failed to open file from hed.name in file extract function\n");
            return -1;
        }

        strcpy(file_size_o, (hed.size));
        sscanf(file_size_o, "%011o", &file_size_d);     //convert octal to decimal
        memset(file_size_o, 0, 12);  //fill file_size_o with 0's, otherwise code fails

        blocks_to_read = file_size_d / 512; //gets number of blocks for current file
        if (file_size_d % 512 > 0) {        //if there are between 1 and 511 bytes left, count it as another block
            blocks_to_read++;
        }
        remainder_bytes = file_size_d % 512;   
        n = 512 - remainder_bytes;        

        while (blocks_to_read != 0) {   //loop through contents of the file in the archive, block by block
            if (blocks_to_read == 1) {  // if this will be the last block in the file
                while (remainder_bytes != 0) {  //read and write one byte at a time until finished writing all the remainder bytes
                    if (fread(mini_buf, sizeof(char), 1, archive_file) > 0) {
                        if (fwrite(mini_buf, sizeof(char), 1, file) != 1) {
                            perror("Failed to write one of the remainder bytes in function extract\n");
                            return -1;
                        }
                    }
                    if (ferror(archive_file)) {
                        perror("failure in reading the last block of the file from archive in function extract (Part 2)\n");
                        return -1;
                    }
                    remainder_bytes--;
                    memset(mini_buf, 0, 1);
                }
                fseek(archive_file, n, SEEK_CUR);   // after finishing writing all the bytes, skip to the next header 
                fclose(file);                                           // (skips 0's between current file and next header)
                
                break;  // logic of the while loop on line 344 is that if (blocks_to_read != 1), then read and write a block of 512 bytes
                        // so this break statement breaks the loop so a 512 block does not get read and written in an unopened file 
                        // before the blocks_to_read counter becomes 0
            }
            
            if (fread(buffer, sizeof(char), 512, archive_file) > 0) { // read from archive
                if (fwrite(buffer, sizeof(char), 512, file) != 512) { // write to file
                    perror("Failed to write the buffer into file in function extract file\n");
                    return -1;
                } 
            }
            if (ferror(archive_file)) {
                perror("failure in reading the file in function extract (Part 3)\n");
                return -1;
            }
                  
            memset(buffer, 0, 512);       // reset the buffer with 0's
            blocks_to_read--;
        }
        
        n = 0;                  // set all variables to 0 just to be safe 
        remainder_bytes = 0;
        file_size_d = 0;
        blocks_to_read = 0;
    }
    if (ferror(file)) {
        perror("failure in reading the header in function extract (Part 1)\n");
        return -1;
    }

    //the following code just adds the footer back to the archive
    memset(buffer, 0, 512);
    if (fwrite(buffer, sizeof(char), 512, archive_file) != 512) {
        perror("Failed to write the footer at the end of archive in function extract\n");
        return -1;
    }
    if (fwrite(buffer, sizeof(char), 512, archive_file) != 512) {
        perror("Failed to write the footer at the end of archive in function extract\n");
        return -1;
    }
    fclose(archive_file);
    return 0;
}