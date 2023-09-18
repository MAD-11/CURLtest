#define CURL_STATICLIB
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <mosquitto.h>
#include <chrono>
#include <thread>

using json = nlohmann::json;
using namespace std;

// console cmd
// mosquitto_pub -h test.mosquitto.org -t topic -m "28.9" -d --cafile C:\path -p 8885 -u user -P password


static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

vector < pair<string, string >>  GetStationsValues(vector<string>& stations, vector<double>& values, string &health)
{
    CURL* curl;
    CURLcode res;
    string readBuffer;
    json responseJson, object;
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.data.gov.sg/v1/environment/air-temperature");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        responseJson = json::parse(readBuffer);
        object = responseJson["api_info"];
        health = object["status"];
    }

    double value = 0.0;
    string station_id;
    int i = -1;
    vector < pair<string, string >> stationValue;
    while(true)
    {
        object = responseJson["items"][0]["readings"][++i];
        
        if (!(object.empty()))
        {
            station_id = object["station_id"];
            
            if (find(stations.begin(),stations.end(), station_id) != stations.end())
            {
                stationValue.push_back(make_pair("/api/temperature/"+station_id, to_string(object["value"])));
            }
        }
        else 
        {
            return stationValue;
        }
    }    
}


int SendTopic(vector < pair<string, string >>& stationValue, string& health) {
    
    int rc;
    struct mosquitto* mosq;

    mosquitto_lib_init();
    mosq = mosquitto_new("topic_publish", true, NULL);

    mosquitto_username_pw_set(mosq, "rw", "readwrite");
    mosquitto_tls_set(mosq, "D:/Users/mad/VisualStudio/projects/CURLtest/ssl/mosquitto.org.crt", nullptr, nullptr, nullptr, nullptr);
    mosquitto_tls_opts_set(mosq, 1, NULL, NULL);
    mosquitto_tls_insecure_set(mosq, false);
    
    rc = mosquitto_connect(mosq, "test.mosquitto.org", 8885, 60);
    if (rc != 0) {
        printf("Client could not connect to broker! Error Code: %d\n", rc);
        mosquitto_destroy(mosq);
        return -1;
    }

    char healthValue[10];
    strcpy_s(healthValue, health.c_str());
    char healthTopic[] = "/api/status";
    stationValue.push_back(make_pair(healthTopic, healthValue));

    for (auto el: stationValue)
    {
        this_thread::sleep_for(std::chrono::milliseconds(500));
        char topic[50];
        char value[50];
        sprintf_s(topic, "%s", el.first.c_str());
        sprintf_s(value, "%s", el.second.c_str());
        mosquitto_publish(mosq, NULL, topic, strlen(value), &value, 0, false);
    }

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);

    mosquitto_lib_cleanup();

    return 0;
}


int main()
{
    vector<string> stations = {"S50","S107"};
    vector<double> values;
    string health = "";
    auto resp = GetStationsValues(stations, values, health);
    SendTopic(resp, health);


    return 0;
}