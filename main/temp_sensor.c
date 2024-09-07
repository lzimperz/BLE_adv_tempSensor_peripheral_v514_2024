#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_sleep.h"
#include "esp_random.h"

#include "temp_sensor.h"

#define SENSOR_LOG_TAG "SENSOR_SIM"

/************************************************************************/
/* El dispositivo emite Advertisings BLE durante 5 segundos y duerme 30 */
/*                                                                      */
/* La temperatura, en vez de ser un número random, lo que hace          */
/* sumar un delta hacia arriba o hacia abajo, según un random           */
/* Por el motivo anterior, necesito que la variable "temp" perdure      */
/* tras los reinicios por software y esto se logra guardandola en la    */
/* memoria del reloj de tiempo real RTC usando el atuributo             */
/* RTC_DATA_ATTR.                                                       */
/*                                                                      */
/* Situacion similar para la variable "restart_counter"                 */
/************************************************************************/
static uint32_t RTC_DATA_ATTR restart_counter = 0;

// Espacio de memoria para alojar la propiedad "float *temp;" del objeto.
float RTC_DATA_ATTR temp;

// Espacio de memoria para alojar la propiedad "unsigned char* temp_string;" del objeto.
unsigned char temp_string[10];

/************************************************************************/
/* Convierte la temperatura almacenada en float, a cadena de caracteres */
/* Formatea la cadena de texto para que se envie siempre la misma       */
/* cantidad de caracteres, con formato "TMP##.#"                        */
/************************************************************************/
static void convert_temp_to_string(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a convert_temp_to_string().");
    if (temp >= 100)
    {
        ESP_LOGE(SENSOR_LOG_TAG, "Temperatura supera limite superior.");
        temp = 99.9;
    }
    if (temp <= 0)
    {
        ESP_LOGE(SENSOR_LOG_TAG, "Temperatura supera limite inferior.");
        temp = 0;
    }
    // Convierta a cadena de texto con formato de 4 digitos, 1 posicion decimal.
    snprintf((char *)temp_string, sizeof(temp_string), "TMP%04.1f", temp);
}

/************************************************************************/
/* Simula el sensor de temperatura, generando un desvio positivo o      */
/* negativo en base al resultado de un random.                          */
/* Lo implementé de este modo para que se vaya formando una curva y no  */
/* puntos inconexos                                                     */
/************************************************************************/
static void sample_temp(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a sample_temp().");

    // Tomo la muestra del supuesto sensor, en este ejemplo solo genero un random.
    ESP_LOGI(SENSOR_LOG_TAG, "Tomando muestra... ");
    uint32_t random_number = esp_random();
    if (random_number > 2147483648)
        temp += 0.3;
    else
        temp -= 0.3;

    if (temp > 40)
        temp = 39.5;
    if (temp < 1)
        temp = 1.5;
    convert_temp_to_string();
}

/************************************************************************/
/* Implementa una funcion de TAREA que se ejecutra en el SO cada vez    */
/* que se la lanza, por unica vez (sin bucle infinito)                  */
/*                                                                      */
/* Lleva al dispositivo a estado de DEEP SLEEP (bajo consuom) tras los  */
/* segundos especificados como parámetro y lo configura para que se     */
/* reinicie tras 30 segundos.                                           */
/*                                                                      */
/* Recibe un uint8_t como parametro tipo "void" y luego lo castea al    */
/* tipo correcto. Las funciones que implementan tareas, reciben         */
/* argumentos de esta manera.                                           */
/************************************************************************/
void go_sleep_task(void *param)
{
    uint8_t seconds = *((uint8_t *)param);
    ESP_LOGI(SENSOR_LOG_TAG, "Inicia go_sleep_task().");
    ESP_LOGI(SENSOR_LOG_TAG, "Segundos para ir a sleep: %d", seconds);
    vTaskDelay(seconds * 1000 / portTICK_PERIOD_MS);

    esp_bt_controller_disable();
    esp_sleep_enable_timer_wakeup(30 * 1000000);
    ESP_LOGI(SENSOR_LOG_TAG, "Sleep!");
    esp_deep_sleep_start();
    vTaskDelete(NULL);
}

/************************************************************************/
/* Dispara la tarea especificada en xTaskCreate                         */
/*                                                                      */
/* Para disparar una tarea, como primer parametro de xTaskCreate le     */
/* pasamos el puntero a la funcion (su "nombre")                        */
/*                                                                      */
/* Se llama en el manejador de eventos BLE, cuando inicia el ADV        */
/************************************************************************/
static void go_sleep(uint8_t seconds)
{
    // uint8_t *seconds_pointer = &seconds;
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a go_sleep().");
    xTaskCreate(go_sleep_task, "go_sleep_task", 4096 * 1, (void *)(&seconds), 3, NULL);
}

/************************************************************************/
/* Inicializa el "objeto"                                               */
/* Lo implementado dentro del IF, se reinicia unicamente cuando el      */
/* dispositivo es energizado, luego, restart_counter será mayor a 0.    */
/* Esto es útil cuando tenemos que inicializar algo en el dispositivo   */
/* por única vez cuando enciende.                                       */
/************************************************************************/
static void initialize(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a initialize().");
    if (restart_counter == 0)
    {
        ESP_LOGI(SENSOR_LOG_TAG, "Inicializa temperatura a 24.");
        temp = 24;
    }
    ESP_LOGI(SENSOR_LOG_TAG, "Reinicio numero: %lu", restart_counter);
    restart_counter++;
}

/*****************************************************
 *   Driver Instance Declaration(s) API(s)
 ******************************************************/
const tempSensor_t tempSensor = {
    .temp_string = temp_string,
    .temp = &temp,
    .initialize = initialize,
    .sample_temp = sample_temp,
    .go_sleep = go_sleep};
