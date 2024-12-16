#pragma once
#include <chrono>
#include <ctime>
#include <iostream>


class Time {
public:
    bool exists = false;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    static Time fromString(const std::string& str) {
        Time timeObj;
        std::istringstream ss(str);
        char delimiter;

        // Format: yyyy-mm-dd hh:ii:ss
        if (!(ss >> timeObj.year >> delimiter)) {
            logger::error("failed to read year from {}", str);
            return timeObj;
        }
        if (!(ss >> timeObj.month >> delimiter)) {
            logger::error("failed to read month from {}", str);
            return timeObj;
        }
        if (!(ss >> timeObj.day >> delimiter)) {
            logger::error("failed to read day from {}", str);
            return timeObj;
        }
        if (!(ss >> timeObj.hour >> delimiter)) {
            logger::error("failed to read hour from {}", str);
            return timeObj;
        }
        if (!(ss >> timeObj.minute >> delimiter)) {
            logger::error("failed to read minute from {}", str);
            return timeObj;
        }
        if (!(ss >> timeObj.second)) {
            logger::error("failed to read second from {}", str);
            return timeObj;
        }

        timeObj.exists = true;

        return timeObj;
    }

    
    static std::pair<Time, Time> fromDatePairString(const std::string& str) {
        Time startDate, endDate;
        std::istringstream ss(str);
        char delimiter;

        if (!(ss >> startDate.day >> delimiter) || delimiter != '-') {
            return std::make_pair(startDate, endDate);
        }
        if (!(ss >> startDate.month >> delimiter) || delimiter != '~') {
            return std::make_pair(startDate, endDate);
        }
        if (!(ss >> endDate.day >> delimiter) || delimiter != '-') {
            return std::make_pair(startDate, endDate);
        }
        if (!(ss >> endDate.month)) {
  
            return std::make_pair(startDate, endDate);
        }
        startDate.exists = true;
        endDate.exists = true;
        return std::make_pair(startDate, endDate);
    }

    bool isBetweenMD(Time& a, Time& b) {
        if (month < a.month) {
            return false;
        }
        if (month > b.month) {
            return false;
        }
        if (day < a.day) {
            return false;
        }
        if (day > b.day) {
            return false;
        }
        return true;
    }

    static Time now() {

        auto result = Time();
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::tm* localTime = std::localtime(&currentTime);
        result.year = 1900 + localTime->tm_year;  // Years since 1900
        result.month = 1 + localTime->tm_mon;     // Months are 0-based
        result.day = localTime->tm_mday;
        result.hour = localTime->tm_hour;
        result.minute = localTime->tm_min;
        result.second = localTime->tm_sec;
        result.exists = true;
        return result;
    }

    void log(std::string text) {
        logger::trace("{}: {}-{}-{} {}:{}:{}", text, year, month, day, hour, minute, second);
    }
};