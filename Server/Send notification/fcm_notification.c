#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "fcm_notification.h"
#include "fcm_token.h"

#define MAX_RESPONSE_SIZE 8192

struct APIResponse {
    char *data;
    size_t size;
};

// Called whenever data comes back from the server
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct APIResponse *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        printf("Oops: not enough memory (realloc failed)\n");
        return 0;
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

static char* create_fcm_message_json(const char* app_token, const char* title, 
                                   const char* body, const char* data_type) {
    json_object *root = json_object_new_object();
    json_object *message = json_object_new_object();
    json_object *notification = json_object_new_object();

    // Add where the message should go
    json_object_object_add(message, "token", json_object_new_string(app_token));

    // Add title and message text
    json_object_object_add(notification, "title", json_object_new_string(title));
    json_object_object_add(notification, "body", json_object_new_string(body));
    json_object_object_add(message, "notification", notification);

    // Add extra data
    if (data_type && strlen(data_type) > 0) {
        json_object *data = json_object_new_object();
        json_object_object_add(data, "type", json_object_new_string(data_type));
        json_object_object_add(message, "data", data);
    }

    // Add the message to the main JSON object
    json_object_object_add(root, "message", message);

    // Convert everything to text
    const char* json_string = json_object_to_json_string(root);
    char* result = strdup(json_string);

    // Free up memory
    json_object_put(root);
    return result;
}

// Actually sends the notification to FCM
int send_fcm_notification(const char* oauth_token, const char* app_token, 
                         const char* title, const char* body, 
                         const char* data_type, const char* project_id) {
    if (!oauth_token || !app_token || !title || !body || !project_id) {
        printf("Uh-oh: missing something important\n");
        return -1;
    }

    char* message_json = create_fcm_message_json(app_token, title, body, data_type);
    if (!message_json) {
        printf("Uh-oh: couldn’t make JSON\n");
        return -1;
    }

    printf("Here's the message JSON: %s\n", message_json);

    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};

    curl = curl_easy_init();
    if (!curl) {
        printf("Oops: couldn’t start curl\n");
        free(message_json);
        return -1;
    }

    // Set up HTTP headers
    struct curl_slist *headers = NULL;

    char auth_header[2048];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", oauth_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json; UTF-8");

    // Build the URL to talk to FCM
    char fcm_url[512];
    snprintf(fcm_url, sizeof(fcm_url), 
             "https://fcm.googleapis.com/v1/projects/%s/messages:send", project_id);

    // Tell curl what to do
    curl_easy_setopt(curl, CURLOPT_URL, fcm_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message_json);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    printf("Sending FCM notification...\n");
    res = curl_easy_perform(curl);

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Clean up memory
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(message_json);

    if (res != CURLE_OK) {
        printf("Curl messed up: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }

    printf("Server said: HTTP %ld\n", response_code);
    if (response.data) {
        printf("Server response: %s\n", response.data);
        free(response.data);
    }

    if (response_code == 200) {
        printf("Nice! Notification sent.\n");
        return 0;
    } else {
        printf("Bummer... notification failed.\n");
        return -1;
    }
}

int send_door_close_reminder(const char* app_token, const char* service_account_file) {
    if (!app_token || !service_account_file) {
        printf("Uh-oh: missing arguments\n");
        return -1;
    }

    printf("Getting OAuth token...\n");
    char* oauth_token = get_fcm_oauth_token(service_account_file);
    if (!oauth_token) {
        printf("Couldn’t get OAuth token\n");
        return -1;
    }

    printf("Got the token!\n");

    int result = send_fcm_notification(
        oauth_token,
        app_token,
        "Door-close reminder",
        "Room 508 : Don't forget to close the door !",
        "door-close-reminder",
        "door-close-reminder"
    );

    free(oauth_token);
    return result;
}
