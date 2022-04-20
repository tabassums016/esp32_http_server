/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"

// HTTP Includes

#include "esp_log.h"
#include "esp_http_server.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID "Tabz"
#define EXAMPLE_ESP_WIFI_PASS "tabassum"
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

int8_t RSSI = 0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG2 = "wifi station";
// static const char *TAG2 = "HTTP";
static const char *TAG = "SERVER";
static int s_retry_num = 0;

char query_default[50] = "0";
int querycount_int = 0;
char *query_nvs;
int8_t query_nvs_count;
char query_array[50][60];
char get_query[500];

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG2, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG2, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG2, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG2, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG2, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG2, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG2, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
char dataString[200];
void substr(const char *src1, int m, int n)
{
    // get the length of the destination string
    int len = n - m;

    // allocate (len + 1) chars for destination (+1 for extra null character)
    char *dest = (char *)malloc(sizeof(char) * (len + 1));

    // extracts characters between m'th and n'th index from source string
    // and copy them into the destination string
    for (int i = m; i < n && (*(src1 + i) != '\0'); i++)
    {
        *dest = *(src1 + i);
        dest++;
    }

    // null-terminate the destination string
    *dest = '\0';

    // return the destination string
    strcpy(dataString, dest - len);
    // 	return dest - len;
}

// MODBUS="1,3,0,104,0,50,0,0"

void query_parser(char query[])
{
    int index = 0;
    int slave_id = 1;
    char slave_data[5];
    char function_data[5];
    char address_dataH[5];
    char address_dataL[5];
    char register_dataH[5];
    char register_dataL[5];
    char data_mode[5];
    char q_count[5];

    memset(slave_data, 0, sizeof(slave_data));
    memset(function_data, 0, sizeof(function_data));
    memset(address_dataH, 0, sizeof(address_dataH));
    memset(address_dataL, 0, sizeof(address_dataL));
    memset(register_dataH, 0, sizeof(register_dataH));
    memset(register_dataL, 0, sizeof(register_dataL));
    memset(data_mode, 0, sizeof(data_mode));
    memset(q_count, 0, sizeof(q_count));

    while (query[slave_id] != ',')
    {
        slave_data[index] = query[slave_id];
        slave_id++;
        index++;
    }
    int function_code = slave_id + 1;
    slave_id = 0;
    index = 0;

    while (query[function_code] != ',')
    {
        function_data[index] = query[function_code];
        function_code++;
        index++;
    }
    int addressH = function_code + 1;
    function_code = 0;
    index = 0;

    while (query[addressH] != ',')
    {
        address_dataH[index] = query[addressH];
        addressH++;
        index++;
    }
    int addressL = addressH + 1;
    addressH = 0;
    index = 0;

    while (query[addressL] != ',')
    {
        address_dataL[index] = query[addressL];
        addressL++;
        index++;
    }
    int registerH = addressL + 1;
    addressL = 0;
    index = 0;

    while (query[registerH] != ',')
    {
        register_dataH[index] = query[registerH];
        registerH++;
        index++;
    }
    int registerL = registerH + 1;
    registerH = 0;
    index = 0;

    while (query[registerL] != ',')
    {
        register_dataL[index] = query[registerL];
        registerL++;
        index++;
    }
    int datatype = registerL + 1;
    registerL = 0;
    index = 0;

    while (query[datatype] != ',')
    {
        data_mode[index] = query[datatype];
        datatype++;
        index++;
    }
    int querycount = datatype + 1;
    datatype = 0;
    index = 0;

    while (query[querycount] != '}')
    {
        q_count[index] = query[querycount];
        querycount++;
        index++;
    }
    querycount = 0;
    index = 0;

    // slave_int = atoi(slave_data);
    // function_int = atoi(function_data);
    // addressH_int = atoi(address_dataH);
    // addressL_int = atoi(address_dataL);
    // registerH_int = atoi(register_dataH);
    // registerL_int = atoi(register_dataL);
    // datatype_int = atoi(data_mode);
    querycount_int = atoi(q_count);

    // printf("Int value of Slave ID %d\n", slave_int);
    // printf("Int value of Function Code %d\n", function_int);
    // printf("Int value of Address Location %d%d\n", addressH_int,addressL_int);
    // printf("Int value of Register Count %d%d\n", registerH_int,registerL_int);
    // printf("Int value of Data Mode %d\n", datatype_int);
    // printf("Int value of Query Count %d\n", querycount_int);

    // query_data[querycount_int][0] = slave_int;
    // query_data[querycount_int][1] = function_int;
    // query_data[querycount_int][2] = addressH_int;
    // query_data[querycount_int][3] = addressL_int;
    // query_data[querycount_int][4] = registerH_int;
    // query_data[querycount_int][5] = registerL_int;

    // query_datatype[querycount_int] = datatype_int;

    // Add_CRC(query_data[querycount_int]);
}

void cmd_parser(const char *data)
{
    int len1 = strcspn(data, "=");
    int len2;
    char src[200], check_cmd[20];
    substr(data, 0, len1);
    strcpy(check_cmd, dataString);

    if (strcmp(check_cmd, "MODBUS") == 0)
    {
        printf("%s\n", data);
        memset(query_default, 0, sizeof(query_default));
        int i = 8;
        int index = 0;
        while (data[i] != '\"')
        {
            query_default[index] = data[i];
            i++;
            index++;
        }
        i = 8;
        index = 0;

        query_parser(query_default);
        printf("RECEIVED QUERY : %s\n", query_default);

        if (querycount_int >= 0 && querycount_int <= 50)
        {
            char nvs_tag[10];
            memset(nvs_tag, 0, sizeof(nvs_tag));
            sprintf(nvs_tag, "Query%d", querycount_int);

            esp_err_t err;
            nvs_handle my_handle;
            nvs_open("COUNT", NVS_READWRITE, &my_handle);
            // Read
            err = nvs_set_i8(my_handle, "QueryCount", querycount_int);
            printf((err != ESP_OK) ? "Query Count Failed!\n" : "Query Count Done\n");
            nvs_commit(my_handle);
            nvs_close(my_handle);

            nvs_open("QUERY", NVS_READWRITE, &my_handle);
            // Read
            err = nvs_set_str(my_handle, nvs_tag, (char *)query_default);
            printf((err != ESP_OK) ? "Query Failed!\n" : "Query Done\n");
            nvs_commit(my_handle);
            nvs_close(my_handle);
        }
    }

    else
    {
        ESP_LOGE("NVS", "Wrong Command\n");
    }
}

void non_volatile_mem_int()
{
    char htmlResponse[35000];
    char *html = htmlResponse;

    esp_err_t err;
    // Open
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... \n");
    nvs_handle my_handle;

    // Query Count Page
    err = nvs_open("COUNT", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Error (%s) opening COUNT\n", esp_err_to_name(err));
    }
    else
    {
        printf("Opened COUNT Page\n");

        int8_t query_count = 0;

        err = nvs_get_i8(my_handle, "QueryCount", &query_count);

        if (err != ESP_OK)
        {
            query_count = -1; //++ Default Query Count
            ESP_LOGI(TAG, "Error (%s) opening Query Count\n", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "Count = %d\n", query_count);
        }
        query_nvs_count = query_count; //++ Query Count Saved in NVS
    }

    // Query Page
    err = nvs_open("QUERY", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Error (%s) opening QUERY\n", esp_err_to_name(err));
    }
    else
    {

        printf("Opened QUERY Page\n");

        for (int i = 0; i <= query_nvs_count; i++)
        {
            char nvs_tag[20];
            memset(nvs_tag, 0, sizeof(nvs_tag));
            sprintf(nvs_tag, "Query%d", i);

            size_t required_size;
            err = nvs_get_str(my_handle, nvs_tag, NULL, &required_size);
            query_nvs = malloc(required_size);
            err = nvs_get_str(my_handle, nvs_tag, query_nvs, &required_size);

            if (err != ESP_OK)
            {
                query_nvs = "{0,0,0,0,0,0,0,0}";
                ESP_LOGI(TAG, "Error (%s) opening Query%d\n", esp_err_to_name(err), query_nvs_count);
            }
            else
            {
                ESP_LOGI(TAG, "Query %d = %s\n", i, query_nvs);
                strcpy(query_array[i], query_nvs);
                // strcpy(get_query[i], (query_nvs);
                // query_parser(query_array[i]);
                //   char htmlResponse[35000];
                //  char *html = htmlResponse;
                // char htmlResponse[35000];
                // char *html = htmlResponse;
                // html += sprintf(html, "%s", query_nvs);
                // printf("\n %s", htmlResponse);
                printf(query_array[i]);
                // char current_query[40];
                // sprintf(current_query, "\"%s\"",query_array);
                // get_query[i]=current_query;
            }
        }
    }

    nvs_close(my_handle);
    // char buffer[1000];
    // memcpy(buffer,get_query,strlen(get_query));
    //  printf("\n%s",buffer);

    printf("\n");
}

void create_json()
{
    // char htmlResponse[strlen(query_array)];
    // char *html = htmlResponse;
    // html += sprintf(html, "{%s", query_array[0]);
    //json_object *jarray = json_object_new_array();
//     for (int i = 0; i <=query_nvs_count; i++)
//     {
//         // html+=sprintf(html, ",%s", query_array[i]);
//        // printf("\n%s", query_array[i]);
//        json_object_array_add(jarray,jstring1);
// //   json_object_array_add(jarray,jstring2);
// //   json_object_array_add(jarray,jstring3);
//     }
    // //   html+=sprintf(html, "%s", "}");
    // printf("\n %s", htmlResponse);

//     jwOpen( buffer, buflen, JW_OBJECT, JW_PRETTY );  // open root node as object
// jwObj_string( "key", "value" );                  // writes "key":"value"
// jwObj_int( "int", 1 );                           // writes "int":1
// jwObj_array( "anArray");                         // start "anArray": [...] 
//     jwArr_int( 0 );                              // add a few integers to the array
//     jwArr_int( 1 );
//     jwArr_int( 2 );
// jwEnd();                                         // end the array
// err= jwClose();

// json_object *jarray = json_object_new_array();
char* out;

         cJSON *root,*car,*query;
        // query = cJSON_CreateNumber(y);

    //root  = cJSON_CreateObject();
    car=  cJSON_CreateArray();
    for (int i = 0; i <=query_nvs_count; i++)
     {
         char id = i;
    //cJSON_AddItemToObject(root, id, cJSON_CreateString(query_array[i]));
    query = cJSON_CreateRaw(query_array[i]);
    cJSON_AddItemToArray(car, query);
     }
    //cJSON_AddItemToObject(root, "carID", cJSON_CreateString("bmw123"));
    // cJSON_AddItemToArray(car, root);

    out = cJSON_Print(car);
    printf("GET REQ= %s\n",out);
    sprintf(get_query,"%s",out);



}

// ESP HTTP CODE functions.....

static esp_err_t on_get_url_handler(httpd_req_t *req)
{
    non_volatile_mem_int();
    create_json();
    ESP_LOGI(TAG, "URL: %s", req->uri);
    httpd_resp_send(req, get_query, strlen(get_query));
    return ESP_OK;
}

static esp_err_t on_post_url_handler(httpd_req_t *req)
{
    char buffer[200];
    memset(&buffer, 0, sizeof(buffer));
    ESP_LOGI(TAG, "URL: %s", req->uri);
    httpd_req_recv(req, buffer, req->content_len);
    ESP_LOGI(TAG, "%s", buffer);
    cmd_parser(buffer);
    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

static void init_server()
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t get_url = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = on_get_url_handler};

    httpd_uri_t post_url = {
        .uri = "/configmodbus",
        .method = HTTP_POST,
        .handler = on_post_url_handler};

    httpd_register_uri_handler(server, &get_url);
    httpd_register_uri_handler(server, &post_url);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG2, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);
    RSSI = ap.rssi;
    printf("WIFI rssi = %d\n", RSSI);
    init_server();
    non_volatile_mem_int();
     //create_json();
}
