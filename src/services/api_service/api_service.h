#pragma once

#include <Arduino.h>
#include <map>

#ifndef LV_API_SERVICE_H
#define LV_API_SERVICE_H
class ApiService
{
public:
    String post(String url, String body, std::map<String, String> header);
};
#endif