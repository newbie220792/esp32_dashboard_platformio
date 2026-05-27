#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    void initialWifi(void);
    void reconnect(void);
    void pushMessage(const char *topic, char *payload);

#ifdef __cplusplus
} /* extern "C" */
#endif