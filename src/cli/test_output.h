#ifndef TEST_OUTPUT_H
#define TEST_OUTPUT_H

#include <stdbool.h>
#include <stdio.h>

FILE* begin_test_output_capture(int* saved_stdout, int* saved_stderr);
bool end_test_output_capture(int saved_stdout, const int* saved_stderr);
void print_test_output(FILE* output, FILE* stream, const char* heading);

#endif
