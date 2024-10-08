#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#include <stdio.h>

#include "viktar.h"

int create_archive(const char *archive_name, char **files, int num_files);
int extract_files(const char *archive_name, char **files, int num_files);
void list_files(const char *archive_name, int show_headers);
ssize_t full_write(int fd, const void *buf, size_t count);
ssize_t full_read(int fd, void *buf, size_t count);

int main(int argc, char *argv[]) {
    int opt;
    int create = FALSE, extract = FALSE, list = FALSE, list_headers = FALSE;
    char *archive_name = NULL;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
            case 'c':
                create = TRUE;
                break;
            case 'x':
                extract = TRUE;
                break;
            case 't':
                list = TRUE;
                break;
            case 'T':
                list_headers = TRUE;
                break;
            case 'f':
                archive_name = optarg;
                break;
	    case 'v':
		fprintf(stderr, "verbose enabled\n");
		break;
	    case 'h':
		printf("help text\n");
		printf("	./viktar\n");
		printf("	Options: xctTf:hv\n");
		printf("		-x		extract file/files from archive\n");
		printf("		-c		create an archive file\n");
		printf("		-t		display a short table of contents of the archive file\n");
		printf("		-T		display a long table of contents of the archive file\n");
		printf("		Only one of xctT can be specified\n");
		printf("		-f filename	use filename as the archive file\n");
		printf("		-v		give verbose diagnostic messages\n");
		printf("		-h		display this AMAZING help message\n");
		exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "Invalid option: %c\n", opt);
                exit(EXIT_FAILURE);
        }
    }

    if (create) {
        create_archive(archive_name, &argv[optind], argc - optind);
    } else if (extract) {
        extract_files(archive_name, &argv[optind], argc - optind);
    } else if (list) {
        list_files(archive_name, FALSE);
    } else if (list_headers) {
        list_files(archive_name, TRUE);
    } else {
        fprintf(stderr, "Please specify an operation: -c, -x, -t, or -T\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int create_archive(const char *archive_name, char **files, int num_files) {
    int arch_fd, file_fd;
    viktar_header_t header;
    char buffer[90000];
    ssize_t bytes_read;

    if (archive_name) {
        arch_fd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (arch_fd == -1) {
            perror("Error opening archive for writing");
            exit(EXIT_FAILURE);
        }
    } else {
        arch_fd = STDOUT_FILENO;
    }

    // Write VIKTAR_FILE header
    full_write(arch_fd, VIKTAR_FILE, strlen(VIKTAR_FILE));

    for (int i = 0; i < num_files; i++) {
        struct stat st;
        if (stat(files[i], &st) == -1) {
            perror("Error stat-ing file");
            continue;
        }

        // Fill the header
        strncpy(header.viktar_name, files[i], VIKTAR_MAX_FILE_NAME_LEN);
        header.st_mode = st.st_mode;
        header.st_uid = st.st_uid;
        header.st_gid = st.st_gid;
        header.st_size = st.st_size;
        header.st_atim = st.st_atim;
        header.st_mtim = st.st_mtim;
        header.st_ctim = st.st_ctim;

        full_write(arch_fd, &header, sizeof(header));

        file_fd = open(files[i], O_RDONLY);
        if (file_fd == -1) {
            perror("Error opening file for reading");
            continue;
        }

        while ((bytes_read = full_read(file_fd, buffer, sizeof(buffer))) > 0) {
            full_write(arch_fd, buffer, bytes_read);
        }

        close(file_fd);
    }

    if (archive_name) {
        close(arch_fd);
    }

    return 0;
}

int extract_files(const char *archive_name, char **files, int num_files) {
    int arch_fd, file_fd;
    viktar_header_t header;
    char buffer[90000];
    ssize_t bytes_read;

    if (archive_name) {
        arch_fd = open(archive_name, O_RDONLY);
        if (arch_fd == -1) {
            perror("Error opening archive for reading");
            exit(EXIT_FAILURE);
        }
    } else {
        arch_fd = STDIN_FILENO;
    }

    // Check VIKTAR_FILE header
    char check_header[strlen(VIKTAR_FILE)];
    full_read(arch_fd, check_header, strlen(VIKTAR_FILE));
    if (strncmp(check_header, VIKTAR_FILE, strlen(VIKTAR_FILE)) != 0) {
        fprintf(stderr, "Not a valid VIKTAR archive.\n");
        exit(EXIT_FAILURE);
    }

    while (full_read(arch_fd, &header, sizeof(header)) == sizeof(header)) {
        int extract = (num_files == 0);  // If no files specified, extract all
        for (int i = 0; i < num_files; i++) {
            if (strcmp(files[i], header.viktar_name) == 0) {
                extract = TRUE;
                break;
            }
        }

        if (extract) {
            file_fd = open(header.viktar_name, O_WRONLY | O_CREAT | O_TRUNC, header.st_mode);
            if (file_fd == -1) {
                perror("Error opening file for writing");
                lseek(arch_fd, header.st_size, SEEK_CUR);  // Skip the file content
                continue;
            }

            size_t to_read = header.st_size;
            while (to_read > 0) {
                ssize_t read_size = sizeof(buffer) < to_read ? sizeof(buffer) : to_read;
                bytes_read = full_read(arch_fd, buffer, read_size);
                full_write(file_fd, buffer, bytes_read);
                to_read -= bytes_read;
            }

            struct utimbuf times;
            times.actime = header.st_atim.tv_sec;
            times.modtime = header.st_mtim.tv_sec;
            utime(header.viktar_name, &times);

            close(file_fd);
        } else {
            lseek(arch_fd, header.st_size, SEEK_CUR);  // Skip the file content
        }
    }

    if (archive_name) {
        close(arch_fd);
    }

    return 0;
}

void list_files(const char *archive_name, int show_headers) {
    int arch_fd;
    viktar_header_t header;

    if (archive_name) {
        arch_fd = open(archive_name, O_RDONLY);
        if (arch_fd == -1) {
            perror("Error opening archive for reading");
            exit(EXIT_FAILURE);
        }
    } else {
        arch_fd = STDIN_FILENO;
    }

    // Check VIKTAR_FILE header
    char check_header[strlen(VIKTAR_FILE)];
    full_read(arch_fd, check_header, strlen(VIKTAR_FILE));
    if (strncmp(check_header, VIKTAR_FILE, strlen(VIKTAR_FILE)) != 0) {
        fprintf(stderr, "Not a valid VIKTAR archive.\n");
        exit(EXIT_FAILURE);
    }

    printf("Contents of viktar file: \"%s\"\n", archive_name);
    while (full_read(arch_fd, &header, sizeof(header)) == sizeof(header)) {
        if (show_headers) {
            printf("	file name: %s\n", header.viktar_name);
            printf("		mode: %o\n", header.st_mode);
            printf("		user: %d\n", header.st_uid);
            printf("		group: %d\n", header.st_gid);
            printf("		size: %ld\n", header.st_size);
            printf("		mtime: %ld\n", header.st_mtim.tv_sec);
            printf("		atime: %ld\n", header.st_atim.tv_sec);
            //printf("Status Change Time: %ld\n", header.st_ctim.tv_sec);
            //printf("\n");
        } else {
            printf("	file name: %s\n", header.viktar_name);
        }

        lseek(arch_fd, header.st_size, SEEK_CUR);  // Skip the file content
    }

    if (archive_name) {
        close(arch_fd);
    }
}

ssize_t full_write(int fd, const void *buf, size_t count) {
    size_t total_written = 0;
    while (total_written < count) {
        ssize_t written = write(fd, (char *)buf + total_written, count - total_written);
        if (written == -1) {
            perror("Error writing to file");
            exit(EXIT_FAILURE);
        }
        total_written += written;
    }
    return total_written;
}

ssize_t full_read(int fd, void *buf, size_t count) {
    size_t total_read = 0;
    while (total_read < count) {
        ssize_t bytes = read(fd, (char *)buf + total_read, count - total_read);
        if (bytes == -1) {
            perror("Error reading from file");
            exit(EXIT_FAILURE);
        }
        if (bytes == 0) {
            break;  // EOF
        }
        total_read += bytes;
    }
    return total_read;
}

