#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <dirent.h>

#define RESPONSE_PIPE "RESP_PIPE_70182"
#define REQUEST_PIPE "REQ_PIPE_70182"

void write_response(char *message, int fd_response)
{
    char lenght = strlen(message);
    write(fd_response, &lenght, sizeof(char));
    write(fd_response, message, lenght);
}

void ping_pong(int fd_response)
{
    unsigned int number = 70182;
    write_response("PING", fd_response);
    write_response("PONG", fd_response);
    write(fd_response, &number, sizeof(unsigned int));
}

void *create_shm(int fd_request, int fd_response)
{
    int fd_shm;
    unsigned int memory_size;

    write_response("CREATE_SHM", fd_response);

    if (read(fd_request, &memory_size, sizeof(unsigned int)) == 0)
    {
        write_response("ERROR", fd_response);
        return NULL;
    }

    fd_shm = shm_open("/ycyNm5", O_CREAT | O_RDWR, 0644);
    if (fd_shm < 0)
    {
        write_response("ERROR", fd_response);
        return NULL;
    }

    ftruncate(fd_shm, memory_size);

    void *shared_memory = mmap(0, memory_size, PROT_WRITE,
                               MAP_SHARED, fd_shm, 0);
    ;
    if (shared_memory == (void *)-1)
    {
        write_response("ERROR", fd_response);
        return NULL;
    }
    write_response("SUCCESS", fd_response);

    return shared_memory;
}

void write_to_shm(int fd_request, int fd_response, void *shared_memory)
{
    unsigned int offset = 0;
    unsigned int value = 0;

    read(fd_request, &offset, sizeof(unsigned int));
    read(fd_request, &value, sizeof(unsigned int));

    write_response("WRITE_TO_SHM", fd_response);

    if (offset + sizeof(unsigned int) > 4233393)
    {
        write_response("ERROR", fd_response);
        return;
    }

    memcpy(shared_memory + offset, &value, sizeof(unsigned int));

    write_response("SUCCESS", fd_response);
}

void *map_memory(int fd_request, int fd_response, int *size)
{
    int fd = -1;
    char lenght;
    char file[512];
    struct stat st;

    write_response("MAP_FILE", fd_response);

    read(fd_request, &lenght, sizeof(char));

    char *file_name = (char *)malloc(lenght * sizeof(char));
    read(fd_request, file_name, lenght);
    *(file_name + lenght) = '\0';

    sprintf(file, "%s", file_name);

    fd = open(file, O_RDONLY);

    if (fd == -1)
    {
        write_response("ERROR", fd_response);
        return NULL;
    }

    fstat(fd, &st);

    *size = st.st_size;

    void *mapped_file = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (mapped_file == (void *)-1)
    {
        write_response("ERROR", fd_response);
        return NULL;
    }

    write_response("SUCCESS", fd_response);

    return mapped_file;
}

void read_from_file_offset(int fd_request, int fd_response, void *mapped_file, void *shared_memory, int file_size)
{
    unsigned int offset;
    unsigned int no_of_bytes;

    read(fd_request, &offset, sizeof(unsigned int));
    read(fd_request, &no_of_bytes, sizeof(unsigned int));

    write_response("READ_FROM_FILE_OFFSET", fd_response);

    if (offset + no_of_bytes > file_size)
    {
        write_response("ERROR", fd_response);
        return;
    }

    memcpy(shared_memory, mapped_file + offset, no_of_bytes);

    write_response("SUCCESS", fd_response);
}

void read_from_file_section(int fd_request, int fd_response, void *mapped_file, void *shared_memory, int file_size)
{

    unsigned int no_of_sections = 0;
    unsigned int header_size = 0;
    unsigned int section_offset = 0;
    unsigned int offset = 0;
    unsigned int no_of_bytes = 0;
    unsigned int section = 0;

    read(fd_request, &section, sizeof(unsigned int));
    read(fd_request, &offset, sizeof(unsigned int));
    read(fd_request, &no_of_bytes, sizeof(unsigned int));

    memcpy(&header_size, mapped_file + file_size - 4, 2);
    memcpy(&no_of_sections, mapped_file + file_size - header_size + 4, 1);

    write_response("READ_FROM_FILE_SECTION", fd_response);

    if (no_of_sections < section)
    {
        write_response("ERROR", fd_response);
        return;
    }

    memcpy(&section_offset, mapped_file + file_size - header_size + 4 + (section - 1) * 29 + 22, 4);

    memcpy(shared_memory, mapped_file + offset + section_offset, no_of_bytes);

    write_response("SUCCESS", fd_response);
}

void read_from_logical_space_offset(int fd_request, int fd_response, void *mapped_file, void *shared_memory, int file_size)
{

    unsigned int no_of_sections = 0;
    unsigned int header_size = 0;
    unsigned int section_offset = 0;
    unsigned int section_size = 0;
    unsigned int logical_section = 0;
    unsigned int logical_offset = 0;
    unsigned int no_of_bytes = 0;
    int section = 0;
    unsigned int start_of_section = 0;
    unsigned int end_of_section = 0;

    read(fd_request, &logical_offset, sizeof(unsigned int));
    read(fd_request, &no_of_bytes, sizeof(unsigned int));

    memcpy(&header_size, mapped_file + file_size - 4, 2);
    memcpy(&no_of_sections, mapped_file + file_size - header_size + 4, 1);

    logical_section = logical_offset / 5120 + 1;

    while (end_of_section < logical_section)
    {
        start_of_section = end_of_section++;

        memcpy(&section_size, mapped_file + file_size - header_size + 4 + section * 29 + 26, 4);

        end_of_section = start_of_section + (section_size / 5120) + 1;

        section++;
        if (section > no_of_sections)
        {
            write_response("READ_FROM_LOGICAL_SPACE_OFFSET", fd_response);
            write_response("ERROR", fd_response);

            return;
        }
    }

    if (section_size < no_of_bytes)
    {
        write_response("READ_FROM_LOGICAL_SPACE_OFFSET", fd_response);
        write_response("ERROR", fd_response);

        return;
    }

    logical_offset = logical_offset - start_of_section * 5120;

    memcpy(&section_offset, mapped_file + file_size - header_size + 4 + (section - 1) * 29 + 22, 4);

    memcpy(shared_memory, mapped_file + section_offset + logical_offset, no_of_bytes);

    write_response("READ_FROM_LOGICAL_SPACE_OFFSET", fd_response);
    write_response("SUCCESS", fd_response);
}

int main()
{
    int response = mkfifo(RESPONSE_PIPE, 0664);
    int request = open(REQUEST_PIPE, O_RDONLY);
    int ok = 1;
    int size = 0;
    void *shared_memory;
    void *mapped_file;
    char lenght;
    char command[512];

    response = open("RESP_PIPE_70182", O_WRONLY);

    if (response != -1)
    {
        write_response("CONNECT", response);
        printf("SUCCESS\n");
    }

    while (ok)
    {
        if (request > 0)
        {
            read(request, &lenght, sizeof(lenght));
            read(request, command, lenght);
        }
        if (strncmp(command, "PING", 4) == 0)
        {
            ping_pong(response);
        }
        else if (strncmp(command, "CREATE_SHM", 10) == 0)
        {
            shared_memory = create_shm(request, response);
        }
        else if (strncmp(command, "WRITE_TO_SHM", 12) == 0)
        {

            write_to_shm(request, response, shared_memory);
        }
        else if (strncmp(command, "MAP_FILE", 8) == 0)
        {

            mapped_file = map_memory(request, response, &size);
        }

        else if (strncmp(command, "READ_FROM_FILE_OFFSET", 21) == 0)
        {
            read_from_file_offset(request, response, mapped_file, shared_memory, size);
        }
        else if (strncmp(command, "READ_FROM_FILE_SECTION", 22) == 0)
        {
            read_from_file_section(request, response, mapped_file, shared_memory, size);
        }

        else if (strncmp(command, "READ_FROM_LOGICAL_SPACE_OFFSET", 30) == 0)
        {
            read_from_logical_space_offset(request, response, mapped_file, shared_memory, size);
        }
        else if (strncmp(command, "EXIT", 4) == 0)
        {
            close(request);
            close(response);
            unlink(REQUEST_PIPE);
            unlink(RESPONSE_PIPE);
            shm_unlink("/ycyNm5");
            ok = 0;
        }
    }

    return 0;
}