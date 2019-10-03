#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int st;

    if ((argc != 2) || ((st = atoi(argv[1])) < 1)) {
        fprintf(stderr, "Usage: %s <positive number>\n", argv[0]);
        return 1;
    }
    while (st > 0) {
        printf("\r\033[2KSleeping for %d second%s...", st, (st > 1) ? "s" : "");
        fflush(stdout);
        sleep(1);
        st--;
    }
    putchar('\n');
    return 0;
}
