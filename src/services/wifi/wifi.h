#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    void initialWifi(void);
    void reconnect(void);
    void pushMessage(const char *topic, const char *payload);

#ifdef __cplusplus
} /* extern "C" */
#endif