//
// Created by michele on 29/09/2020.
//

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <string>
#include <fstream>

class Config {
    std::string path_to_watch;
    std::string database_path;
    int millis_filesystem_watcher;
    int event_queue_size;
    int seconds_between_reconnections;
    int max_connection_retries;
    int max_server_error_retries;
    int max_auth_error_retries;
    int timeout_seconds;
    int select_timeout_seconds;
    int max_response_waiting;

public:
    Config() = default;
    void load(const std::string &configFilePath);
    const std::string getPathToWatch();
    const std::string getDatabasePath();
    int getMillisFilesystemWatcher();
    int getEventQueueSize();
    int getSecondsBetweenReconnections();
    int getMaxConnectionRetries();
    int getMaxServerErrorRetries();
    int getMaxAuthErrorRetries();
    int getTimeoutSeconds();
    int getSelectTimeoutSeconds();
    int getMaxResponseWaiting();
};


#endif //CLIENT_CONFIG_H
