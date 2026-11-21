#include "cli/test_output.h"
#include "common/errors.h"
#include <stdlib.h>

#ifdef _WIN32
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#define DUP _dup
#define DUP2 _dup2
#define CLOSE _close
#define FILENO _fileno
#else
#include <unistd.h>
#define DUP dup
#define DUP2 dup2
#define CLOSE close
#define FILENO fileno
#endif

static bool capture_test_errors(int output_fd, int* saved_stderr)
{
    if (fflush(stderr) != 0) {
        report_error("Could not flush test errors");

        return false;
    }

    int stderr_fd = FILENO(stderr);
    *saved_stderr = DUP(stderr_fd);

    if (*saved_stderr < 0) {
        report_error("Could not save standard error");

        return false;
    }

    if (DUP2(output_fd, stderr_fd) < 0) {
        CLOSE(*saved_stderr);
        report_error("Could not redirect test errors");

        return false;
    }

    return true;
}

static void restore_test_stream(int saved_descriptor, FILE* stream)
{
    int descriptor = FILENO(stream);
    int restored = DUP2(saved_descriptor, descriptor);

    CLOSE(saved_descriptor);
    clearerr(stream);

    if (restored < 0) {
        fatal_error("Could not restore test output stream");
    }
}

static FILE* create_test_output_file(void)
{
#ifdef _WIN32
    char* path = _tempnam(NULL, "mbx");

    if (!path) {
        return NULL;
    }

    int flags = _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY;
    int descriptor = _open(path, flags, _S_IREAD | _S_IWRITE);

    free(path);

    if (descriptor < 0) {
        return NULL;
    }

    FILE* output = _fdopen(descriptor, "w+b");

    if (!output) {
        _close(descriptor);
    }

    return output;
#else
    return tmpfile();
#endif
}

FILE* begin_test_output_capture(int* saved_stdout, int* saved_stderr)
{
    if (fflush(stdout) != 0) {
        report_error("Could not flush test output");

        return NULL;
    }

    FILE* output = create_test_output_file();

    if (!output) {
        report_error("Could not capture test output");

        return NULL;
    }

    int stdout_fd = FILENO(stdout);
    int output_fd = FILENO(output);

    *saved_stdout = DUP(stdout_fd);

    if (*saved_stdout < 0) {
        fclose(output);
        report_error("Could not save standard output");

        return NULL;
    }

    if (DUP2(output_fd, stdout_fd) < 0) {
        CLOSE(*saved_stdout);
        fclose(output);
        report_error("Could not redirect test output");

        return NULL;
    }

    if (saved_stderr && !capture_test_errors(output_fd, saved_stderr)) {
        restore_test_stream(*saved_stdout, stdout);
        fclose(output);

        return NULL;
    }

    return output;
}

bool end_test_output_capture(int saved_stdout, const int* saved_stderr)
{
    bool errors_flushed = true;

    if (saved_stderr) {
        errors_flushed = fflush(stderr) == 0;
    }

    bool flushed = fflush(stdout) == 0 && errors_flushed;

    restore_test_stream(saved_stdout, stdout);

    if (saved_stderr) {
        restore_test_stream(*saved_stderr, stderr);
    }

    if (!flushed) {
        report_error("Could not flush captured test output");
    }

    return flushed;
}

void print_test_output(FILE* output, FILE* stream, const char* heading)
{
    if (fseek(output, 0, SEEK_SET) != 0) {
        report_error("Could not rewind captured test output");

        return;
    }

    char buffer[4096];
    size_t count = fread(buffer, 1, sizeof(buffer), output);

    if (count > 0 && heading) {
        fputs(heading, stream);
    }

    while (count > 0) {
        fwrite(buffer, 1, count, stream);

        count = fread(buffer, 1, sizeof(buffer), output);
    }

    if (ferror(output)) {
        report_error("Could not read captured test output");
    }
}
