#include <stdio.h>
#include "../constants.h"
#include "../xml_dump.h"
#include "../package.h"
#include "../parsepkg.h"

int main() {
    struct XmlStruct res;

    init_package_parser();

    int x;
    for (x=0; x<1000; x++) {
        res = xml_from_package_file("icedax-1.1.10-2.fc14.i686.rpm", PKG_CHECKSUM_SHA256, "", "", 5, NULL);
        free(res.primary);
        free(res.filelists);
        free(res.other);
    }

    res = xml_from_package_file("icedax-1.1.10-2.fc14.i686.rpm", PKG_CHECKSUM_SHA256, "", "", 5, NULL);

    free_package_parser();

    printf("Test - Start\n");
    printf("%s\n\n%s\n\n%s\n", res.primary, res.filelists, res.other);
    printf("Test - Done\n");

    free(res.primary);
    free(res.filelists);
    free(res.other);

    return 0;
}