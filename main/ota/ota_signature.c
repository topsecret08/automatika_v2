#include "ota_signature.h"
#include "ota_pubkey.h"

#include "esp_log.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/sha256.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "OTA_SIG";

#define SIG_MAGIC "ATKSIG01"
#define SIG_MAGIC_LEN 8
#define SIG_FIXED_LEN (2 + SIG_MAGIC_LEN)
#define SIG_MAX_LEN 80
#define SIG_MIN_LEN 8
#define HASH_CHUNK 4096

esp_err_t ota_signature_verify(const esp_partition_t *part, size_t total_len, size_t *image_len) {
    if (!part || !image_len) return ESP_ERR_INVALID_ARG;
    if (total_len <= SIG_FIXED_LEN + SIG_MIN_LEN || total_len > part->size) return ESP_ERR_INVALID_SIZE;

    uint8_t tail[SIG_FIXED_LEN];
    esp_err_t err = esp_partition_read(part, total_len - SIG_FIXED_LEN, tail, sizeof(tail));
    if (err != ESP_OK) return err;
    if (memcmp(tail + 2, SIG_MAGIC, SIG_MAGIC_LEN) != 0) {
        ESP_LOGE(TAG, "no signature trailer");
        return ESP_ERR_NOT_FOUND;
    }

    size_t sig_len = ((size_t)tail[0] << 8) | tail[1];
    if (sig_len < SIG_MIN_LEN || sig_len > SIG_MAX_LEN || sig_len + SIG_FIXED_LEN >= total_len) {
        ESP_LOGE(TAG, "bad signature length %u", (unsigned)sig_len);
        return ESP_FAIL;
    }
    *image_len = total_len - SIG_FIXED_LEN - sig_len;

    uint8_t sig[SIG_MAX_LEN];
    err = esp_partition_read(part, *image_len, sig, sig_len);
    if (err != ESP_OK) return err;

    uint8_t *buf = malloc(HASH_CHUNK);
    if (!buf) return ESP_ERR_NO_MEM;

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    size_t off = 0;
    while (off < *image_len) {
        size_t n = *image_len - off;
        if (n > HASH_CHUNK) n = HASH_CHUNK;
        err = esp_partition_read(part, off, buf, n);
        if (err != ESP_OK) break;
        mbedtls_sha256_update(&sha, buf, n);
        off += n;
    }
    free(buf);
    uint8_t hash[32];
    if (err == ESP_OK) mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);
    if (err != ESP_OK) return err;

    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0) rc = mbedtls_ecp_point_read_binary(&grp, &Q, OTA_SIGN_PUBKEY, sizeof(OTA_SIGN_PUBKEY));
    if (rc == 0) rc = mbedtls_ecp_check_pubkey(&grp, &Q);
    if (rc == 0) rc = mbedtls_ecdsa_verify(&grp, hash, sizeof(hash), &Q, sig, sig_len);
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);

    if (rc != 0) {
        ESP_LOGE(TAG, "signature check failed (mbedtls %d)", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "image signature OK (%u bytes)", (unsigned)*image_len);
    return ESP_OK;
}
