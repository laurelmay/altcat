#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Removes the O_APPEND flag from a given fd.
 */
void remove_append(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags ^ O_APPEND);
    }
}

/*
 * Returns true if STDOUT_FILENO has O_APPEND set; false otherwise
 */
bool stdout_append() {
    return fcntl(STDOUT_FILENO, F_GETFL) & O_APPEND;
}

/*
 * Print command usage
 */
void print_usage(char *program) {
    printf("%s FILE [FILE...]\n", program);
}

int main(int argc, char **argv) {

    if (argc < 2) {
        /*
         * No file was given.
         */
        fprintf(stderr, "Invalid arguments.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (stdout_append() && isatty(STDOUT_FILENO)) {
        /*
         * In some cases, stdout can have O_APPEND set but not be pointing
         * to a particular file. I noticed this when running `make` before
         * using altcat. For our purposes, this flag can safely be removed. It
         * should be someone elses job to set it back later (or to have not
         * left it set to begin with).
         *
         * It is not safe to remove O_APPEND when STDOUT_FILENO is not a tty.
         * We should not overwrite files.
         */
        remove_append(STDOUT_FILENO);
    }

     /*
     * Neither splice nor sendfile support file descriptors with
     * O_APPEND. Either removing O_APPEND failed or we're not redirected to a
     * tty.
     */
    if (stdout_append()) {
        fprintf(stderr, "Unable to append to files.\n");
        return EXIT_FAILURE;
    }

    int num_files = argc - 1;
    int fds[num_files];

    /*
     * Open all files and add them to the array of file descriptors.
     */
    for (int i = 0; i < num_files; i++) {
        char *in_filename = argv[i + 1];
        fds[i] = open(argv[i + 1], O_RDONLY);
        if (fds[i] < 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "Unable to open %s. File does not exist.\n",
                        in_filename);
            } else {
                fprintf(stderr, "Unable to open %s.\n", in_filename);
            }
            return EXIT_FAILURE;
        }
    }

    /*
     * Copy the files to stdout. Close the files when done. Exit with a 
     * non-zero status on error.
     */
    for (int i = 0; i < num_files; i++) {
        /*
         * Use fstat to determine the file size so we know how many bytes
         * to copy.
         */
        struct stat st;
        fstat(fds[i], &st);

        /* 
         * Copy between the file descriptors without an intermediate copy into
         * userspace. Try splice in case one is a pipe (which stdout often is);
         * if not, then use sendfile if splice set errno to EINVAL (as it does
         * when neither argument is a pipe).
         */

        ssize_t bytes = splice(fds[i], NULL, STDOUT_FILENO, NULL,
                               st.st_size, 0);

        if (bytes == -1 && errno == EINVAL) {
            bytes = sendfile(STDOUT_FILENO, fds[i], NULL, st.st_size);
        }

        /*
         * If things still failed, exit with errno as the exit status.
         */
        if (bytes < 0) {
            return errno;
        }
        close(fds[i]);
    }

    return EXIT_SUCCESS;
}
