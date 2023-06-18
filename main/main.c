#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_system.h"
#include "esp_system.h"
#include "esp_log.h"

#include "cJSON.h"
#include "driver/uart.h"

#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_tls.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"
#include "esp_http_client.h"

#include "cJSON.h"

#include "esp_timer.h"
#include <inttypes.h>

#include "driver/gpio.h"
#include <sys/param.h>
#include <esp_http_server.h>
#include <stdbool.h>
#include "esp_sntp.h"

#include "nvs.h"

#include <driver/i2c.h>
#include <esp_log.h>
#include "HD44780.h"
#include "dht11.h"
#define LCD_ADDR 0x27
#define SDA_PIN  21
#define SCL_PIN  22
#define LCD_COLS 16
#define LCD_ROWS 2
unsigned long LastSendMQTT = 0; 


void LCDBegin(void);
void HienThiLCD(void);


void delay(uint32_t time);
cJSON *str_jsonMQTT,*str_jsonLora;
void ConnectWiFi(void);	
static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);



//====================WIFI===================================================================
#define EXAMPLE_ESP_WIFI_SSID      "GalaxyA51"
#define EXAMPLE_ESP_WIFI_PASS      "11223344"
#define EXAMPLE_ESP_MAXIMUM_RETRY  500
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static const char *TAG1 = "wifi station";
static EventGroupHandle_t s_wifi_event_group;
char str_ip[16];
void ConnectWiFi(void)
{
	s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,&instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler,NULL,&instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
          
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG1, "wifi_init_sta finished.");

  
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    
    if (bits & WIFI_CONNECTED_BIT) 
	{
        ESP_LOGI(TAG1, "connected to ap SSID:%s password:%s",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } 
	else if (bits & WIFI_FAIL_BIT) 
	{
        ESP_LOGI(TAG1, "Failed to connect to SSID:%s, password:%s",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
	else 
	{
        ESP_LOGE(TAG1, "UNEXPECTED EVENT");
    }


    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
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
            ESP_LOGI(TAG1, "retry to connect to the AP");
        } 
		else 
		{
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
       
    } 
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG1, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
		
		esp_ip4addr_ntoa(&event->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);


		printf("I have a connection and my IP is %s!", str_ip);

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

//====================MQTT===================================================================
bool InsConnectMQtt = false;
void ConnectMQTTSendData(void *arg);
void MQTT_DataJson(void);

void JsonParseMQTT(char *buf);
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static const char *TAG2 = "MQTT_EXAMPLE";

int  ND = 0;
int	 DA = 0;

#define MQTT_sizeoff 200

char MQTT_JSON[MQTT_sizeoff];
char Str_ND[MQTT_sizeoff];
char Str_DA[MQTT_sizeoff] ;

char *topic_pub = "nhatquang3102/A";
char *topic_sub = "nhatquang3102/B";
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG2, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, topic_sub, 0);
            ESP_LOGI(TAG2, "sent subscribe successful, msg_id=%d", msg_id);
			InsConnectMQtt = true;

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG2, "MQTT_EVENT_DISCONNECTED");
			InsConnectMQtt = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG2, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG2, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            
			printf("Messager MQTT: %.*s\r\n",event->data_len,event->data);
			
			char *bufData = calloc(event->data_len + 1, sizeof(char));
			
			snprintf(bufData, event->data_len + 1,  "%s", event->data);
			
			printf("Data MQTT:%s\n",bufData);
			
			LastSendMQTT = esp_timer_get_time()/1000;
			taskYIELD();
						
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG2, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG2, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG2, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    mqtt_event_handler_cb(event_data);
}


void app_main(void)
{
	
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
	delay(500);
    DHT11_init(GPIO_NUM_4);
	delay(10);
	LCDBegin();
	ConnectWiFi();
	delay(2000);
	
	xTaskCreate(ConnectMQTTSendData, "Task1", 2048, NULL, 5, NULL);
	
}




void ConnectMQTTSendData(void *arg)
{	
	 const esp_mqtt_client_config_t mqtt_cfg = {
		
        .broker.address.uri = "mqtt://nhatquang3102:4CFDA840F1F84281@ngoinhaiot.com:1111",
    };
	
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
	
	
	LastSendMQTT = esp_timer_get_time()/1000;
	
	
	while(1)
	{
		if(esp_timer_get_time()/1000 - LastSendMQTT >= 1500)
		{
			if(InsConnectMQtt)
			{
			
				HienThiLCD();
				delay(50);
			
				MQTT_DataJson();
				esp_mqtt_client_publish(client, topic_pub, MQTT_JSON, 0, 1, 0);
				ESP_LOGI(TAG2, "SEND MQTT %s", MQTT_JSON);
				delay(10000);		
				taskYIELD();
			}
			
			LastSendMQTT = esp_timer_get_time()/1000;
		}
		
		delay(50);
	}
	vTaskDelete(NULL);
}
void MQTT_DataJson(void)
{
	for(int i = 0 ; i < MQTT_sizeoff ; i++)
	{
		
		MQTT_JSON[i] = 0;	
		Str_ND[i] = 0;
		Str_DA[i] = 0;
	}

	sprintf(Str_ND, "%d", DHT11_read().temperature);
	sprintf(Str_DA, "%d", DHT11_read().humidity);

	strcat(MQTT_JSON,"{\"ND\":\"");
	strcat(MQTT_JSON,Str_ND);
	strcat(MQTT_JSON,"\",");
	
	
	strcat(MQTT_JSON,"\"DA\":\"");
	strcat(MQTT_JSON,Str_DA);
	strcat(MQTT_JSON,"\"}");
}

void LCDBegin(void)
{
	LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);
	delay(100);
	LCD_home();
    LCD_clearScreen();
	LCD_setCursor(0, 0);
    LCD_writeStr("--16x2 LCD--");
    LCD_setCursor(0, 1);
    LCD_writeStr("LCD Demo");
	delay(1000);
}
void HienThiLCD(void)
{
	char str[20] = {0};
	sprintf (str, "Nhiet do:%d",DHT11_read().temperature);
	char str1[20] = {0};
	sprintf (str1, "Do am:%d",DHT11_read().humidity);

	LCD_clearScreen();
	LCD_setCursor(0, 0);
    LCD_writeStr(str);
	delay(100);
    LCD_setCursor(0, 1);
    LCD_writeStr(str1);
}
void delay(uint32_t time)
{
	vTaskDelay(time / portTICK_PERIOD_MS);
}





