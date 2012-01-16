#include <stdio.h>
#include "../package.h"
#include "../xml_dump.h"

int main() {
    Package *package = package_new();

    package->name = "ŠŠŠJMÉNÓ";
    package->pkgKey = 111;
    package->pkgId = "32543dsafchecksum";
    package->arch = "i386";
    package->version = "88";
    package->epoch = "11";
    package->release = "08";
    package->summary = "short summary";
    package->description = "long description";
    package->url = "http://foo.bar";
    package->time_file = 123456;
    package->time_build = 234567;
    package->rpm_license = "GPL";
    package->rpm_vendor = "we";
    package->rpm_group = "grupa";
    package->rpm_buildhost = "superman";
    package->rpm_sourcerpm = "neco.src.rpm";
    package->rpm_header_start = 1;
    package->rpm_header_end = 100;
    package->rpm_packager = "batman";
    package->size_package = 777;
    package->size_installed = 888;
    package->size_archive = 666;
    package->location_href = "tady";
    package->location_base = "";
    package->checksum_type = "sha256";


    int x;
    for (x=0; x<4; x++) {
        Dependency *file = dependency_new();
        file->name = "soubor";
        file->flags = "A";
        file->epoch = "1";
        file->version = "2";
        file->release = "3";
        file->pre = 1;
        package->requires = g_slist_append(package->requires, file);
    }

    for (x=0; x<3; x++) {
        Dependency *file = dependency_new();
        file->name = "soubor";
        file->flags = "A";
        file->epoch = "1";
        file->version = "2";
        file->release = "3";
        file->pre = 1;
        package->provides = g_slist_append(package->provides, file);
    }

    for (x=0; x<2; x++) {
        Dependency *file = dependency_new();
        file->name = "soubor";
        file->flags = "A";
        file->epoch = "1";
        file->version = "2";
        file->release = "3";
        file->pre = 1;
        package->conflicts = g_slist_append(package->conflicts, file);
    }

    Dependency *file = dependency_new();
    file->name = "soubor";
    file->flags = "A";
    file->epoch = "1";
    file->version = "2";
    file->release = "3";
    file->pre = 1;
    package->obsoletes = g_slist_append(package->obsoletes, file);


    PackageFile *pkgfile = package_file_new();
    pkgfile->path = "/bin/";
    pkgfile->name = "neco";
    pkgfile->type = "";

    PackageFile *pkgfile2 = package_file_new();
    pkgfile2->path = "/usr/";
    pkgfile2->name = "addresar";
    pkgfile2->type = "dir";

    PackageFile *pkgfile3 = package_file_new();
    pkgfile3->path = "/etc/";
    pkgfile3->name = "neco.conf";
    pkgfile3->type = "";

    PackageFile *pkgfile4 = package_file_new();
    pkgfile4->path = "/usr/lib/";
    pkgfile4->name = "sendmail";
    pkgfile4->type = "";

    PackageFile *pkgfile5 = package_file_new();
    pkgfile5->path = "/etc/";
    pkgfile5->name = "neco.dir";
    pkgfile5->type = "dir";

    PackageFile *pkgfile6 = package_file_new();
    pkgfile6->path = "/bin/";
    pkgfile6->name = "neco.dir";
    pkgfile6->type = "dir";

    PackageFile *pkgfile7 = package_file_new();
    pkgfile7->path = "/usr/lib/";
    pkgfile7->name = "sendmail";
    pkgfile7->type = "dir";

    package->files = g_slist_append(package->files, pkgfile);
    package->files = g_slist_append(package->files, pkgfile2);
    package->files = g_slist_append(package->files, pkgfile3);
    package->files = g_slist_append(package->files, pkgfile4);
    package->files = g_slist_append(package->files, pkgfile5);
    package->files = g_slist_append(package->files, pkgfile6);
    package->files = g_slist_append(package->files, pkgfile7);


/*
    ChangelogEntry *chlog = changelog_entry_new();
    chlog->author = "foobar";
    chlog->date = 1234567;
    chlog->changelog = "very long changelog message";

    ChangelogEntry *chlog2 = changelog_entry_new();
    chlog2->author = "foobar2";
    chlog2->date = 67890;
    chlog2->changelog = "short changelog message";

    package->changelogs = g_slist_append(package->changelogs, chlog);
    package->changelogs = g_slist_append(package->changelogs, chlog2);
*/

    //printf("Jde se dumpovat\n");
    char *ret = xml_dump_primary(package, NULL);
    printf("%s\n\n", ret);

    package_free(package);
    free(ret);

    return 0;
}

