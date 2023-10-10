#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>

#include "NimBLEDevice.h"
#include "client.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "utils.h"

static BLEUUID serviceUUID("00000211-b2d1-43f0-9b88-960cebf8b91e");
static BLEUUID readUUID("00000213-b2d1-43f0-9b88-960cebf8b91e");
static BLEUUID writeUUID("00000212-b2d1-43f0-9b88-960cebf8b91e");

static BLERemoteCharacteristic *readCharacteristic;
static BLERemoteCharacteristic *writeCharacteristic;

static TaskHandle_t connectTaskHandle;
static TeslaBLE::Client client;
static nvs_handle_t storage_handle;

static bool isConnected = false;
static bool isWhitelisted = false;
static const char *TAG = "TeslaBLE";
static const BLEAddress TESLA_ADDRESS("b0:d2:78:87:26:72");

extern "C" {
void app_main();
}

void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData,
              size_t length, bool isNotify) {
  VCSEC_FromVCSECMessage message_o = VCSEC_FromVCSECMessage_init_zero;
  int return_code = client.ParseFromVCSECMessage(pData, length, &message_o);
  if (return_code != 0) {
    ESP_LOGE(TAG, "Failed to parse incomming message\n");
    return;
  }

  ESP_LOGI(TAG, "Messages type: %d", message_o.which_sub_message);

  if (message_o.which_sub_message ==
      VCSEC_AuthenticationRequest_sessionInfo_tag) {
    ESP_LOGI(TAG, "Received ephemeral key\n");

    esp_err_t err =
        nvs_set_blob(storage_handle, "tesla_key",
                     message_o.sub_message.sessionInfo.publicKey.bytes,
                     message_o.sub_message.sessionInfo.publicKey.size);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save tesla key: %s", esp_err_to_name(err));
      return;
    }

    int result_code =
        client.LoadTeslaKey(message_o.sub_message.sessionInfo.publicKey.bytes,
                            message_o.sub_message.sessionInfo.publicKey.size);

    if (result_code != 0) {
      ESP_LOGE(TAG, "Failed load tesla key");
      return;
    }

    isConnected = true;
  } else if (message_o.which_sub_message ==
             VCSEC_AuthenticationRequest_reasonsForAuth_tag) {
    ESP_LOGI(TAG, "Received new counter from the car: %lu",
             message_o.sub_message.commandStatus.sub_message.signedMessageStatus
                 .counter);
    client.SetCounter(&message_o.sub_message.commandStatus.sub_message
                           .signedMessageStatus.counter);

    esp_err_t err = nvs_set_u32(storage_handle, "counter",
                                message_o.sub_message.commandStatus.sub_message
                                    .signedMessageStatus.counter);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save counter: %s", esp_err_to_name(err));
    }
  } else if (message_o.which_sub_message ==
             VCSEC_FromVCSECMessage_authenticationRequest_tag) {
    printf("Received authenticationRequest %d",
           message_o.sub_message.authenticationRequest.requestedLevel);
  } else if (message_o.which_sub_message ==
             VCSEC_FromVCSECMessage_vehicleStatus_tag) {
    ESP_LOGI(TAG, "Received status message:");
    ESP_LOGI(TAG, "Car is \"%s\"",
             message_o.sub_message.vehicleStatus.vehicleLockState ? "locked"
                                                                  : "unlocked");
    ESP_LOGI(TAG, "Car is \"%s\"",
             message_o.sub_message.vehicleStatus.vehicleSleepStatus
                 ? "awake"
                 : "sleeping");
    ESP_LOGI(TAG, "Charge port is \"%s\"",
             message_o.sub_message.vehicleStatus.closureStatuses.chargePort
                 ? "open"
                 : "closed");
    ESP_LOGI(TAG, "Front driver door is \"%s\"",
             message_o.sub_message.vehicleStatus.closureStatuses.frontDriverDoor
                 ? "open"
                 : "closed");
    ESP_LOGI(
        TAG, "Front passenger door is \"%s\"",
        message_o.sub_message.vehicleStatus.closureStatuses.frontPassengerDoor
            ? "open"
            : "closed");
    ESP_LOGI(TAG, "Rear driver door is \"%s\"",
             message_o.sub_message.vehicleStatus.closureStatuses.rearDriverDoor
                 ? "open"
                 : "closed");
    ESP_LOGI(
        TAG, "Rear passenger door is \"%s\"",
        message_o.sub_message.vehicleStatus.closureStatuses.rearPassengerDoor
            ? "open"
            : "closed");
    ESP_LOGI(TAG, "Front trunk is \"%s\"",
             message_o.sub_message.vehicleStatus.closureStatuses.frontTrunk
                 ? "open"
                 : "closed");
    ESP_LOGI(TAG, "Rear trunk is \"%s\"",
             message_o.sub_message.vehicleStatus.closureStatuses.rearTrunk
                 ? "open"
                 : "closed");
  }

  esp_err_t err = nvs_commit(storage_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed commit storage: %s", esp_err_to_name(err));
  }
}

bool connectToCar() {
  BLEClient *pClient = BLEDevice::createClient();
  pClient->connect(TESLA_ADDRESS);

  if (!pClient->isConnected()) {
    ESP_LOGI(TAG, "Failed to connect!");
    return false;
  }

  ESP_LOGI(TAG, "Connected to car!");

  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    ESP_LOGE(TAG, "Failed to find our service UUID: %s",
             serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  readCharacteristic = pRemoteService->getCharacteristic(readUUID);
  if (readCharacteristic == nullptr) {
    ESP_LOGE(TAG, "Failed to find our read characteristic UUID: %s",
             readUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  if (!readCharacteristic->subscribe(false, notifyCB)) {
    ESP_LOGE(TAG, "Failed to subscribe to characteristic UUID: %s",
             readUUID.toString().c_str());
    return false;
  }

  writeCharacteristic = pRemoteService->getCharacteristic(writeUUID);
  if (writeCharacteristic == nullptr) {
    ESP_LOGE(TAG, "Failed to find our write characteristic UUID: %s",
             writeUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  return true;
}

void connectTask(void *parameter) {
  for (;;) {
    if (connectToCar()) {
      ESP_LOGI(TAG, "Connected to Car!");

      if (isConnected == false) {
        unsigned char whitelist_message_buffer[200];
        size_t whitelist_message_length = 0;
        int return_code = client.BuildWhiteListMessage(
            whitelist_message_buffer, &whitelist_message_length);

        if (return_code != 0) {
          ESP_LOGE(TAG, "Failed to build whitelist message");
          continue;
        }

        if (writeCharacteristic->writeValue(whitelist_message_buffer,
                                            whitelist_message_length)) {
          ESP_LOGI(TAG, "Please tab your card now on the reader");
        }
      }

      while (isConnected == false) {
        unsigned char ephemeral_key_message_buffer[200];
        size_t ephemeral_key_message_length = 0;
        int return_code = client.BuildEphemeralKeyMessage(
            ephemeral_key_message_buffer, &ephemeral_key_message_length);

        if (return_code != 0) {
          ESP_LOGE(TAG, "Failed to build whitelist message\n");
          continue;
        }

        if (writeCharacteristic->writeValue(ephemeral_key_message_buffer,
                                            ephemeral_key_message_length)) {
          ESP_LOGI(TAG, "Waiting for keycard to be tapped...\n");
        }

        usleep(10000000);
      }

      unsigned char action_message_buffer[200];
      size_t action_message_bufferlength = 0;
      VCSEC_RKEAction_E action = VCSEC_RKEAction_E_RKE_ACTION_OPEN_CHARGE_PORT;
      int return_code = client.BuildActionMessage(
          &action, action_message_buffer, &action_message_bufferlength);

      if (return_code != 0) {
        ESP_LOGE(TAG, "Failed to build action message");
        usleep(5000000);
        continue;
      }

      if (writeCharacteristic->writeValue(action_message_buffer,
                                          action_message_bufferlength)) {
        ESP_LOGI(TAG, "Sent message to car");
      }

      vTaskDelete(connectTaskHandle);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay a second between loops.
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "Starting");

  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize flash: %s", esp_err_to_name(err));
    esp_restart();
  }

  err = nvs_open("storage", NVS_READWRITE, &storage_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
    esp_restart();
  }

  client = TeslaBLE::Client{};
  size_t required_private_key_size = 0;
  err = nvs_get_blob(storage_handle, "private_key", NULL,
                     &required_private_key_size);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed read private key from storage: %s",
             esp_err_to_name(err));
  }

  if (required_private_key_size == 0) {
    int result_code = client.CreatePrivateKey();
    if (result_code != 0) {
      ESP_LOGI(TAG, "Failed create private key");
      esp_restart();
    }

    unsigned char private_key_buffer[300];
    size_t private_key_length = 0;
    client.GetPrivateKey(private_key_buffer, sizeof(private_key_buffer),
                         &private_key_length);

    esp_err_t err = nvs_set_blob(storage_handle, "private_key",
                                 private_key_buffer, private_key_length);

    err = nvs_commit(storage_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed commit storage: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Private key successfully created");
  } else {
    unsigned char private_key_buffer[required_private_key_size];
    err = nvs_get_blob(storage_handle, "private_key", private_key_buffer,
                       &required_private_key_size);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed read private key from storage: %s",
               esp_err_to_name(err));
      esp_restart();
    }

    int result_code =
        client.LoadPrivateKey(private_key_buffer, required_private_key_size);
    if (result_code != 0) {
      ESP_LOGE(TAG, "Failed load private key");
      esp_restart();
    }

    ESP_LOGI(TAG, "Private key loaded successfully");
    TeslaBLE::DumpBuffer("\n", private_key_buffer, required_private_key_size);
  }

  size_t required_tesla_key_size = 0;
  err =
      nvs_get_blob(storage_handle, "tesla_key", NULL, &required_tesla_key_size);
  if (required_tesla_key_size > 0) {
    isWhitelisted = true;
    unsigned char tesla_key_buffer[required_tesla_key_size];

    err = nvs_get_blob(storage_handle, "tesla_key", tesla_key_buffer,
                       &required_tesla_key_size);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed read tesla key from storage: %s",
               esp_err_to_name(err));
      esp_restart();
    }

    int result_code =
        client.LoadTeslaKey(tesla_key_buffer, required_tesla_key_size);

    if (result_code != 0) {
      ESP_LOGE(TAG, "Failed load tesla key");
      esp_restart();
    }

    ESP_LOGI(TAG, "Tesla key loaded successfully");
    isConnected = true;
  }

  uint32_t counter = 0;
  err = nvs_get_u32(storage_handle, "counter", &counter);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed read counter from storage: %s", esp_err_to_name(err));
  }

  if (counter > 0) {
    client.SetCounter(&counter);
    ESP_LOGI(TAG, "Loaded old counter %lu", counter);
  }

  BLEDevice::init("TeslaBLE");
  xTaskCreate(connectTask, "connectTask", 5000, NULL, 1, &connectTaskHandle);
}