#include <stdio.h>
#include "src/long.h"

int main(int argc, const char** argv)
{
    if (argc != 2) {
        printf("Usage: ./long $file-path\n");
        return 0;
    }

    return long_run_file(argv[1]);
}
