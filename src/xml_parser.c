#include <assert.h>
#include "error.h"
#include "xml_parser.h"
#include "xml_parser_internal.h"
#include "misc.h"

cr_ParserData *
cr_xml_parser_data(unsigned int numstates)
{
    cr_ParserData *pd = g_new0(cr_ParserData, 1);
    pd->ret = CRE_OK;
    pd->content = g_malloc(CONTENT_REALLOC_STEP);
    pd->acontent = CONTENT_REALLOC_STEP;
    pd->msgs = g_string_new(0);
    pd->swtab = g_malloc0(sizeof(cr_StatesSwitch *) * numstates);
    pd->sbtab = g_malloc(sizeof(unsigned int) * numstates);

    return pd;
}

char *
cr_xml_parser_data_free(cr_ParserData *pd)
{
    char *msgs;
    g_free(pd->content);
    msgs = g_string_free(pd->msgs, FALSE);
    g_free(pd->swtab);
    g_free(pd->sbtab);
    g_free(pd);
    return msgs;
}

void XMLCALL
cr_char_handler(void *pdata, const XML_Char *s, int len)
{
    int l;
    char *c;
    cr_ParserData *pd = pdata;

    if (pd->ret != CRE_OK)
        return; /* There was an error -> do nothing */

    if (!pd->docontent)
        return; /* Do not store the content */

    l = pd->lcontent + len + 1;
    if (l > pd->acontent) {
        pd->acontent = l + CONTENT_REALLOC_STEP;
        pd->content = realloc(pd->content, pd->acontent);
    }

    c = pd->content + pd->lcontent;
    pd->lcontent += len;
    while (len-- > 0)
        *c++ = *s++;
    *c = '\0';
}

int
cr_newpkgcb(cr_Package **pkg,
            const char *pkgId,
            const char *name,
            const char *arch,
            void *cbdata,
            GError **err)
{
    CR_UNUSED(pkgId);
    CR_UNUSED(name);
    CR_UNUSED(arch);
    CR_UNUSED(cbdata);

    assert(pkg && *pkg == NULL);
    assert(!err || *err == NULL);

    *pkg = cr_package_new();

    return CRE_OK;
}
