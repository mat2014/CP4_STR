#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

// --- DADOS DO ALUNO ---
#define STUDENT_INFO "{Matheus-RM:83129}"


#define QUEUE_SIZE 5
#define RECEIVE_TIMEOUT_MS 2000
#define SUPERVISOR_INTERVAL_MS 3000
#define WATCHDOG_TIMEOUT_S 4
#define GENERATOR_INTERVAL_MS 1000

static const char *TAG = "SISTEMA_CONCORRENTE";

QueueHandle_t data_queue;

volatile bool g_generator_ok = true;
volatile bool g_receiver_ok = true;

void generator_task(void *pvParameters);
void receiver_task(void *pvParameters);
void supervisor_task(void *pvParameters);

void app_main(void)
{
    
    ESP_LOGI(TAG, STUDENT_INFO " Iniciando sistema...");

    data_queue = xQueueCreate(QUEUE_SIZE, sizeof(int));

    xTaskCreate(generator_task, "Generator", 2048, NULL, 5, NULL);
    xTaskCreate(receiver_task, "Receiver", 2048, NULL, 5, NULL);
    xTaskCreate(supervisor_task, "Supervisor", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, STUDENT_INFO " Tasks em execução.");
}

void generator_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    int value_to_send = 0;
    while (1) {
        esp_task_wdt_reset();
        
        if (xQueueSend(data_queue, &value_to_send, (TickType_t)0) == pdPASS) {
            ESP_LOGI(TAG, STUDENT_INFO " [GERADOR] Enviando valor: %d", value_to_send);
        } else {
            ESP_LOGW(TAG, STUDENT_INFO " [GERADOR] AVISO: Fila cheia. Valor %d descartado.", value_to_send);
        }
        
        value_to_send++;
        g_generator_ok = true;
        
        vTaskDelay(pdMS_TO_TICKS(GENERATOR_INTERVAL_MS));
    }
}

void receiver_task(void *pvParameters) {
    esp_task_wdt_add(NULL);
    int received_value;
    int timeout_count = 0;
    while (1) {
        esp_task_wdt_reset();
        
        if (xQueueReceive(data_queue, &received_value, pdMS_TO_TICKS(RECEIVE_TIMEOUT_MS)) == pdPASS) {
            timeout_count = 0;
            g_receiver_ok = true;

            int *temp_storage = (int*) malloc(sizeof(int));
            if (temp_storage != NULL) {
                *temp_storage = received_value;
                ESP_LOGI(TAG, STUDENT_INFO " [RECEPTOR] Valor recebido: %d", *temp_storage);
                free(temp_storage);
            } else {
                ESP_LOGE(TAG, STUDENT_INFO " [RECEPTOR] ERRO: Falha ao alocar memória!");
                g_receiver_ok = false;
            }
        } else {
            timeout_count++;
            g_receiver_ok = false;
            if (timeout_count == 1) {
                ESP_LOGW(TAG, STUDENT_INFO " [RECEPTOR] AVISO: Timeout 1 - Nenhum dado.");
            } else if (timeout_count == 2) {
                ESP_LOGE(TAG, STUDENT_INFO " [RECEPTOR] ERRO: Timeout 2 - Sistema pode estar lento.");
            } else {
                ESP_LOGE(TAG, STUDENT_INFO " [RECEPTOR] ERRO FATAL: Timeout 3 - O gerador pode ter travado.");
            }
        }
    }
}

void supervisor_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_INTERVAL_MS));
        
        const char* gen_status_str = g_generator_ok ? "OK" : "FALHA";
        const char* recv_status_str = g_receiver_ok ? "OK" : "AVISO";

        
        printf("\n"); 
        printf("--- [SUPERVISOR] Status do Sistema | %s ---\n", STUDENT_INFO);
        printf("    Módulo Gerador:      %s\n", gen_status_str);
        printf("    Módulo Receptor:     %s\n", recv_status_str);
        printf("    Memória Heap Livre:  %ld bytes\n", esp_get_free_heap_size());
        printf("----------------------------------------------------------------------\n\n");
    }
}