#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "fcm_notification.h"

int main() {
    // Initialiser libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Token de votre app (REMPLACEZ PAR LE VRAI TOKEN)
    const char* app_token = "fr16uYX2SyGQ6oxVja-iRU:APA91bEiOPvAycPo9Izr3HCNcPAS5qpv0tV0f1sPFhNSypTv36pg1fOMTqzKj4xU-XjpDNyZ7SsXyezQqjNT4FUo-GxEAjfnRs2bWYmM0G9eXb0l2yY9Sh4";
    
    // Envoyer la notification
    printf("=== Envoi du rappel de fermeture de porte ===\n");
    int result = send_door_close_reminder(app_token, "door-close-reminder-79f888941429.json");
    
    curl_global_cleanup();
    
    if (result == 0) {
        printf("✅ Notification envoyée avec succès!\n");
        return 0;
    } else {
        printf("❌ Erreur lors de l'envoi\n");
        return 1;
    }
}