#include "web_config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签，用于 ESP_LOG 输出
static const char *TAG = "WEB_SERVER";

// 引用 CMake 打包进来的外部 HTML 文件变量 (系统会自动根据文件名生成这些符号)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// 【新增】：后端校验，检查字符串是否全为数字
// 参数：str - 要检查的字符串
// 返回：true 如果字符串全为数字，否则 false
static bool is_numeric_str(const char *str) {
    if (str == NULL || *str == '\0') return false;
    while (*str) {
        if (!isdigit((unsigned char)*str)) return false;
        str++;
    }
    return true;
}

// URL 解码函数，将 URL 编码的字符串解码为原始字符串
// 参数：dst - 解码后的输出缓冲区，src - 要解码的输入字符串
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit((int)a) && isxdigit((int)b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10); else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16 * a + b; 
            src += 3;
        } else if (*src == '+') { 
            *dst++ = ' '; src++; 
        } else { 
            *dst++ = *src++; 
        }
    }
    *dst++ = '\0';
}

// GET请求：直接发送分离出来的 index.html
// 处理根路径的 GET 请求，返回嵌入的 HTML 页面
static esp_err_t root_get_handler(httpd_req_t *req) {
    // 动态计算嵌入文件的大小并发送
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

// POST请求：处理表单提交
// 处理 /save 路径的 POST 请求，解析表单数据，验证输入，保存到 NVS，并重启设备
static esp_err_t save_post_handler(httpd_req_t *req) {
    char buf[1024] = {0}; 
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        ESP_LOGE(TAG, "表单数据太长，拒绝接收！");
        return ESP_FAIL;
    }
    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char value[128], decoded_uri[128], decoded_prodid[128], decoded_devid[128], decoded_pwd[128];
    
    memset(decoded_prodid, 0, sizeof(decoded_prodid));
    memset(decoded_devid, 0, sizeof(decoded_devid));

    if (httpd_query_key_value(buf, "uri", value, sizeof(value)) == ESP_OK) { url_decode(decoded_uri, value); }
    if (httpd_query_key_value(buf, "prodid", value, sizeof(value)) == ESP_OK) { url_decode(decoded_prodid, value); }
    if (httpd_query_key_value(buf, "devid", value, sizeof(value)) == ESP_OK) { url_decode(decoded_devid, value); }
    if (httpd_query_key_value(buf, "mqpass", value, sizeof(value)) == ESP_OK) { url_decode(decoded_pwd, value); }

    // 【后端拦截】
    if (!is_numeric_str(decoded_prodid) || !is_numeric_str(decoded_devid)) {
        ESP_LOGE(TAG, "安全拦截：产品ID或设备ID包含非数字字符！");
        const char* err_page = "<h2 style='color:red;text-align:center;font-family:Arial;margin-top:50px;'>❌ 错误：产品ID和设备ID必须是纯数字！<br><br><a href='/'>返回重试</a></h2>";
        
        // 👇【新增】：强制声明使用 UTF-8 编码
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, err_page, HTTPD_RESP_USE_STRLEN);
        return ESP_OK; 
    }

    // 保存到 NVS
    nvs_handle_t my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    nvs_set_str(my_handle, "mqtt_uri", decoded_uri);
    nvs_set_str(my_handle, "mqtt_prodid", decoded_prodid);
    nvs_set_str(my_handle, "mqtt_devid", decoded_devid);
    nvs_set_str(my_handle, "mqtt_pwd", decoded_pwd);
    nvs_commit(my_handle);
    nvs_close(my_handle);

    ESP_LOGI(TAG, "MQTT 参数已更新！正在重启以应用新配置...");
    
    // 👇【新增】：强制声明使用 UTF-8 编码，彻底解决乱码！
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, "<h2 style='color:#28a745;text-align:center;font-family:Arial;margin-top:50px;'>✅ 配置已保存！设备正在连接云端...</h2>", HTTPD_RESP_USE_STRLEN);
    
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart(); 
    return ESP_OK;
}

// 启动 MQTT Web 配置服务器
// 初始化 HTTP 服务器，注册 GET 和 POST 处理器，并启动服务
void start_mqtt_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_len = 2048;
    config.max_req_hdr_len = 2048;
    config.stack_size = 8192; 

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get);
        
        httpd_uri_t uri_post = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post);
        
        ESP_LOGI(TAG, "===============================================");
        ESP_LOGI(TAG, "🖥️ 后台 Web 配置服务已启动！");
        ESP_LOGI(TAG, "👉 请在浏览器中输入设备 IP 来修改 MQTT 配置");
        ESP_LOGI(TAG, "===============================================");
    }
}