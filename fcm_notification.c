#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "fcm_notification.h"
#include "fcm_token.h"

#define MAX_RESPONSE_SIZE 8192

// Structure pour stocker la réponse HTTP
struct APIResponse {
    char *data;
    size_t size;
};

// Callback pour recevoir les données de la réponse HTTP
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct APIResponse *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        printf("Erreur: pas assez de mémoire (realloc)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

// Fonction pour créer le JSON du message FCM
static char* create_fcm_message_json(const char* app_token, const char* title, 
                                   const char* body, const char* data_type) {
    json_object *root = json_object_new_object();
    json_object *message = json_object_new_object();
    json_object *notification = json_object_new_object();
    
    // Créer le token
    json_object *token = json_object_new_string(app_token);
    json_object_object_add(message, "token", token);
    
    // Créer la notification
    json_object *title_obj = json_object_new_string(title);
    json_object *body_obj = json_object_new_string(body);
    json_object_object_add(notification, "title", title_obj);
    json_object_object_add(notification, "body", body_obj);
    json_object_object_add(message, "notification", notification);
    
    // Créer les données personnalisées si spécifiées
    if (data_type && strlen(data_type) > 0) {
        json_object *data = json_object_new_object();
        json_object *type_obj = json_object_new_string(data_type);
        json_object_object_add(data, "type", type_obj);
        json_object_object_add(message, "data", data);
    }
    
    // Assembler le message final
    json_object_object_add(root, "message", message);
    
    // Convertir en string
    const char* json_string = json_object_to_json_string(root);
    char* result = strdup(json_string);
    
    json_object_put(root);
    return result;
}

// Fonction pour envoyer la notification FCM
int send_fcm_notification(const char* oauth_token, const char* app_token, 
                         const char* title, const char* body, 
                         const char* data_type, const char* project_id) {
    if (!oauth_token || !app_token || !title || !body || !project_id) {
        printf("Erreur: paramètres obligatoires manquants\n");
        return -1;
    }
    
    // Créer le JSON du message
    char* message_json = create_fcm_message_json(app_token, title, body, data_type);
    if (!message_json) {
        printf("Erreur: impossible de créer le JSON du message\n");
        return -1;
    }
    
    printf("Message JSON créé: %s\n", message_json);
    
    // Initialiser CURL
    CURL *curl;
    CURLcode res;
    struct APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        printf("Erreur: impossible d'initialiser CURL\n");
        free(message_json);
        return -1;
    }
    
    // Préparer les headers
    struct curl_slist *headers = NULL;
    
    // Header Authorization avec le token OAuth2
    char auth_header[2048];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", oauth_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json; UTF-8");
    
    // Construire l'URL FCM avec l'ID du projet
    char fcm_url[512];
    snprintf(fcm_url, sizeof(fcm_url), 
             "https://fcm.googleapis.com/v1/projects/%s/messages:send", project_id);
    
    // Configurer CURL
    curl_easy_setopt(curl, CURLOPT_URL, fcm_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message_json);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Effectuer la requête
    printf("Envoi de la notification FCM...\n");
    res = curl_easy_perform(curl);
    
    // Vérifier le code de réponse HTTP
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Nettoyer
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(message_json);
    
    if (res != CURLE_OK) {
        printf("Erreur CURL: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }
    
    printf("Code de réponse HTTP: %ld\n", response_code);
    if (response.data) {
        printf("Réponse du serveur: %s\n", response.data);
        free(response.data);
    }
    
    if (response_code == 200) {
        printf("Notification envoyée avec succès!\n");
        return 0;
    } else {
        printf("Erreur lors de l'envoi de la notification\n");
        return -1;
    }
}

// Fonction pratique pour envoyer le rappel de fermeture de porte
int send_door_close_reminder(const char* app_token, const char* service_account_file) {
    if (!app_token || !service_account_file) {
        printf("Erreur: paramètres manquants\n");
        return -1;
    }
    
    // Obtenir le token OAuth2
    printf("Récupération du token OAuth2...\n");
    char* oauth_token = get_fcm_oauth_token(service_account_file);
    if (!oauth_token) {
        printf("Erreur: impossible d'obtenir le token OAuth2\n");
        return -1;
    }
    
    printf("Token OAuth2 obtenu avec succès\n");
    
    // Envoyer la notification avec les valeurs fixes
    int result = send_fcm_notification(
        oauth_token,
        app_token,
        "Door-close reminder",
        "Room 508 : Don't forget to close the door !",
        "door-close-reminder",
        "door-close-reminder"  // ID du projet
    );
    
    free(oauth_token);
    return result;
}