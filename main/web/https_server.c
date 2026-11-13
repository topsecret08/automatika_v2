#include "https_server.h"
#include "config.h"
#include "core/flash_fs.h"

#include "esp_https_server.h"
#include "esp_log.h"

// mbedtls: генерация ключа и самоподписанного сертификата.
// Все нужные функции (X509_CREATE, X509_CRT_WRITE, PK_WRITE, ECP, CTR_DRBG)
// уже включены в esp_config.h ESP-IDF 5.3, отдельный sdkconfig не нужен.
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "HTTPS";

// PEM-буферы должны жить всё время работы сервера: esp_https_server
// передаёт указатели в mbedtls_ssl_config без копирования.
static char *s_cert = NULL;
static char *s_key = NULL;
static httpd_handle_t s_srv = NULL;

// ------------------------------------------------------------------
// Чтение/запись PEM в littlefs
// ------------------------------------------------------------------
static char *load_pem(const char *path, size_t max_len, const char *marker) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    if (st.st_size < 32 || (size_t)st.st_size >= max_len) {
        ESP_LOGW(TAG, "%s has unexpected size %ld, regenerating", path, (long)st.st_size);
        return NULL;
    }

    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) { free(buf); return NULL; }

    size_t rd = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    buf[rd] = '\0';

    if (strstr(buf, marker) == NULL) {
        ESP_LOGW(TAG, "%s is not a valid PEM (%s), regenerating", path, marker);
        free(buf);
        return NULL;
    }
    return buf;
}

static bool save_pem(const char *path, const char *pem) {
    if (!flash_fs_ready()) return false;

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGW(TAG, "cannot write %s", path);
        return false;
    }
    size_t len = strlen(pem);
    size_t wr = fwrite(pem, 1, len, f);
    fclose(f);

    if (wr != len) {
        ESP_LOGW(TAG, "short write to %s", path);
        return false;
    }
    ESP_LOGI(TAG, "saved %s (%u bytes)", path, (unsigned)len);
    return true;
}

// ------------------------------------------------------------------
// Генерация самоподписанного сертификата (EC P-256)
// ------------------------------------------------------------------
static esp_err_t generate_pair(void) {
    mbedtls_pk_context       key;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509write_cert   crt;
    char errbuf[96];

    mbedtls_pk_init(&key);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_x509write_crt_init(&crt);

    esp_err_t result = ESP_FAIL;
    char *cert = calloc(1, WEB_CERT_PEM_MAX);
    char *priv = calloc(1, WEB_KEY_PEM_MAX);
    if (!cert || !priv) {
        ESP_LOGE(TAG, "out of memory for PEM buffers");
        goto cleanup;
    }

    static const char pers[] = "automatika-pwa";
    int ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *)pers, sizeof(pers) - 1);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "ctr_drbg_seed: %s", errbuf);
        goto cleanup;
    }

    // EC P-256: быстрее RSA-2048 на ESP32 и заметно меньше по памяти.
    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "pk_setup: %s", errbuf);
        goto cleanup;
    }
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key),
                              mbedtls_ctr_drbg_random, &drbg);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "ecp_gen_key: %s", errbuf);
        goto cleanup;
    }

    // Серийный номер 0x01: для единственного самоподписанного сертификата
    // уникальности достаточно, а CSR/CA-инфраструктура здесь не нужна.
    // В mbedtls 3.6 используется только set_serial_raw (big-endian байты).
    unsigned char serial[1] = { 0x01 };

    char dn[96];
    snprintf(dn, sizeof(dn), "CN=%s,O=Automatika,C=RU", WEB_CERT_CN);

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    mbedtls_x509write_crt_set_subject_name(&crt, dn);
    mbedtls_x509write_crt_set_issuer_name(&crt, dn);
    mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
    mbedtls_x509write_crt_set_validity(&crt, WEB_CERT_VALID_FROM, WEB_CERT_VALID_TO);
    // CA:TRUE нужен, чтобы сертификат можно было установить как доверенный
    // корень на Android/glass. pathlen=0 — он не подписывает другие сертификаты.
    mbedtls_x509write_crt_set_basic_constraints(&crt, 1, 0);
    mbedtls_x509write_crt_set_key_usage(&crt,
        MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_CERT_SIGN);

    // SAN обязателен современным браузерам. Доступ по IP не покрывается:
    // для установки PWA используйте https://automatika.local.
    // В mbedtls 3.6 API принимает список (SAN list), а не строку "DNS:...".
    mbedtls_x509_san_list san = {0};
    san.node.type = MBEDTLS_X509_SAN_DNS_NAME;
    san.node.san.unstructured_name.p = (unsigned char *)WEB_CERT_CN;
    san.node.san.unstructured_name.len = strlen(WEB_CERT_CN);
    ret = mbedtls_x509write_crt_set_subject_alternative_name(&crt, &san);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "subject_alt_name: %s", errbuf);
        goto cleanup;
    }

    ret = mbedtls_x509write_crt_pem(&crt, (unsigned char *)cert, WEB_CERT_PEM_MAX,
                                    mbedtls_ctr_drbg_random, &drbg);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "crt_pem: %s", errbuf);
        goto cleanup;
    }

    ret = mbedtls_pk_write_key_pem(&key, (unsigned char *)priv, WEB_KEY_PEM_MAX);
    if (ret != 0) {
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        ESP_LOGE(TAG, "key_pem: %s", errbuf);
        goto cleanup;
    }

    s_cert = cert;
    s_key = priv;
    cert = NULL;   // владение перешло к статическим указателям
    priv = NULL;

    // Если littlefs недоступен, HTTPS всё равно поднимется: PEM останутся
    // в RAM до перезагрузки. Просто предупреждаем — сертификат не переживёт
    // рестарт и будет сгенерирован заново.
    if (!save_pem(WEB_CERT_PATH, s_cert) || !save_pem(WEB_KEY_PATH, s_key)) {
        ESP_LOGW(TAG, "certificate not persisted; will be regenerated on boot");
    } else {
        ESP_LOGI(TAG, "self-signed certificate generated for %s", WEB_CERT_CN);
    }

    result = ESP_OK;

cleanup:
    if (cert) free(cert);
    if (priv) free(priv);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_pk_free(&key);
    return result;
}

// ------------------------------------------------------------------
// Запуск
// ------------------------------------------------------------------
esp_err_t https_server_start(httpd_handle_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;

    // 1. Пытаемся переиспользовать сертификат с прошлой загрузки.
    s_cert = load_pem(WEB_CERT_PATH, WEB_CERT_PEM_MAX, "BEGIN CERTIFICATE");
    s_key  = load_pem(WEB_KEY_PATH, WEB_KEY_PEM_MAX, "PRIVATE KEY");
    if (!s_cert || !s_key) {
        if (s_cert) { free(s_cert); s_cert = NULL; }
        if (s_key)  { free(s_key);  s_key  = NULL; }

        // 2. Иначе генерируем новую пару. Это занимает доли секунды
        //    на EC P-256 и не требует внешнего CA.
        if (generate_pair() != ESP_OK) {
            ESP_LOGE(TAG, "cannot generate certificate, HTTPS disabled");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(TAG, "loaded certificate %s", WEB_CERT_PATH);
    }

    // 3. Поднимаем TLS-сервер. Второй (insecure) порт не создаём:
    //    обычный HTTP уже работает в web_server.c на порту 80.
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.servercert      = (const uint8_t *)s_cert;
    cfg.servercert_len  = strlen(s_cert) + 1;
    cfg.prvtkey_pem     = (const uint8_t *)s_key;
    cfg.prvtkey_len     = strlen(s_key) + 1;
    cfg.transport_mode  = HTTPD_SSL_TRANSPORT_SECURE;
    cfg.port_secure     = WEB_HTTPS_PORT;

    // TLS-сессия требует больше стека, чем обычный HTTP.
    cfg.httpd.max_uri_handlers  = 96;
    cfg.httpd.max_open_sockets  = 3;
    cfg.httpd.stack_size        = 10240;
    cfg.httpd.lru_purge_enable  = true;
    cfg.httpd.recv_wait_timeout = 10;
    cfg.httpd.send_wait_timeout = 10;

    esp_err_t err = httpd_ssl_start(&s_srv, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ssl_start failed: %s", esp_err_to_name(err));
        free(s_cert); s_cert = NULL;
        free(s_key);  s_key  = NULL;
        s_srv = NULL;
        return err;
    }

    *out = s_srv;
    ESP_LOGI(TAG, "HTTPS listening on port %d (CN=%s)", WEB_HTTPS_PORT, WEB_CERT_CN);
    return ESP_OK;
}

void https_server_stop(void) {
    if (s_srv) {
        httpd_ssl_stop(s_srv);
        s_srv = NULL;
    }
    if (s_cert) { free(s_cert); s_cert = NULL; }
    if (s_key)  { free(s_key);  s_key  = NULL; }
}

const char *https_server_cert_pem(void) { return s_cert; }
