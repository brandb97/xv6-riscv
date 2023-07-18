#include "kernel/types.h"
#include "user/user.h"

int main()
{
    if (fork() == -1) {
        while (smptest()) {
            sleep(1);
        }
    } else {
        do {
            sleep(1);
        } while (smptest());
    }
    exit(0);
}