#include <string.h>
#include <inttypes.h>

#include "common.h"
#include "json.h"
#include "modules/api.h"
#include "modules/libxml.h"
#include "struct/hashtable.h"

typedef struct {
    bool uptodate;
    HashTable *records;
} domain_t;

typedef enum {
    RECORD_TYPE_ANY, /* special value to target any type */
    RECORD_TYPE_A,
    RECORD_TYPE_AAAA,
    RECORD_TYPE_CNAME,
    RECORD_TYPE_DKIM,
    RECORD_TYPE_LOC,
    RECORD_TYPE_MX,
    RECORD_TYPE_NAPTR,
    RECORD_TYPE_NS,
    RECORD_TYPE_PTR,
    RECORD_TYPE_SPF,
    RECORD_TYPE_SRV,
    RECORD_TYPE_SSHFP,
    RECORD_TYPE_TXT
} record_type_t;

static struct {
    const char *short_name;
    // ...
} record_type_map[] = {
    [ RECORD_TYPE_ANY ] = { "ANY" },
    [ RECORD_TYPE_A ] = { "A" },
    [ RECORD_TYPE_AAAA ] = { "AAAA" },
    [ RECORD_TYPE_CNAME ] = { "CNAME" },
    [ RECORD_TYPE_DKIM ] = { "DKIM" },
    [ RECORD_TYPE_LOC ] = { "LOC" },
    [ RECORD_TYPE_MX ] = { "MX" },
    [ RECORD_TYPE_NAPTR ] = { "NAPTR" },
    [ RECORD_TYPE_NS ] = { "NS" },
    [ RECORD_TYPE_PTR ] = { "PTR" },
    [ RECORD_TYPE_SPF ] = { "SPF" },
    [ RECORD_TYPE_SRV ] = { "SRV" },
    [ RECORD_TYPE_SSHFP ] = { "SSHFP" },
    [ RECORD_TYPE_TXT ] = { "TXT" }
};

typedef struct {
    uint32_t id;
    uint32_t ttl;
    const char *name;
    record_type_t type;
    const char *target;
} record_t;

typedef struct {
    char *domain;
    char *record; // also called subdomain
    char *value; // also called target
    char *type;
} record_argument_t;

#define MODULE_NAME "domain"
#define ACCOUNT_SPECIFIC 1

#ifndef ACCOUNT_SPECIFIC
// TODO: domains/records cache is not account specific
static HashTable *domains = NULL;
#endif

static void domain_destroy(void *data)
{
    domain_t *d;

    assert(NULL != data);

    d = (domain_t *) data;
    hashtable_destroy(d->records);
    free(d);
}

static void record_destroy(void *data)
{
    record_t *r;

    assert(NULL != data);

    r = (record_t *) data;
    if (NULL != r->name) {
        free((void *) r->name);
    }
    if (NULL != r->target) {
        free((void *) r->target);
    }
    free(r);
}

static domain_t *domain_new(void)
{
    domain_t *d;

    d = mem_new(*d);
    d->uptodate = FALSE;
    d->records = hashtable_new(NULL, value_equal, NULL, NULL, record_destroy);

    return d;
}

static void domain_on_set_account(void **data)
{
    if (NULL == *data) {
        *data = hashtable_ascii_cs_new((DupFunc) strdup, free, domain_destroy);
    }
}

static void domain_dtor(void)
{
#if 0
#ifdef ACCOUNT_SPECIFIC
    HashTable *domains;

    if (account_current_get_data(MODULE_NAME, /*(void **) */&domains)) {
#else
    if (NULL != domains) {
#endif
        hashtable_destroy(domains);
    }
#endif
}

static record_type_t xmlGetPropAsRecordType(xmlNodePtr node, const char *name)
{
    size_t i;
    xmlChar *value;
    record_type_t ret;

    ret = 0;
    if (NULL != (value = xmlGetProp(node, BAD_CAST name))) {
        for (i = 0; i < ARRAY_SIZE(record_type_map); i++) {
            if (0 == strcmp((const char *) value, record_type_map[i].short_name)) {
                ret = i;
                break;
            }
        }
        xmlFree(value);
    }

    return ret;
}

static int parse_record(HashTable *records, xmlDocPtr doc)
{
    record_t *r;
    xmlNodePtr root;

    if (NULL == (root = xmlDocGetRootElement(doc))) {
        return 0;
    }
    r = mem_new(*r);
    r->id = xmlGetPropAsInt(root, "id");
    r->ttl = xmlGetPropAsInt(root, "ttl");
    r->name = xmlGetPropAsString(root, "subDomain");
    r->target = xmlGetPropAsString(root, "target");
    r->type = xmlGetPropAsRecordType(root, "fieldType");
    hashtable_quick_put_ex(records, 0, r->id, NULL, r, NULL);
    xmlFreeDoc(doc);

    return TRUE;
}

/*
<opt>
  <anon>domain1.ext</anon>
  <anon>domain2.ext</anon>
</opt>
*/
static command_status_t domain_list(void *UNUSED(arg), error_t **error)
{
#ifdef ACCOUNT_SPECIFIC
    HashTable *domains;
#endif
    bool request_success;

    // populate
    // TODO: hashtable_size(domains) < 1 is not sufficient to known if we have the full list of domains in this hashtable
#ifdef ACCOUNT_SPECIFIC
    domains = NULL;
    account_current_get_data(MODULE_NAME, /*(void **) */&domains);
    assert(NULL != domains);
#endif
    if (hashtable_size(domains) < 1/* || (1 == argc && 0 == strcmp(argv[0], "nocache"))*/) {
        xmlDocPtr doc;
        request_t *req;
        xmlNodePtr root, n;

        req = request_get(REQUEST_FLAG_SIGN, API_BASE_URL "/domain");
        request_add_header(req, "Accept: text/xml");
        request_success = request_execute(req, RESPONSE_XML, (void **) &doc, error);
        request_dtor(req);

        if (request_success) {
            if (NULL == (root = xmlDocGetRootElement(doc))) {
                error_set(error, WARN, "Failed to parse XML document");
                return COMMAND_FAILURE;
            }
            hashtable_clear(domains);
            for (n = root->children; n != NULL; n = n->next) {
                xmlChar *content;

                content = xmlNodeGetContent(n);
                hashtable_put(domains, content, domain_new(), NULL);
                xmlFree(content);
            }
        } else {
            return COMMAND_FAILURE;
        }
    }
    // display
    {
        Iterator it;

        hashtable_to_iterator(&it, domains);
        for (iterator_first(&it); iterator_is_valid(&it); iterator_next(&it)) {
            const char *domain;

            iterator_current(&it, (void **) &domain);
            puts(domain);
        }
        iterator_close(&it);
    }

    return COMMAND_SUCCESS;
}

static int get_domain_records(const char *domain, domain_t **d, error_t **error)
{
#ifdef ACCOUNT_SPECIFIC
    HashTable *domains;
#endif
    bool request_success;

    *d = NULL;
#ifdef ACCOUNT_SPECIFIC
    domains = NULL;
    account_current_get_data(MODULE_NAME, /*(void **) */&domains);
    assert(NULL != domains);
#endif
    if (!hashtable_get(domains, (void *) domain, (void **) d) || !(*d)->uptodate) {
        xmlDocPtr doc;
        request_t *req;
        xmlNodePtr root, n;

        if (NULL == *d) {
            hashtable_put(domains, (void *) domain, *d = domain_new(), NULL);
        }
        req = request_get(REQUEST_FLAG_SIGN, API_BASE_URL "/domain/zone/%s/record", domain);
        request_add_header(req, "Accept: text/xml");
        request_success = request_execute(req, RESPONSE_XML, (void **) &doc, error);
        request_dtor(req);
        // result
        if (request_success) {
            if (NULL == (root = xmlDocGetRootElement(doc))) {
                return 0;
            }
            for (n = root->children; request_success && n != NULL; n = n->next) {
                xmlDocPtr doc;
                xmlChar *content;

                content = xmlNodeGetContent(n);
                req = request_get(REQUEST_FLAG_SIGN, API_BASE_URL "/domain/zone/%s/record/%s", domain, (const char *) content);
                request_add_header(req, "Accept: text/xml");
                request_success &= request_execute(req, RESPONSE_XML, (void **) &doc, error); // request_success is assumed to be TRUE before the first iteration
                request_dtor(req);
                // result
                parse_record((*d)->records, doc);
                xmlFree(content);
            }
            xmlFreeDoc(doc);
            (*d)->uptodate = TRUE;
        }
    }

    return request_success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

// TODO: optionnal arguments fieldType and subDomain in query string
static command_status_t record_list(void *arg, error_t **error)
{
    domain_t *d;
    Iterator it;
    command_status_t ret;
    record_argument_t *args;

    args = (record_argument_t *) arg;
    assert(NULL != args->domain);
    if (COMMAND_SUCCESS == (ret = get_domain_records(args->domain, &d, error))) {
        // display
        hashtable_to_iterator(&it, d->records);
        for (iterator_first(&it); iterator_is_valid(&it); iterator_next(&it)) {
            record_t *r;

            r = iterator_current(&it, NULL);
            printf(
                "%s %s%s%s => %s (ttl: %" PRIu32 ", id: %" PRIu32 ")\n",
                record_type_map[r->type].short_name,
                r->name,
                NULL == r->name || '\0' == *r->name ? "" : ".",
                args->domain,
                r->target,
                r->ttl,
                r->id
            );
        }
        iterator_close(&it);
    }

    return ret;
}

static bool str_include(const char *search, ...)
{
    char *str;
    bool found;
    va_list ap;

    found = FALSE;
    va_start(ap, search);
    while (!found && NULL != (str = va_arg(ap, char *))) {
        if (0 == strcmp(search, str)) {
            found = TRUE;
        }
    }
    va_end(ap);

    return found;
}

// ./ovh domain domain.ext record add toto type CNAME www
// NOTE: it seems OVH permits to create multiple times a DNS record with same name and type
static command_status_t record_add(void *arg, error_t **error)
{
    xmlDocPtr doc;
    bool request_success;
    record_argument_t *args;

    args = (record_argument_t *) arg;
    assert(NULL != args->type);
    assert(NULL != args->domain);
    assert(NULL != args->record);
    if (!str_include(args->type, "A", "AAAA", "CNAME", "DKIM", "LOC", "MX", "NAPTR", "NS", "PTR", "SPF", "SRV", "SSHFP", "TXT", NULL)) {
        error_set(error, WARN, "unknown DNS record type '%s'\n", args->type);
        return COMMAND_FAILURE;
    }
    {
        String *buffer;

        // data
        {
            json_value_t root;
            json_document_t *doc;

            buffer = string_new();
            doc = json_document_new();
            root = json_object();
            json_object_set_property(root, "target", json_string(args->value));
            json_object_set_property(root, "fieldType", json_string(args->type));
//             if ('\0' != *subdomain)
                json_object_set_property(root, "subDomain", json_string(args->record));
            json_document_set_root(doc, root);
            json_document_serialize(doc, buffer);
            json_document_destroy(doc);
        }
        // request
        {
            request_t *req;

            req = request_post(REQUEST_FLAG_SIGN, buffer->ptr, API_BASE_URL "/domain/zone/%s/record", args->domain);
            request_add_header(req, "Accept: text/xml");
            request_add_header(req, "Content-type: application/json");
            request_success = request_execute(req, RESPONSE_XML, (void **) &doc, error);
            request_dtor(req);
            string_destroy(buffer);
        }
    }
    // result
    if (request_success) {
        domain_t *d;
#ifdef ACCOUNT_SPECIFIC
        HashTable *domains;
#endif

        d = NULL;
#ifdef ACCOUNT_SPECIFIC
        domains = NULL;
        account_current_get_data(MODULE_NAME, /*(void **) */&domains);
        assert(NULL != domains);
#endif
        if (!hashtable_get(domains, (void *) args->domain, (void **) &d)) {
            hashtable_put(domains, (void *) args->domain, d = domain_new(), NULL);
        }
        parse_record(d->records, doc);
    }

    return request_success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

static command_status_t record_delete(void *arg, error_t **error)
{
    domain_t *d;
    record_t *match;
    bool request_success;
    record_argument_t *args;

    args = (record_argument_t *) arg;
    assert(NULL != args->domain);
    assert(NULL != args->record);
    if (request_success = (COMMAND_SUCCESS == get_domain_records(args->domain, &d, error))) {
#if 0
        // what was the goal? If true, it is more the domain which does not exist!
        if (!hashtable_get(domains, (void *) argv[0], (void **) &d)) {
            error_set(error, WARN, "Domain '%s' doesn't have any record named '%s'\n", argv[0], argv[3]);
            return COMMAND_FAILURE;
        }
#endif
        {
            Iterator it;
            size_t matches;

            matches = 0;
            hashtable_to_iterator(&it, d->records);
            for (iterator_first(&it); iterator_is_valid(&it); iterator_next(&it)) {
                record_t *r;

                r = iterator_current(&it, NULL);
                if (0 == strcmp(r->name, args->record)) { // TODO: strcmp_l? type?
                    ++matches;
                    match = r;
                }
            }
            iterator_close(&it);
            switch (matches) {
                case 1:
                {
                    printf("Confirm deletion of '%s.%s' (y/N)> ", match->name, args->domain);
                    fflush(stdout);
                    if ('y' != /*tolower*/(getchar())) {
                        return COMMAND_SUCCESS; // yeah, success because *we* canceled it
                    }
                    break;
                }
                case 0:
                    error_set(error, WARN, "Abort, no record match '%s'\n", args->record);
                    return COMMAND_FAILURE;
                default:
                    error_set(error, WARN, "Abort, more than one record match '%s'\n", args->record);
                    return COMMAND_FAILURE;
            }
        }
        {
            request_t *req;

            // request
#if 0
            req = request_delete(REQUEST_FLAG_SIGN, API_BASE_URL "/domain/zone/%s/%" PRIu32, args->domain, match->id);
            request_add_header(req, "Accept: text/xml");
            request_success = request_execute(req, RESPONSE_IGNORE, NULL, error);
            request_dtor(req);
#else
            printf("deletion of '%s.%s' done\n", match->name, args->domain);
#endif
            // result
            if (request_success) {
                hashtable_quick_delete(d->records, match->id, NULL, TRUE);
            }
        }
    }

    return request_success ? COMMAND_SUCCESS : COMMAND_FAILURE;
}

// arguments: [ttl <int>] [name <string>] [target <string>]
// (in any order)
// if we change record's name (subDomain), we need to update records cache
static command_status_t record_update(void *arg, error_t **error)
{
#if 0
    uint32_t ttl;
    String *buffer;
    request_t *req;
    const char *subdomain, *target;

    // data
    {
        json_value_t root;
        json_document_t *doc;

        buffer = string_new();
        doc = json_document_new();
        root = json_object();
        json_object_set_property(root, "target", json_string(target));
        json_object_set_property(root, "fieldType", json_string(type));
//             if ('\0' != *subdomain)
            json_object_set_property(root, "subDomain", json_string(subdomain));
        json_object_set_property(root, "ttl", json_integer(ttl));
        json_document_set_root(doc, root);
        json_document_serialize(doc, buffer);
        json_document_destroy(doc);
    }
    req = request_put(REQUEST_FLAG_SIGN, API_BASE_URL "/domain/zone/%s/%" PRIu32, argv[0], <id>);
    request_execute(req, RESPONSE_IGNORE, NULL, error);
    request_dtor(req);
#endif
    return TRUE;
}

static bool domain_ctor(graph_t *g)
{
#ifdef ACCOUNT_SPECIFIC
    account_register_module_callbacks(MODULE_NAME, (DtorFunc) hashtable_destroy, domain_on_set_account);
#else
    domains = hashtable_ascii_cs_new((DupFunc) strdup, free, domain_destroy);
#endif
    {
        argument_t
            *arg_domain,
            *arg_record, // called subdomain by OVH
            *arg_type,
            *arg_value // called target by OVH
        ;
        argument_t *lit_domain, *lit_domain_list;
        argument_t *lit_record, *lit_record_list, *lit_record_add, *lit_record_delete, *lit_record_update, *lit_record_type;

        // domain ...
        lit_domain = argument_create_literal("domain", NULL);
        lit_domain_list = argument_create_literal("list", domain_list);
        // domain X record ...
        lit_record = argument_create_literal("record", NULL);
        lit_record_list = argument_create_literal("list", record_list);
        lit_record_add = argument_create_literal("add", record_add);
        lit_record_delete = argument_create_literal("delete", record_delete);
        lit_record_update = argument_create_literal("update", record_update);
        lit_record_type = argument_create_literal("type", NULL);

        arg_domain = argument_create_string(offsetof(record_argument_t, domain), NULL, NULL);
        arg_record = argument_create_string(offsetof(record_argument_t, record), NULL, NULL);
        arg_type = argument_create_string(offsetof(record_argument_t, type), NULL, NULL); // TODO: choices
        arg_value = argument_create_string(offsetof(record_argument_t, value), NULL, NULL);

        // domain ...
        graph_create_full_path(g, lit_domain, lit_domain_list, NULL);
        // domain X record ...
        graph_create_full_path(g, lit_domain, arg_domain, lit_record, lit_record_list, NULL);
        graph_create_full_path(g, lit_domain, arg_domain, lit_record, arg_record, lit_record_add, arg_value, lit_record_type, arg_type, NULL);
        graph_create_full_path(g, lit_domain, arg_domain, lit_record, arg_record, lit_record_delete, NULL);
//         graph_create_full_path(g, lit_domain, arg_domain, lit_record, arg_record, lit_record_update, NULL);
    }

    return TRUE;
}

DECLARE_MODULE(domain) = {
    MODULE_NAME,
    domain_ctor,
    NULL,
    domain_dtor
};
