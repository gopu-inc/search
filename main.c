#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_RESULTS 10

typedef struct {
    char title[512];
    char url[1024];
    char snippet[1024];
} SearchResult;

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    fwrite(ptr, 1, total, (FILE*)userdata);
    return total;
}

static void escape_json(const char *src, char *dest, size_t dest_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dest_size; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            dest[j++] = '\\';
            dest[j++] = src[i];
        } else if (src[i] == '\n' || src[i] == '\r') {
            dest[j++] = ' ';
        } else {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

static void extract_results(const char *html, SearchResult *results, int *count) {
    *count = 0;
    const char *ptr = html;
    
    while (*count < MAX_RESULTS) {
        ptr = strstr(ptr, "b_algo");
        if (!ptr) break;
        
        const char *h2 = strstr(ptr, "<h2");
        if (!h2) { ptr++; continue; }
        
        const char *a_start = strstr(h2, "<a");
        if (!a_start) { ptr++; continue; }
        
        const char *href = strstr(a_start, "href=\"");
        if (!href) { ptr++; continue; }
        href += 6;
        
        const char *href_end = strchr(href, '"');
        if (!href_end) { ptr++; continue; }
        
        size_t url_len = href_end - href;
        if (url_len >= 1024) url_len = 1023;
        strncpy(results[*count].url, href, url_len);
        results[*count].url[url_len] = '\0';
        
        const char *a_text_start = strchr(a_start, '>');
        if (!a_text_start) { ptr++; continue; }
        a_text_start++;
        
        const char *a_text_end = strchr(a_text_start, '<');
        if (!a_text_end) { ptr++; continue; }
        
        size_t title_len = a_text_end - a_text_start;
        if (title_len >= 512) title_len = 511;
        strncpy(results[*count].title, a_text_start, title_len);
        results[*count].title[title_len] = '\0';
        
        const char *p_start = strstr(a_text_end, "<p");
        if (p_start) {
            const char *p_text_start = strchr(p_start, '>');
            if (p_text_start) {
                p_text_start++;
                const char *p_text_end = strchr(p_text_start, '<');
                if (p_text_end) {
                    size_t snip_len = p_text_end - p_text_start;
                    if (snip_len >= 1024) snip_len = 1023;
                    strncpy(results[*count].snippet, p_text_start, snip_len);
                    results[*count].snippet[snip_len] = '\0';
                }
            }
        }
        
        (*count)++;
        ptr = a_text_end;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <query>\n", argv[0]);
        return 1;
    }

    char query[256];
    snprintf(query, sizeof(query), "%s", argv[1]);

    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    char *encoded = curl_easy_escape(curl, query, 0);
    char url[512];
    snprintf(url, sizeof(url), "https://www.bing.com/search?q=%s", encoded);

    FILE *fp = fopen("result.html", "w");
    if (!fp) {
        curl_easy_cleanup(curl);
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fclose(fp);
        curl_free(encoded);
        curl_easy_cleanup(curl);
        return 1;
    }
    
    fclose(fp);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    FILE *rfp = fopen("result.html", "r");
    if (!rfp) return 1;
    
    fseek(rfp, 0, SEEK_END);
    long fsize = ftell(rfp);
    fseek(rfp, 0, SEEK_SET);
    
    char *html = malloc(fsize + 1);
    if (!html) { fclose(rfp); return 1; }
    
    fread(html, 1, fsize, rfp);
    html[fsize] = '\0';
    fclose(rfp);
    remove("result.html");

    SearchResult results[MAX_RESULTS];
    int count = 0;
    extract_results(html, results, &count);
    
    printf("{\n  \"query\": \"%s\",\n  \"count\": %d,\n  \"results\": [\n", query, count);
    for (int i = 0; i < count; i++) {
        char esc_title[1024], esc_url[2048], esc_snip[2048];
        escape_json(results[i].title, esc_title, sizeof(esc_title));
        escape_json(results[i].url, esc_url, sizeof(esc_url));
        escape_json(results[i].snippet, esc_snip, sizeof(esc_snip));

        printf("    {\n");
        printf("      \"title\": \"%s\",\n", esc_title);
        printf("      \"url\": \"%s\",\n", esc_url);
        printf("      \"snippet\": \"%s\"\n", esc_snip);
        printf("    }%s\n", (i < count - 1) ? "," : "");
    }
    printf("  ]\n}\n");
    
    free(html);
    return 0;
}
