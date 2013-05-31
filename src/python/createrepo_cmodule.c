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

#include "src/createrepo_c.h"

#include "exception-py.h"
#include "load_metadata-py.h"
#include "locate_metadata-py.h"
#include "package-py.h"
#include "parsepkg-py.h"
#include "repomd-py.h"
#include "repomdrecord-py.h"
#include "sqlite-py.h"
#include "xml_dump-py.h"
#include "xml_file-py.h"
#include "xml_parser-py.h"

static struct PyMethodDef createrepo_c_methods[] = {
    {"package_from_rpm",        (PyCFunction)py_package_from_rpm,
     METH_VARARGS | METH_KEYWORDS, NULL},
    {"xml_from_rpm",            (PyCFunction)py_xml_from_rpm,
     METH_VARARGS | METH_KEYWORDS, NULL},
    {"xml_dump_primary",        (PyCFunction)py_xml_dump_primary,
     METH_VARARGS, NULL},
    {"xml_dump_filelists",      (PyCFunction)py_xml_dump_filelists,
     METH_VARARGS, NULL},
    {"xml_dump_other",          (PyCFunction)py_xml_dump_other,
     METH_VARARGS, NULL},
    {"xml_dump",                (PyCFunction)py_xml_dump,
     METH_VARARGS, NULL},
    {"xml_parse_primary",       (PyCFunction)py_xml_parse_primary,
     METH_VARARGS, NULL},
    {"xml_parse_filelists",     (PyCFunction)py_xml_parse_filelists,
     METH_VARARGS, NULL},
    {"xml_parse_other",         (PyCFunction)py_xml_parse_other,
     METH_VARARGS, NULL},
    { NULL }
};

PyMODINIT_FUNC
init_createrepo_c(void)
{
    PyObject *m = Py_InitModule("_createrepo_c", createrepo_c_methods);
    if (!m)
        return;

    /* Exceptions */
    if (!init_exceptions())
        return;
    PyModule_AddObject(m, "CreaterepoCError", CrErr_Exception);

    /* Objects */

    /* _createrepo_c.Package */
    if (PyType_Ready(&Package_Type) < 0)
        return;
    Py_INCREF(&Package_Type);
    PyModule_AddObject(m, "Package", (PyObject *)&Package_Type);

    /* _createrepo_c.Metadata */
    if (PyType_Ready(&Metadata_Type) < 0)
        return;
    Py_INCREF(&Metadata_Type);
    PyModule_AddObject(m, "Metadata", (PyObject *)&Metadata_Type);

    /* _createrepo_c.MetadataLocation */
    if (PyType_Ready(&MetadataLocation_Type) < 0)
        return;
    Py_INCREF(&MetadataLocation_Type);
    PyModule_AddObject(m, "MetadataLocation", (PyObject *)&MetadataLocation_Type);

    /* _createrepo_c.Repomd */
    if (PyType_Ready(&Repomd_Type) < 0)
        return;
    Py_INCREF(&Repomd_Type);
    PyModule_AddObject(m, "Repomd", (PyObject *)&Repomd_Type);

    /* _createrepo_c.RepomdRecord */
    if (PyType_Ready(&RepomdRecord_Type) < 0)
        return;
    Py_INCREF(&RepomdRecord_Type);
    PyModule_AddObject(m, "RepomdRecord", (PyObject *)&RepomdRecord_Type);

    /* _createrepo_c.Sqlite */
    if (PyType_Ready(&Sqlite_Type) < 0)
        return;
    Py_INCREF(&Sqlite_Type);
    PyModule_AddObject(m, "Sqlite", (PyObject *)&Sqlite_Type);

    /* _createrepo_c.XmlFile */
    if (PyType_Ready(&XmlFile_Type) < 0)
        return;
    Py_INCREF(&XmlFile_Type);
    PyModule_AddObject(m, "XmlFile", (PyObject *)&XmlFile_Type);

    /* Createrepo init */

    cr_xml_dump_init();
    cr_package_parser_init();

    /* Module constants */

    /* Version */
    PyModule_AddIntConstant(m, "VERSION_MAJOR", CR_VERSION_MAJOR);
    PyModule_AddIntConstant(m, "VERSION_MINOR", CR_VERSION_MINOR);
    PyModule_AddIntConstant(m, "VERSION_PATCH", CR_VERSION_PATCH);

    /* Checksum types */
    PyModule_AddIntConstant(m, "MD5", CR_CHECKSUM_MD5);
    PyModule_AddIntConstant(m, "SHA1", CR_CHECKSUM_SHA1);
    PyModule_AddIntConstant(m, "SHA256", CR_CHECKSUM_SHA256);

    /* Compression types */
    PyModule_AddIntConstant(m, "AUTO_DETECT_COMPRESSION", CR_CW_AUTO_DETECT_COMPRESSION);
    PyModule_AddIntConstant(m, "UNKNOWN_COMPRESSION", CR_CW_UNKNOWN_COMPRESSION);
    PyModule_AddIntConstant(m, "NO_COMPRESSION", CR_CW_NO_COMPRESSION);
    PyModule_AddIntConstant(m, "GZ_COMPRESSION", CR_CW_GZ_COMPRESSION);
    PyModule_AddIntConstant(m, "BZ2_COMPRESSION", CR_CW_BZ2_COMPRESSION);
    PyModule_AddIntConstant(m, "XZ_COMPRESSION", CR_CW_XZ_COMPRESSION);

    /* Load Metadata key values */
    PyModule_AddIntConstant(m, "HT_KEY_DEFAULT", CR_HT_KEY_DEFAULT);
    PyModule_AddIntConstant(m, "HT_KEY_HASH", CR_HT_KEY_HASH);
    PyModule_AddIntConstant(m, "HT_KEY_NAME", CR_HT_KEY_NAME);
    PyModule_AddIntConstant(m, "HT_KEY_FILENAME", CR_HT_KEY_FILENAME);

    /* Sqlite DB types */
    PyModule_AddIntConstant(m, "DB_PRIMARY", CR_DB_PRIMARY);
    PyModule_AddIntConstant(m, "DB_FILELISTS", CR_DB_FILELISTS);
    PyModule_AddIntConstant(m, "DB_OTHER", CR_DB_OTHER);

    /* XmlFile types */
    PyModule_AddIntConstant(m, "XMLFILE_PRIMARY", CR_XMLFILE_PRIMARY);
    PyModule_AddIntConstant(m, "XMLFILE_FILELISTS", CR_XMLFILE_FILELISTS);
    PyModule_AddIntConstant(m, "XMLFILE_OTHER", CR_XMLFILE_OTHER);

    /* XmlParser types */
    PyModule_AddIntConstant(m, "XML_WARNING_UNKNOWNTAG", CR_XML_WARNING_UNKNOWNTAG);
    PyModule_AddIntConstant(m, "XML_WARNING_MISSINGATTR", CR_XML_WARNING_MISSINGATTR);
    PyModule_AddIntConstant(m, "XML_WARNING_UNKNOWNVAL", CR_XML_WARNING_UNKNOWNVAL);
    PyModule_AddIntConstant(m, "XML_WARNING_BADATTRVAL", CR_XML_WARNING_BADATTRVAL);
}
