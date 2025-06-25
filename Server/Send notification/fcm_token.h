#ifndef FCM_TOKEN_H
#define FCM_TOKEN_H

/**
 * Récupère un token OAuth2 pour Firebase Cloud Messaging
 * 
 * @param service_account_file Chemin vers le fichier JSON du compte de service
 * @return Token OAuth2 (à libérer avec free()) ou NULL en cas d'erreur
 */
char* get_fcm_oauth_token(const char* service_account_file);

#endif // FCM_TOKEN_H