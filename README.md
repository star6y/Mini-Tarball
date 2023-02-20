# Mini-Tarball

This project resembles a simplified version of the tar function on Linux and other Unix based operating systems. tar can take many files and creates a single archive, similar to a zip tool, but
without the compression. 

For this project, we can:
Create an archive with the  "-c"  flag
Append to an existing archive with  "-f"
List the files in the archive with  "-t"
Update files if they're in archive with  "-u"
Extract files with the  "-x"  flag 


Example:
./minitar <operation> -f <archive_name> <file_name_1> <file_name_2> ... <file_name_n>
./minitar -c -f foo.tar hello.txt hola.txt
