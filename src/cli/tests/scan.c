#include <string.h>
#include <stdio.h>

int main() {
    char data[4096];
    long long ofs=0;
    while (read(0, data, 4096)>0) {
        if (strnstr(data, "072 472 0401", 4096)) {
            printf("block %lld\n", ofs);
            break;
        }
        ofs++;
        if (ofs%(1024*10)==0) {
            printf("%lld Mb\r", ofs*4/1024);
            fflush(stdout);
        }
    }
}
