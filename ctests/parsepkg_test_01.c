#include <stdio.h>
#include "../constants.h"
#include "../xml_dump.h"
#include "../package.h"
#include "../parsepkg.h"

int main() {
    struct XmlStruct res;
    res = xml_from_package_file("icedax-1.1.10-2.fc14.i686.rpm", PKG_CHECKSUM_SHA256, "", "", 4);

    printf("Test - Start\n");
    printf("%s\n\n%s\n\n%s\n", res.primary, res.filelists, res.other);
    printf("Test - Done\n");

    free(res.primary);
    free(res.filelists);
    free(res.other);

    return 0;
}