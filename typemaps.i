
#ifdef SWIGPYTHON

%typemap(in) gint64 {
    $1 = ($1_type)PyInt_AsLong($input);
}

%typemap(out) gint64 {
    $result = PyInt_FromLong($1);
}

%typemap(in) gint8, gint16, gint32, gint64, gshort, glong {
    $1 = ($1_type)PyInt_AsLong($input);
}

%typemap(out) gint8, gint16, gint32, gint64, gshort, glong {
    $result = PyInt_FromLong($1);
}

%typemap(in) guint8, guint16, guint32, guint64, gushort, gulong {
    $1 = ($1_type)PyLong_AsUnsignedLong($input);
}

%typemap(out) guint8, guint16, guint32, guint64, gushort, gulong {
    $result = PyLong_FromUnsignedLong($1);
}

%typemap(in) gchar * {
    $1 = ($1_type)PyString_AsString($input);
}

%typemap(out) gchar * {
    $result = PyString_FromString($1);
}

%typemap(in) gboolean {
    if ($input == Py_True)
        $1 = TRUE;
    else if ($input == Py_False)
        $1 = FALSE;
    else
    {
        PyErr_SetString(
            PyExc_ValueError,
            "Python object passed to a gboolean argument was not True "
            "or False" );
        return NULL;
    }
}

%typemap(out) gboolean {
    if ($1 == TRUE)
    {
        Py_INCREF(Py_True);
        $result = Py_True;
    }
    else if ($1 == FALSE)
    {
        Py_INCREF(Py_False);
        $result = Py_False;
    }
    else
    {
        PyErr_SetString(
            PyExc_ValueError,
            "function returning gboolean returned a value that wasn't "
            "TRUE or FALSE.");
        return NULL;
    }
}

%typemap(out) GHashTable *load_metadata {
    PyObject *dict = PyDict_New();

    GHashTable *hash_table = $1;
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        struct package_metadata *md = (struct package_metadata *) value;
        PyObject *py_value = PyTuple_New(6);
        PyTuple_SetItem(py_value, 0, PyLong_FromLong(md->time_file));
        PyTuple_SetItem(py_value, 1, PyLong_FromLong(md->size_package));
        PyTuple_SetItem(py_value, 2, PyString_FromString(md->checksum_type));
        PyTuple_SetItem(py_value, 3, PyString_FromString(md->primary_xml));
        PyTuple_SetItem(py_value, 4, PyString_FromString(md->filelists_xml));
        PyTuple_SetItem(py_value, 5, PyString_FromString(md->other_xml));
        PyDict_SetItemString(dict, key, py_value);
    }
    g_hash_table_destroy (hash_table);

    $result = dict;
}

/*
%typemap(out) GSList *changelogs {
    PyObject *list = PyList_New(0);
    GSList *element = NULL;
    for(element = $1; element; element=element->next) {
        void* feature = (void*) element->data;
        PyList_Append(list, feature);
    }
    $result = list;
}

%typemap(in) GSList *changelogs {
    int i;
    GSList *list = NULL;
    for (i=0; i < PyList_Size($input); i++) {
        printf("OK\n");
        ChangelogEntry *item = (ChangelogEntry*) PyList_GetItem($input, i);
        printf("item: %s\n", item->author);
        g_slist_prepend (list, item);
    }
    list = g_slist_reverse(list);
    $1 = list;
}
*/

%typemap(in) Package * {
    char *str = NULL;
    long num = 0;
    PyObject *val = NULL;
    Package *pkg = package_new();

    val = PyDict_GetItemString($input, "checksum");
    if (val) {
        pkg->pkgId = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "name");
    if (val) {
        pkg->name = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "arch");
    if (val) {
        pkg->arch = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "version");
    if (val) {
        pkg->version = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "epoch");
    if (val) {
        pkg->epoch = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "release");
    if (val) {
        pkg->release = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "summary");
    if (val) {
        pkg->summary = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "description");
    if (val) {
        pkg->description = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "url");
    if (val) {
        pkg->url = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "time_file");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->time_file = num;

    val = PyDict_GetItemString($input, "time_build");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->time_build = num;

    val = PyDict_GetItemString($input, "rpm_license");
    if (val) {
        pkg->rpm_license = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "rpm_vendor");
    if (val) {
        pkg->rpm_vendor = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "rpm_group");
    if (val) {
        pkg->rpm_group = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "rpm_buildhost");
    if (val) {
        pkg->rpm_buildhost = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "rpm_sourcerpm");
    if (val) {
        pkg->rpm_sourcerpm = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "rpm_header_start");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->rpm_header_start = num;

    val = PyDict_GetItemString($input, "rpm_header_end");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->rpm_header_end = num;

    val = PyDict_GetItemString($input, "rpm_packager");
    if (val) {
        pkg->rpm_packager = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "size_package");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->size_package = num;

    val = PyDict_GetItemString($input, "size_installed");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->size_installed = num;

    val = PyDict_GetItemString($input, "size_archive");
    if (val) num = PyLong_AsLong(val); else num = 0;
    pkg->size_archive = num;

    val = PyDict_GetItemString($input, "location_href");
    if (val) {
        pkg->location_href = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "location_base");
    if (val) {
        pkg->location_base = PyString_AsString(val);
    }

    val = PyDict_GetItemString($input, "checksum_type");
    if (val) {
        pkg->checksum_type = PyString_AsString(val);
    }


    // Files
    PyObject *files = NULL;
    files = PyDict_GetItemString($input, "files");
    if (files && PyList_Check(files)) {
        Py_ssize_t x;
        Py_ssize_t len = PyList_Size(files);
        for (x=0; x < len; x++) {
            PyObject *file_tuple = PyList_GetItem(files, x);
            if (file_tuple && PyTuple_Check(file_tuple) && PyTuple_Size(file_tuple) == 3) {
                PyObject *name = PyTuple_GetItem(file_tuple, 0);
                PyObject *path = PyTuple_GetItem(file_tuple, 1);
                PyObject *type = PyTuple_GetItem(file_tuple, 2);
                if (name && type && PyString_Check(name), PyString_Check(type)) {
                    PackageFile *pkg_f = package_file_new();
                    pkg_f->name = PyString_AsString(name);
                    pkg_f->path = PyString_AsString(path);
                    pkg_f->type = PyString_AsString(type);
                    pkg->files = g_slist_prepend(pkg->files, pkg_f);
                }
            }
        }
        pkg->files = g_slist_reverse(pkg->files);
    }


    // Provides, obsoletes, conflicts, requires
    int type;
    char *types[] = {"provides", "obsoletes", "conflicts", "requires"};

    for (type=0; type < 4; type++) {
        GSList **dep_list = NULL;
        switch (type) {
            case 0:
                dep_list = &(pkg->provides);
                break;
            case 1:
                dep_list = &(pkg->obsoletes);
                break;
            case 2:
                dep_list = &(pkg->conflicts);
                break;
            case 3:
                dep_list = &(pkg->requires);
                break;
        }

        files = NULL;
        files = PyDict_GetItemString($input, types[type]);
        if (files && PyList_Check(files)) {
            Py_ssize_t x;
            Py_ssize_t len = PyList_Size(files);
            for (x=0; x < len; x++) {
                PyObject *file_tuple = PyList_GetItem(files, x);
                Py_ssize_t tuple_len = PyTuple_Size(file_tuple);
                if (file_tuple && PyTuple_Check(file_tuple) && (tuple_len == 5 || tuple_len == 6)) {
                    PyObject *name = PyTuple_GetItem(file_tuple, 0);
                    PyObject *flags = PyTuple_GetItem(file_tuple, 1);
                    PyObject *epoch = PyTuple_GetItem(file_tuple, 2);
                    PyObject *version = PyTuple_GetItem(file_tuple, 3);
                    PyObject *release = PyTuple_GetItem(file_tuple, 4);
                    PyObject *pre = NULL;
                    if (tuple_len == 6) {
                        pre = PyTuple_GetItem(file_tuple, 5);
                    }

                    if (!name || !PyString_Check(name)) {
                        continue;
                    }
                    Dependency *pkg_d = dependency_new();
                    pkg_d->name = PyString_AsString(name);

                    if (pre && PyInt_Check(pre)) {
                        pkg_d->pre = PyInt_AsLong(pre);
                    }

                    if (!flags || !PyString_Check(flags)) {
                        continue;
                    }
                    pkg_d->flags = PyString_AsString(flags);

                    if (epoch && PyString_Check(epoch)) {
                        pkg_d->epoch = PyString_AsString(epoch);
                    }
                    if (version && PyString_Check(version)) {
                        pkg_d->version = PyString_AsString(version);
                    }
                    if (release && PyString_Check(release)) {
                        pkg_d->release = PyString_AsString(release);
                    }

                    *dep_list = g_slist_prepend(*dep_list, pkg_d);
                } // end if
            } // end for
            *dep_list = g_slist_reverse(*dep_list);
        } // end if
    } // end for


    // Changelogs
    PyObject *changelogs = NULL;
    changelogs = PyDict_GetItemString($input, "changelogs");
    if (changelogs && PyList_Check(changelogs)) {
        Py_ssize_t x;
        Py_ssize_t len = PyList_Size(changelogs);
        for (x=0; x < len; x++) {
            PyObject *changelog_tuple = PyList_GetItem(changelogs, x);
            if (changelog_tuple && PyTuple_Check(changelog_tuple) && PyTuple_Size(changelog_tuple) == 3) {
                PyObject *date = PyTuple_GetItem(changelog_tuple, 0);
                PyObject *author = PyTuple_GetItem(changelog_tuple, 1);
                PyObject *changelog = PyTuple_GetItem(changelog_tuple, 2);
                if (date && author && changelog && (PyLong_Check(date) || PyInt_Check(date))
                   && PyString_Check(author) && PyString_Check(changelog)) {
                    ChangelogEntry *pkg_ch = changelog_entry_new();
                    pkg_ch->date = PyLong_AsLong(date);
                    pkg_ch->author = PyString_AsString(author);
                    pkg_ch->changelog = PyString_AsString(changelog);
                    pkg->changelogs = g_slist_prepend(pkg->changelogs, pkg_ch);
                }
            }
        }
        pkg->changelogs = g_slist_reverse(pkg->changelogs);
    }

    $1 = pkg;
}


%typemap(in) Header {
    struct hdrObject_s {
        PyObject_HEAD
        Header h;
        HeaderIterator hi;
    };

    //$input
    Header h;
    h = ((struct hdrObject_s *) $input)->h;
    $1 = h;
}

#endif

