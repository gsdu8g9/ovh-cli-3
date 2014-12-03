#include <openssl/ssl.h>
#include <openssl/evp.h>
#include "common.h"

static bool openssl_ctor(void)
{
    SSL_library_init();
    OpenSSL_add_all_digests();

    return TRUE;
}

static void openssl_dtor(void)
{
    EVP_cleanup();
}

DECLARE_MODULE(openssl) = {
    "openssl",
    NULL,
    openssl_ctor,
    NULL,
    openssl_dtor
};
