#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include "fcm_token.h"

#define MAX_TOKEN_SIZE 2048
#define MAX_JWT_SIZE 4096

// Structure to store HTTP response
struct APIResponse {
    char *data;
    size_t size;
};

// Callback to receive HTTP response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct APIResponse *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        printf("Error: insufficient memory (realloc)\n");
        return 0;
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

static char* base64_url_encode(const unsigned char* input, int length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    char *encoded = malloc(bufferPtr->length + 1);
    memcpy(encoded, bufferPtr->data, bufferPtr->length);
    encoded[bufferPtr->length] = '\0';

    BIO_free_all(bio);

    // Convert to Base64 URL-safe (replace + with -, / with _, remove =)
    size_t len = strlen(encoded);
    for (size_t i = 0; i < len; i++) {
        if (encoded[i] == '+') encoded[i] = '-';
        else if (encoded[i] == '/') encoded[i] = '_';
        else if (encoded[i] == '=') {
            encoded[i] = '\0';
            break;
        }
    }

    return encoded;
}

// Function to sign with RSA SHA256
static char* sign_jwt(const char* message, EVP_PKEY* private_key) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) return NULL;

    if (EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, private_key) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }

    if (EVP_DigestSignUpdate(mdctx, message, strlen(message)) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }

    size_t sig_len;
    if (EVP_DigestSignFinal(mdctx, NULL, &sig_len) <= 0) {
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }

    unsigned char *sig = malloc(sig_len);
    if (EVP_DigestSignFinal(mdctx, sig, &sig_len) <= 0) {
        EVP_MD_CTX_free(mdctx);
        free(sig);
        return NULL;
    }

    EVP_MD_CTX_free(mdctx);

    char* signature = base64_url_encode(sig, sig_len);
    free(sig);

    return signature;
}

static char* create_jwt(const char* client_email, const char* private_key_str) {
    if (!client_email || !private_key_str) {
        printf("Error: NULL parameters\n");
        return NULL;
    }

    // JWT header
    const char* header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    char* header_encoded = base64_url_encode((const unsigned char*)header, strlen(header));
    if (!header_encoded) {
        printf("Error: header encoding\n");
        return NULL;
    }

    // JWT payload
    time_t now = time(NULL);
    time_t exp = now + 3600; // Expires in 1 hour

    char payload[1024];
    snprintf(payload, sizeof(payload),
        "{"
        "\"iss\":\"%s\","
        "\"scope\":\"https://www.googleapis.com/auth/firebase.messaging\","
        "\"aud\":\"https://oauth2.googleapis.com/token\","
        "\"exp\":%ld,"
        "\"iat\":%ld"
        "}",
        client_email, exp, now);

    char* payload_encoded = base64_url_encode((const unsigned char*)payload, strlen(payload));
    if (!payload_encoded) {
        printf("Error: payload encoding\n");
        free(header_encoded);
        return NULL;
    }

    // Message to sign (header.payload)
    size_t message_len = strlen(header_encoded) + strlen(payload_encoded) + 2;
    char* message = malloc(message_len);
    if (!message) {
        printf("Error: message memory allocation\n");
        free(header_encoded);
        free(payload_encoded);
        return NULL;
    }
    snprintf(message, message_len, "%s.%s", header_encoded, payload_encoded);

    printf("Debug: Loading private key...\n");

    // Load private key
    BIO *bio = BIO_new_mem_buf(private_key_str, -1);
    if (!bio) {
        printf("Error: BIO creation\n");
        free(header_encoded);
        free(payload_encoded);
        free(message);
        return NULL;
    }

    EVP_PKEY *private_key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (!private_key) {
        printf("Error: failed to load private key\n");
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        printf("OpenSSL error detail: %s\n", err_buf);
        free(header_encoded);
        free(payload_encoded);
        free(message);
        return NULL;
    }

    printf("Debug: Private key loaded, signing...\n");

    // Sign the message
    char* signature = sign_jwt(message, private_key);
    EVP_PKEY_free(private_key);

    if (!signature) {
        printf("Error: failed to sign JWT\n");
        free(header_encoded);
        free(payload_encoded);
        free(message);
        return NULL;
    }

    // Assemble final JWT
    size_t jwt_len = strlen(message) + strlen(signature) + 2;
    char* jwt = malloc(jwt_len);
    if (!jwt) {
        printf("Error: JWT allocation\n");
        free(header_encoded);
        free(payload_encoded);
        free(message);
        free(signature);
        return NULL;
    }
    snprintf(jwt, jwt_len, "%s.%s", message, signature);

    free(header_encoded);
    free(payload_encoded);
    free(message);
    free(signature);

    return jwt;
}

static char* extract_access_token(const char* json_response) {
    json_object *root = json_tokener_parse(json_response);
    if (!root) return NULL;

    json_object *acces*_
