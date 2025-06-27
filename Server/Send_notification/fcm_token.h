#ifndef FCM_TOKEN_H
#define FCM_TOKEN_H

/**
 * Retrieves an OAuth2 token for Firebase Cloud Messaging
 * 
 * @param service_account_file Path to the service account JSON file
 * @return OAuth2 token (must be freed with free()), or NULL on failure
 */
char* get_fcm_oauth_token(const char* service_account_file);

#endif // FCM_TOKEN_H
