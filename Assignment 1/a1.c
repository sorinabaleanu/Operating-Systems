#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

off_t convert_permission(const char *perm_string)
{
	int base = 1, aux = 0;
	off_t number = 0, answ = 0;
	char binary_string[9] = "";

	for (int i = 0; i < 9; i++)
	{
		if (perm_string[i] == '-')
			binary_string[i] = '0';
		else
			binary_string[i] = '1';
	}

	sscanf(binary_string, "%ld", &number);

	while (number)
	{
		aux = number % 10;
		number = number / 10;
		answ += aux * base;
		base *= 2;
	}

	return answ;
}

void list(const char *path, int recursive, off_t size, const char *perm_string)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	char fullPath[512];
	struct stat statbuf;
	int permissions;

	if (perm_string != NULL)
	{

		permissions = convert_permission(perm_string);
	}

	dir = opendir(path);

	if (dir == NULL)
	{

		return;
	}

	while ((entry = readdir(dir)) != NULL)
	{

		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
		{
			snprintf(fullPath, 512, "%s/%s", path, entry->d_name);

			if (lstat(fullPath, &statbuf) == 0)

			{
				if ((statbuf.st_size > size && S_ISREG(statbuf.st_mode)) || size == 0)
				{
					if (perm_string == NULL || (statbuf.st_mode & 0777) == permissions)
					{
						printf("%s\n", fullPath);
					}
				}

				if (recursive && S_ISDIR(statbuf.st_mode))
				{
					list(fullPath, recursive, size, perm_string);
				}
			}
		}
	}
	closedir(dir);
}

void parse(int fd)
{
	int sect_type = 0, sect_offset = 0, sect_size = 0, header_size = 0, no_of_sections = 0, version = 0;
	char magic[2] = "", sect_name[20] = "";

	lseek(fd, -4, SEEK_END);
	read(fd, &header_size, 2);

	read(fd, &magic, 2);

	if (strcmp(magic, "ll") != 0)
	{
		printf("ERROR\nwrong magic\n");
		return;
	}

	lseek(fd, -header_size, SEEK_CUR);
	read(fd, &version, 4);

	if (version < 124 || version > 175)
	{

		printf("ERROR\nwrong version\n");
		return;
	}

	read(fd, &no_of_sections, 1);

	if (no_of_sections < 3 || no_of_sections > 13)
	{
		printf("ERROR\nwrong sect_nr\n");
		return;
	}

	for (int i = 1; i <= no_of_sections; i++)
	{
		read(fd, &sect_name, 20);
		read(fd, &sect_type, 1);
		if (sect_type != 45 && sect_type != 53)
		{
			printf("ERROR\nwrong sect_types\n");
			return;
		}
		read(fd, &sect_offset, 4);
		read(fd, &sect_size, 4);
	}

	lseek(fd, -header_size + 5, SEEK_END);

	printf("SUCCESS\nversion=%d\nnr_sections=%d\n", version, no_of_sections);

	for (int i = 1; i <= no_of_sections; i++)
	{
		read(fd, &sect_name, 20);
		read(fd, &sect_type, 1);
		read(fd, &sect_offset, 4);
		read(fd, &sect_size, 4);

		sect_name[20] = '\0';

		printf("section%d: %s %d %d\n", i, sect_name, sect_type, sect_size);
	}
}

int verify_sf(int fd, int size)
{
	int sect_type = 0, sect_offset = 0, sect_size = 0, header_size = 0, nr_of_sections = 0, version = 0;
	char magic[2] = "", sect_name[20] = "";

	lseek(fd, -4, SEEK_END);
	read(fd, &header_size, 2);

	read(fd, &magic, 2);
	if (strcmp(magic, "ll") != 0)
	{
		return 0;
	}

	lseek(fd, -header_size, SEEK_CUR);
	read(fd, &version, 4);

	if (version < 124 || version > 175)
	{
		return 0;
	}

	read(fd, &nr_of_sections, 1);

	if (nr_of_sections < 3 || nr_of_sections > 13)
	{

		return 0;
	}

	for (int i = 1; i <= nr_of_sections; i++)
	{
		read(fd, &sect_name, 20);
		read(fd, &sect_type, 1);

		if (sect_type != 45 && sect_type != 53)
		{
			return 0;
		}

		read(fd, &sect_offset, 4);
		read(fd, &sect_size, 4);

		if (sect_size > 1225 && size)
		{
			return 0;
		}
	}

	return 1;
}

void extract(int fd, int section_number, int line_number)
{
	int header_size = 0, sect_offset = 0, sect_size = 0, jump = 0, no_of_sections = 0, no_of_lines = 1, current_line = 0;
	char buf[1];

	if (!verify_sf(fd, 0))
	{
		printf("ERROR\ninvalid file\n");
		return;
	}

	jump = (section_number - 1) * 29;

	lseek(fd, -4, SEEK_END);
	read(fd, &header_size, 2);
	lseek(fd, -header_size + 4, SEEK_END);
	read(fd, &no_of_sections, 1);

	if (no_of_sections < section_number)
	{
		printf("ERROR\ninvalid section\n");
		return;
	}

	lseek(fd, jump + 21, SEEK_CUR);

	read(fd, &sect_offset, 4);
	read(fd, &sect_size, 4);

	lseek(fd, sect_offset, SEEK_SET);

	for (int size = 0; size < sect_size; size++)
	{
		if (read(fd, &buf, 1))
		{
			if (strcmp(buf, "\n") == 0)
			{
				no_of_lines++;
			}
		}
	}

	if (no_of_lines < line_number)
	{
		printf("ERROR\ninvalid line\n");
		return;
	}

	lseek(fd, sect_offset, SEEK_SET);

	current_line = no_of_lines;

	printf("SUCCESS\n");

	for (int size = 0; size < sect_size; size++)
	{
		if (read(fd, &buf, 1))

		{
			if (current_line == line_number)
			{
				printf("%s", buf);
			}
			if (strcmp(buf, "\n") == 0)
			{
				current_line--;
			}
		}
	}
}

void findall(const char *path)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	struct stat statbuf;
	char fullPath[512] = "";

	dir = opendir(path);

	if (dir == NULL)
	{

		return;
	}

	while ((entry = readdir(dir)) != NULL)
	{

		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
		{
			snprintf(fullPath, 512, "%s/%s", path, entry->d_name);

			if (lstat(fullPath, &statbuf) == 0)
			{

				if (S_ISREG(statbuf.st_mode))
				{
					int fd = open(fullPath, O_RDONLY);

					if (verify_sf(fd, 1))
					{
						printf("%s\n", fullPath);
					}
					close(fd);
				}
				if (S_ISDIR(statbuf.st_mode))
				{
					findall(fullPath);
				}
			}
		}
	}

	closedir(dir);
}

int main(int argc, char **argv)
{

	int recursiv = 0, fd = -1, section = 0, line_number = 0;
	char *path = NULL, *perm_string = NULL;
	struct stat statbuf;
	off_t size = 0;
	if (argc >= 2)
	{

		if (strcmp(argv[1], "variant") == 0)
		{
			printf("70182\n");
		}
		else if (strcmp(argv[1], "list") == 0)
		{

			for (int i = 2; i < argc; i++)
			{
				if (strncmp(argv[i], "path=", 5) == 0)
				{
					path = argv[i] + 5;
				}
				else if (strcmp(argv[i], "recursive") == 0)
				{
					recursiv = 1;
				}
				else if (strncmp(argv[i], "size_greater=", 13) == 0)
				{
					sscanf(argv[i] + 13, "%ld", &size);
				}
				else if (strncmp(argv[i], "permissions=", 12) == 0)
				{
					perm_string = argv[i] + 12;
				}
			}

			if (lstat(path, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
			{
				printf("ERROR\ninvalid directory path\n");
			}
			else
			{
				printf("SUCCESS\n");
				list(path, recursiv, size, perm_string);
			}
		}

		else if (strncmp(argv[1], "parse", 5) == 0)
		{
			if (strncmp(argv[2], "path=", 5) == 0)
			{
				path = argv[2] + 5;
			}

			if ((fd = open(path, O_RDONLY)) >= 0)
			{
				parse(fd);
				close(fd);
			}
			else
			{
				printf("ERROR\ninvalid file path\n");
			}
		}

		else if (strncmp(argv[1], "extract", 7) == 0)

		{
			if (strncmp(argv[2], "path=", 5) == 0)
			{
				path = argv[2] + 5;
			}
			if (strncmp(argv[3], "section=", 8) == 0)
			{
				sscanf(argv[3] + 8, "%d", &section);
			}

			if (strncmp(argv[4], "line=", 5) == 0)
			{
				sscanf(argv[4] + 5, "%d", &line_number);
			}

			if ((fd = open(path, O_RDONLY)) >= 0)
			{
				extract(fd, section, line_number);
				close(fd);
			}
		}

		else if (strncmp(argv[1], "findall", 7) == 0)
		{

			if (strncmp(argv[2], "path=", 5) == 0)
			{
				path = argv[2] + 5;
			}

			if (lstat(path, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
			{
				printf("ERROR\ninvalid directory path\n");
			}
			else
			{
				printf("SUCCESS\n");
				findall(path);
			}
		}
	}
	return 0;
}
