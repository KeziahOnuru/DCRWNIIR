#ifndef FCM_NOTIFICATION_H
#define FCM_NOTIFICATION_H

/**
 * Envoie une notification Firebase Cloud Messaging (FCM)
 * 
 * @param oauth_token Token OAuth2 pour l'authentification
 * @param app_token Token de l'application destinataire
 * @param title Titre de la notification
 * @param body Corps du message de la notification
 * @param data_type Type de données personnalisées (peut être NULL)
 * @param project_id ID du projet Firebase
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_fcm_notification(const char* oauth_token, const char* app_token, 
                         const char* title, const char* body, 
                         const char* data_type, const char* project_id);

/**
 * Fonction pratique pour envoyer un rappel de fermeture de porte
 * 
 * @param app_token Token de l'application destinataire
 * @param service_account_file Chemin vers le fichier JSON du compte de service
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int send_door_close_reminder(const char* app_token, const char* service_account_file);

#endif // FCM_NOTIFICATION_H