#pragma once

/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <string>
#include <list>
#include <cmath>
#include <wx/datetime.h>

#include "../xlights/src-core/utils/UtilFunctions.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"

#include <log.h>

class City
{
    static std::list<City> _cities;

public:
    float _latitude;
    float _longitude;
    std::string _city;
    std::string _country;
    std::string _state;
    float _timeZone;

    City(std::string country, std::string city, std::string state, float latitude, float longitude, float timezone)
    {
        _latitude = latitude;
        _longitude = longitude;
        _city = city;
        _country = country;
        _state = state;
        _timeZone = timezone;
    }

    static void Test()
    {
        City* city = GetCity("Sydney");
        wxDateTime date = wxDateTime::Now();
        date.SetDay(1);
        date.SetMonth(wxDateTime::Jan);
        date.SetYear(2018);
        for (int m = 0; m < 12; m++)
        {
            date.SetMonth((wxDateTime::Month)m);
            wxDateTime sunrise = city->GetSunrise(date);
            wxDateTime sunset = city->GetSunset(date);
            spdlog::debug("Date {} Sunrise {:02d}:{:02d} Sunset {:02d}:{:02d}", date.FormatISODate().ToStdString(), sunrise.GetHour(), sunrise.GetMinute(), sunset.GetHour(), sunset.GetMinute());
        }
    }

    static City* GetCity(std::string city)
    {
        for (auto it = City::_cities.begin(); it != City::_cities.end(); ++it)
        {
            if (it->_city == city)
            {
                return &(*it);
            }
        }

        return nullptr;
    }

    static std::list<std::string> GetCities()
    {
        std::list<std::string> res;

        for (auto it = _cities.begin(); it != _cities.end(); ++it)
        {
            res.push_back(it->_city);
        }
        return res;
    }

    static bool GetCityLocation(std::string city, float& latitude, float& longitude)
    {
        for (auto it = City::_cities.begin(); it != City::_cities.end(); ++it)
        {
            if (it->_city == city)
            {
                latitude = it->_latitude;
                longitude = it->_longitude;
                return true;
            }
        }

        latitude = 0;
        longitude = 0;
        return false;
    }

    enum class SunEvent { Dawn, Sunrise, Sunset, Dusk };

    // local time of day of the event on the given date; dawn and dusk are civil twilight
    static wxDateTime GetSunTime(double latitude, double longitude, const wxDateTime& date, SunEvent event);

    // the listed city closest to a location
    static std::string GetNearestCity(double latitude, double longitude);

    wxDateTime GetSunrise(wxDateTime date)
    {
        return GetSunTime(_latitude, _longitude, date, SunEvent::Sunrise);
    }

    wxDateTime GetSunset(wxDateTime date)
    {
        return GetSunTime(_latitude, _longitude, date, SunEvent::Sunset);
    }

    static bool GetDefaultCityLocation(float timezone, float& latitude, float& longitude)
    {
        for (auto it = City::_cities.begin(); it != City::_cities.end(); ++it)
        {
            if (it->_timeZone == timezone)
            {
                latitude = it->_latitude;
                longitude = it->_longitude;
                return true;
            }
        }

        latitude = 0;
        longitude = 0;
        return false;
    }
};
