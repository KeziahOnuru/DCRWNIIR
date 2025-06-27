#ifndef FCM_NOTIFICATION_H
#define FCM_NOTIFICATION_H

/**
 * Sends a Firebase Cloud Messaging (FCM) notification
 * 
 * @param oauth_token OAuth2 token for authentication
 * @param app_token Recipient application's token
 * @param title Notification title
 * @param body Notification message body
 * @param data_type Custom data type (can be NULL)
 * @param project_id Firebase project ID
 * @return 0 on success, -1 on failure
 */
int send_fcm_notification(const char* oauth_token, const char* app_token, 
                         const char* title, const char* body, 
                         const char* data_type, const char* project_id);

/**
 * Convenience function to send a door close reminder
 * 
 * @param app_token Recipient application's token
 * @param service_account_file Path to the service account JSON file
 * @return 0 on success, -1 on failure
 */
int send_door_close_reminder(const char* app_token, const char* service_account_file);

#endif // FCM_NOTIFICATION_H
