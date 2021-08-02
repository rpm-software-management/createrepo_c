/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012-2013  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <Python.h>
#include <datetime.h> // from python

#include "src/createrepo_c.h"

#include "checksum-py.h"
#include "compression_wrapper-py.h"
#include "contentstat-py.h"
#include "exception-py.h"
#include "load_metadata-py.h"
#include "locate_metadata-py.h"
#include "misc-py.h"
#include "package-py.h"
#include "parsepkg-py.h"
#include "repomd-py.h"
#include "repomdrecord-py.h"
#include "sqlite-py.h"
#include "updatecollection-py.h"
#include "updatecollectionmodule-py.h"
#include "updatecollectionpackage-py.h"
#include "updateinfo-py.h"
#include "updaterecord-py.h"
#include "updatereference-py.h"
#include "xml_dump-py.h"
#include "xml_file-py.h"
#include "xml_parser-py.h"

struct module_state {
    PyObject *error;
};

static struct PyMethodDef createrepo_c_methods[] = {
    {"package_from_rpm",        (PyCFunction)py_package_from_rpm,
        METH_VARARGS | METH_KEYWORDS, package_from_rpm__doc__},
    {"xml_from_rpm",            (PyCFunction)py_xml_from_rpm,
        METH_VARARGS | METH_KEYWORDS, xml_from_rpm__doc__},
    {"xml_dump_primary",        (PyCFunction)py_xml_dump_primary,
        METH_VARARGS, xml_dump_primary__doc__},
    {"xml_dump_filelists",      (PyCFunction)py_xml_dump_filelists,
        METH_VARARGS, xml_dump_filelists__doc__},
    {"xml_dump_other",          (PyCFunction)py_xml_dump_other,
        METH_VARARGS, xml_dump_other__doc__},
    {"xml_dump_updaterecord",    (PyCFunction)py_xml_dump_updaterecord,
        METH_VARARGS, xml_dump_updaterecord__doc__},
    {"xml_dump",                (PyCFunction)py_xml_dump,
        METH_VARARGS, xml_dump__doc__},
    {"xml_parse_primary",       (PyCFunction)py_xml_parse_primary,
        METH_VARARGS, xml_parse_primary__doc__},
    {"xml_parse_primary_snippet",(PyCFunction)py_xml_parse_primary_snippet,
        METH_VARARGS, xml_parse_primary_snippet__doc__},
    {"xml_parse_filelists",     (PyCFunction)py_xml_parse_filelists,
        METH_VARARGS, xml_parse_filelists__doc__},
    {"xml_parse_filelists_snippet",(PyCFunction)py_xml_parse_filelists_snippet,
        METH_VARARGS, xml_parse_filelists_snippet__doc__},
    {"xml_parse_other",         (PyCFunction)py_xml_parse_other,
        METH_VARARGS, xml_parse_other__doc__},
    {"xml_parse_other_snippet",(PyCFunction)py_xml_parse_other_snippet,
        METH_VARARGS, xml_parse_other_snippet__doc__},
    {"xml_parse_repomd",        (PyCFunction)py_xml_parse_repomd,
        METH_VARARGS, xml_parse_repomd__doc__},
    {"xml_parse_updateinfo",    (PyCFunction)py_xml_parse_updateinfo,
        METH_VARARGS, xml_parse_updateinfo__doc__},
    {"xml_parse_main_metadata_together",(PyCFunctionWithKeywords)py_xml_parse_main_metadata_together,
        METH_VARARGS | METH_KEYWORDS, xml_parse_main_metadata_together__doc__},
    {"checksum_name_str",       (PyCFunction)py_checksum_name_str,
        METH_VARARGS, checksum_name_str__doc__},
    {"checksum_type",           (PyCFunction)py_checksum_type,
        METH_VARARGS, checksum_type__doc__},
    {"compress_file_with_stat", (PyCFunction)py_compress_file_with_stat,
        METH_VARARGS, compress_file_with_stat__doc__},
    {"decompress_file_with_stat",(PyCFunction)py_decompress_file_with_stat,
        METH_VARARGS, decompress_file_with_stat__doc__},
    {"compression_suffix",      (PyCFunction)py_compression_suffix,
        METH_VARARGS, compression_suffix__doc__},
    {"detect_compression",      (PyCFunction)py_detect_compression,
        METH_VARARGS, detect_compression__doc__},
    {"compression_type",        (PyCFunction)py_compression_type,
        METH_VARARGS, compression_type__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

static struct PyModuleDef createrepo_c_module_def = {
        PyModuleDef_HEAD_INIT,
        "_createrepo_c",
        NULL,
        sizeof(struct module_state),
        createrepo_c_methods,
        NULL,
        NULL,
        NULL,
        NULL
};

PyObject *
PyInit__createrepo_c(void)
{
    PyObject *m = PyModule_Create(&createrepo_c_module_def);
    if (!m)
        return NULL;

    /* Exceptions */
    if (!init_exceptions())
        return NULL;
    PyModule_AddObject(m, "CreaterepoCError", CrErr_Exception);

    /* Objects */

    /* _createrepo_c.ContentStat */
    if (PyType_Ready(&ContentStat_Type) < 0)
        return NULL;
    Py_INCREF(&ContentStat_Type);
    PyModule_AddObject(m, "ContentStat", (PyObject *)&ContentStat_Type);

    /* _createrepo_c.CrFile */
    if (PyType_Ready(&CrFile_Type) < 0)
        return NULL;
    Py_INCREF(&CrFile_Type);
    PyModule_AddObject(m, "CrFile", (PyObject *)&CrFile_Type);

    /* _createrepo_c.Package */
    if (PyType_Ready(&Package_Type) < 0)
        return NULL;
    Py_INCREF(&Package_Type);
    PyModule_AddObject(m, "Package", (PyObject *)&Package_Type);

    /* _createrepo_c.Metadata */
    if (PyType_Ready(&Metadata_Type) < 0)
        return NULL;
    Py_INCREF(&Metadata_Type);
    PyModule_AddObject(m, "Metadata", (PyObject *)&Metadata_Type);

    /* _createrepo_c.MetadataLocation */
    if (PyType_Ready(&MetadataLocation_Type) < 0)
        return NULL;
    Py_INCREF(&MetadataLocation_Type);
    PyModule_AddObject(m, "MetadataLocation", (PyObject *)&MetadataLocation_Type);

    /* _createrepo_c.Repomd */
    if (PyType_Ready(&Repomd_Type) < 0)
        return NULL;
    Py_INCREF(&Repomd_Type);
    PyModule_AddObject(m, "Repomd", (PyObject *)&Repomd_Type);

    /* _createrepo_c.RepomdRecord */
    if (PyType_Ready(&RepomdRecord_Type) < 0)
        return NULL;
    Py_INCREF(&RepomdRecord_Type);
    PyModule_AddObject(m, "RepomdRecord", (PyObject *)&RepomdRecord_Type);

    /* _createrepo_c.Sqlite */
    if (PyType_Ready(&Sqlite_Type) < 0)
        return NULL;
    Py_INCREF(&Sqlite_Type);
    PyModule_AddObject(m, "Sqlite", (PyObject *)&Sqlite_Type);

    /* _createrepo_c.UpdateCollection */
    if (PyType_Ready(&UpdateCollection_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateCollection_Type);
    PyModule_AddObject(m, "UpdateCollection",
                       (PyObject *)&UpdateCollection_Type);

    /* _createrepo_c.UpdateCollectionModule */
    if (PyType_Ready(&UpdateCollectionModule_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateCollectionModule_Type);
    PyModule_AddObject(m, "UpdateCollectionModule",
                       (PyObject *)&UpdateCollectionModule_Type);

    /* _createrepo_c.UpdateCollectionPackage */
    if (PyType_Ready(&UpdateCollectionPackage_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateCollectionPackage_Type);
    PyModule_AddObject(m, "UpdateCollectionPackage",
                       (PyObject *)&UpdateCollectionPackage_Type);

    /* _createrepo_c.UpdateInfo */
    if (PyType_Ready(&UpdateInfo_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateInfo_Type);
    PyModule_AddObject(m, "UpdateInfo", (PyObject *)&UpdateInfo_Type);

    /* _createrepo_c.UpdateRecord */
    if (PyType_Ready(&UpdateRecord_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateRecord_Type);
    PyModule_AddObject(m, "UpdateRecord", (PyObject *)&UpdateRecord_Type);

    /* _createrepo_c.UpdateReference */
    if (PyType_Ready(&UpdateReference_Type) < 0)
        return NULL;
    Py_INCREF(&UpdateReference_Type);
    PyModule_AddObject(m, "UpdateReference", (PyObject *)&UpdateReference_Type);

    /* _createrepo_c.XmlFile */
    if (PyType_Ready(&XmlFile_Type) < 0)
        return NULL;
    Py_INCREF(&XmlFile_Type);
    PyModule_AddObject(m, "XmlFile", (PyObject *)&XmlFile_Type);

    /* Createrepo init */

    cr_xml_dump_init();
    cr_package_parser_init();

    Py_AtExit(cr_xml_dump_cleanup);
    Py_AtExit(cr_package_parser_cleanup);

    /* Python macro to use datetime objects */

    PyDateTime_IMPORT;

    /* Module constants */

    /* Version */
    PyModule_AddIntConstant(m, "VERSION_MAJOR", CR_VERSION_MAJOR);
    PyModule_AddIntConstant(m, "VERSION_MINOR", CR_VERSION_MINOR);
    PyModule_AddIntConstant(m, "VERSION_PATCH", CR_VERSION_PATCH);

    /* Checksum types */
    PyModule_AddIntConstant(m, "CHECKSUM_UNKNOWN", CR_CHECKSUM_UNKNOWN);
    PyModule_AddIntConstant(m, "MD5", CR_CHECKSUM_MD5);
    PyModule_AddIntConstant(m, "SHA", CR_CHECKSUM_SHA);
    PyModule_AddIntConstant(m, "SHA1", CR_CHECKSUM_SHA1);
    PyModule_AddIntConstant(m, "SHA224", CR_CHECKSUM_SHA224);
    PyModule_AddIntConstant(m, "SHA256", CR_CHECKSUM_SHA256);
    PyModule_AddIntConstant(m, "SHA384", CR_CHECKSUM_SHA384);
    PyModule_AddIntConstant(m, "SHA512", CR_CHECKSUM_SHA512);

    /* File open modes */
    PyModule_AddIntConstant(m, "MODE_READ", CR_CW_MODE_READ);
    PyModule_AddIntConstant(m, "MODE_WRITE", CR_CW_MODE_WRITE);

    /* Compression types */
    PyModule_AddIntConstant(m, "AUTO_DETECT_COMPRESSION", CR_CW_AUTO_DETECT_COMPRESSION);
    PyModule_AddIntConstant(m, "UNKNOWN_COMPRESSION", CR_CW_UNKNOWN_COMPRESSION);
    PyModule_AddIntConstant(m, "NO_COMPRESSION", CR_CW_NO_COMPRESSION);
    PyModule_AddIntConstant(m, "GZ_COMPRESSION", CR_CW_GZ_COMPRESSION);
    PyModule_AddIntConstant(m, "BZ2_COMPRESSION", CR_CW_BZ2_COMPRESSION);
    PyModule_AddIntConstant(m, "XZ_COMPRESSION", CR_CW_XZ_COMPRESSION);
    PyModule_AddIntConstant(m, "ZCK_COMPRESSION", CR_CW_ZCK_COMPRESSION);

    /* Zchunk support */
#ifdef WITH_ZCHUNK
    PyModule_AddIntConstant(m, "HAS_ZCK", 1);
#else
    PyModule_AddIntConstant(m, "HAS_ZCK", 0);
#endif // WITH_ZCHUNK

    /* Load Metadata key values */
    PyModule_AddIntConstant(m, "HT_KEY_DEFAULT", CR_HT_KEY_DEFAULT);
    PyModule_AddIntConstant(m, "HT_KEY_HASH", CR_HT_KEY_HASH);
    PyModule_AddIntConstant(m, "HT_KEY_NAME", CR_HT_KEY_NAME);
    PyModule_AddIntConstant(m, "HT_KEY_FILENAME", CR_HT_KEY_FILENAME);

    /* Load Metadata key dup action */
    PyModule_AddIntConstant(m, "HT_DUPACT_KEEPFIRST", CR_HT_DUPACT_KEEPFIRST);
    PyModule_AddIntConstant(m, "HT_DUPACT_REMOVEALL", CR_HT_DUPACT_REMOVEALL);

    /* Sqlite DB types */
    PyModule_AddIntConstant(m, "DB_PRIMARY", CR_DB_PRIMARY);
    PyModule_AddIntConstant(m, "DB_FILELISTS", CR_DB_FILELISTS);
    PyModule_AddIntConstant(m, "DB_OTHER", CR_DB_OTHER);

    /* XmlFile types */
    PyModule_AddIntConstant(m, "XMLFILE_PRIMARY", CR_XMLFILE_PRIMARY);
    PyModule_AddIntConstant(m, "XMLFILE_FILELISTS", CR_XMLFILE_FILELISTS);
    PyModule_AddIntConstant(m, "XMLFILE_OTHER", CR_XMLFILE_OTHER);
    PyModule_AddIntConstant(m, "XMLFILE_PRESTODELTA", CR_XMLFILE_PRESTODELTA);
    PyModule_AddIntConstant(m, "XMLFILE_UPDATEINFO", CR_XMLFILE_UPDATEINFO);

    /* XmlParser types */
    PyModule_AddIntConstant(m, "XML_WARNING_UNKNOWNTAG", CR_XML_WARNING_UNKNOWNTAG);
    PyModule_AddIntConstant(m, "XML_WARNING_MISSINGATTR", CR_XML_WARNING_MISSINGATTR);
    PyModule_AddIntConstant(m, "XML_WARNING_UNKNOWNVAL", CR_XML_WARNING_UNKNOWNVAL);
    PyModule_AddIntConstant(m, "XML_WARNING_BADATTRVAL", CR_XML_WARNING_BADATTRVAL);

    return m;
}
