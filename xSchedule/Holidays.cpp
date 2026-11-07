/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "Holidays.h"

namespace Holidays {

namespace {

wxDateTime Fixed(int year, wxDateTime::Month month, int day) {
    return wxDateTime((wxDateTime::wxDateTime_t)day, month, year);
}

// the nth (1 based) given weekday of a month; n = -1 means the last one
wxDateTime NthWeekday(int year, wxDateTime::Month month, wxDateTime::WeekDay weekday, int n) {
    wxDateTime d;
    d.SetToWeekDay(weekday, n, month, year);
    return d.GetDateOnly();
}

// Gregorian Easter Sunday (anonymous Gregorian algorithm)
wxDateTime Easter(int year) {
    const int a = year % 19;
    const int b = year / 100;
    const int c = year % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int month = (h + l - 7 * m + 114) / 31; // 3 = March, 4 = April
    const int day = ((h + l - 7 * m + 114) % 31) + 1;
    return Fixed(year, (wxDateTime::Month)(month - 1), day);
}

// the given weekday on or before a date
wxDateTime OnOrBefore(wxDateTime date, wxDateTime::WeekDay weekday) {
    while (date.GetWeekDay() != weekday) date -= wxDateSpan::Day();
    return date;
}

} // namespace

const std::vector<Holiday>& All() {
    static const std::vector<Holiday> holidays = {
        { "newyearsday", "New Year's Day (Jan 1)", "" },
        { "epiphany", "Epiphany (Jan 6)", "" },
        { "mlkday_us", "Martin Luther King Jr. Day (US, 3rd Mon in Jan)", "US" },
        { "australiaday", "Australia Day (Jan 26)", "AU" },
        { "waitangiday", "Waitangi Day (NZ, Feb 6)", "NZ" },
        { "valentines", "Valentine's Day (Feb 14)", "" },
        { "familyday_ca", "Family Day (Canada, 3rd Mon in Feb)", "CA" },
        { "presidentsday_us", "Presidents' Day (US, 3rd Mon in Feb)", "US" },
        { "stpatricks", "St. Patrick's Day (Mar 17)", "" },
        { "mothering_uk", "Mothering Sunday (UK, Ireland, 3 weeks before Easter)", "GB IE" },
        { "goodfriday", "Good Friday", "CA GB IE AU NZ DE ZA" },
        { "easter", "Easter Sunday", "" },
        { "eastermonday", "Easter Monday", "CA GB IE AU NZ DE FR BE" },
        { "anzacday", "Anzac Day (Australia, NZ, Apr 25)", "AU NZ" },
        { "freedomday_za", "Freedom Day (South Africa, Apr 27)", "ZA" },
        { "earlymay", "Early May bank holiday (UK, Ireland, 1st Mon in May)", "GB IE" },
        { "mothersday_us", "Mother's Day (2nd Sun in May)", "US CA AU NZ DE BE ZA" },
        { "ascension", "Ascension Day (Easter + 39 days)", "DE FR BE" },
        { "victoriaday_ca", "Victoria Day (Canada, Mon before May 25)", "CA" },
        { "memorialday_us", "Memorial Day (US, last Mon in May)", "US" },
        { "springbank_uk", "Spring bank holiday (UK, last Mon in May)", "GB" },
        { "whitsunday", "Whit Sunday (Pentecost)", "DE FR BE" },
        { "whitmonday", "Whit Monday", "DE FR BE" },
        { "junebank_ie", "June bank holiday (Ireland, 1st Mon in Jun)", "IE" },
        { "kingsbirthday_au", "King's Birthday (Australia, 2nd Mon in Jun)", "AU" },
        { "fathersday_us", "Father's Day (3rd Sun in Jun)", "US CA GB IE FR ZA" },
        { "juneteenth_us", "Juneteenth (US, Jun 19)", "US" },
        { "canadaday", "Canada Day (Jul 1)", "CA" },
        { "independenceday_us", "Independence Day (US, Jul 4)", "US" },
        { "bastilleday", "Bastille Day (France, Jul 14)", "FR" },
        { "nationalday_be", "Belgian National Day (Jul 21)", "BE" },
        { "civicholiday_ca", "Civic Holiday (Canada, 1st Mon in Aug)", "CA" },
        { "augustbank_ie", "August bank holiday (Ireland, 1st Mon in Aug)", "IE" },
        { "summerbank_uk", "Summer bank holiday (UK, last Mon in Aug)", "GB" },
        { "laborday_us", "Labor Day (1st Mon in Sep)", "US CA" },
        { "fathersday_au", "Father's Day (Australia, NZ, 1st Sun in Sep)", "AU NZ" },
        { "heritageday_za", "Heritage Day (South Africa, Sep 24)", "ZA" },
        { "germanunity", "German Unity Day (Oct 3)", "DE" },
        { "thanksgiving_ca", "Thanksgiving (Canada, 2nd Mon in Oct)", "CA" },
        { "columbusday_us", "Columbus Day (US, 2nd Mon in Oct)", "US" },
        { "labourday_nz", "Labour Day (NZ, 4th Mon in Oct)", "NZ" },
        { "octoberbank_ie", "October bank holiday (Ireland, last Mon in Oct)", "IE" },
        { "halloween", "Halloween (Oct 31)", "" },
        { "bonfirenight", "Bonfire Night (UK, NZ, Nov 5)", "GB NZ" },
        { "remembrancesunday_uk", "Remembrance Sunday (UK, 2nd Sun in Nov)", "GB" },
        { "veteransday_us", "Veterans Day (US, Nov 11)", "US" },
        { "remembranceday", "Remembrance Day (Nov 11)", "CA GB AU NZ" },
        { "armisticeday", "Armistice Day (France, Belgium, Nov 11)", "FR BE" },
        { "stmartin", "St. Martin's Day (Germany, Belgium, Nov 11)", "DE BE" },
        { "thanksgiving_us", "Thanksgiving (US, 4th Thu in Nov)", "US" },
        { "advent1", "First Sunday of Advent", "" },
        { "stnicholas", "St. Nicholas Day (Germany, Belgium, Dec 6)", "DE BE" },
        { "reconciliationday_za", "Day of Reconciliation (South Africa, Dec 16)", "ZA" },
        { "christmaseve", "Christmas Eve (Dec 24)", "" },
        { "christmas", "Christmas Day (Dec 25)", "" },
        { "boxingday", "Boxing Day / St. Stephen's Day (Dec 26)", "CA GB IE AU NZ ZA DE BE" },
        { "newyearseve", "New Year's Eve (Dec 31)", "" },
    };
    return holidays;
}

wxDateTime DateFor(const std::string& id, int year) {
    if (id == "newyearsday") return Fixed(year, wxDateTime::Jan, 1);
    if (id == "epiphany") return Fixed(year, wxDateTime::Jan, 6);
    if (id == "mlkday_us") return NthWeekday(year, wxDateTime::Jan, wxDateTime::Mon, 3);
    if (id == "australiaday") return Fixed(year, wxDateTime::Jan, 26);
    if (id == "waitangiday") return Fixed(year, wxDateTime::Feb, 6);
    if (id == "valentines") return Fixed(year, wxDateTime::Feb, 14);
    if (id == "familyday_ca" || id == "presidentsday_us") return NthWeekday(year, wxDateTime::Feb, wxDateTime::Mon, 3);
    if (id == "stpatricks") return Fixed(year, wxDateTime::Mar, 17);
    if (id == "mothering_uk") return Easter(year) - wxDateSpan::Days(21);
    if (id == "goodfriday") return Easter(year) - wxDateSpan::Days(2);
    if (id == "easter") return Easter(year);
    if (id == "eastermonday") return Easter(year) + wxDateSpan::Days(1);
    if (id == "anzacday") return Fixed(year, wxDateTime::Apr, 25);
    if (id == "freedomday_za") return Fixed(year, wxDateTime::Apr, 27);
    if (id == "earlymay") return NthWeekday(year, wxDateTime::May, wxDateTime::Mon, 1);
    if (id == "mothersday_us") return NthWeekday(year, wxDateTime::May, wxDateTime::Sun, 2);
    if (id == "ascension") return Easter(year) + wxDateSpan::Days(39);
    if (id == "victoriaday_ca") return OnOrBefore(Fixed(year, wxDateTime::May, 24), wxDateTime::Mon);
    if (id == "memorialday_us" || id == "springbank_uk") return NthWeekday(year, wxDateTime::May, wxDateTime::Mon, -1);
    if (id == "whitsunday") return Easter(year) + wxDateSpan::Days(49);
    if (id == "whitmonday") return Easter(year) + wxDateSpan::Days(50);
    if (id == "junebank_ie") return NthWeekday(year, wxDateTime::Jun, wxDateTime::Mon, 1);
    if (id == "kingsbirthday_au") return NthWeekday(year, wxDateTime::Jun, wxDateTime::Mon, 2);
    if (id == "fathersday_us") return NthWeekday(year, wxDateTime::Jun, wxDateTime::Sun, 3);
    if (id == "juneteenth_us") return Fixed(year, wxDateTime::Jun, 19);
    if (id == "canadaday") return Fixed(year, wxDateTime::Jul, 1);
    if (id == "independenceday_us") return Fixed(year, wxDateTime::Jul, 4);
    if (id == "bastilleday") return Fixed(year, wxDateTime::Jul, 14);
    if (id == "nationalday_be") return Fixed(year, wxDateTime::Jul, 21);
    if (id == "civicholiday_ca" || id == "augustbank_ie") return NthWeekday(year, wxDateTime::Aug, wxDateTime::Mon, 1);
    if (id == "summerbank_uk") return NthWeekday(year, wxDateTime::Aug, wxDateTime::Mon, -1);
    if (id == "laborday_us") return NthWeekday(year, wxDateTime::Sep, wxDateTime::Mon, 1);
    if (id == "fathersday_au") return NthWeekday(year, wxDateTime::Sep, wxDateTime::Sun, 1);
    if (id == "heritageday_za") return Fixed(year, wxDateTime::Sep, 24);
    if (id == "germanunity") return Fixed(year, wxDateTime::Oct, 3);
    if (id == "thanksgiving_ca" || id == "columbusday_us") return NthWeekday(year, wxDateTime::Oct, wxDateTime::Mon, 2);
    if (id == "labourday_nz") return NthWeekday(year, wxDateTime::Oct, wxDateTime::Mon, 4);
    if (id == "octoberbank_ie") return NthWeekday(year, wxDateTime::Oct, wxDateTime::Mon, -1);
    if (id == "halloween") return Fixed(year, wxDateTime::Oct, 31);
    if (id == "bonfirenight") return Fixed(year, wxDateTime::Nov, 5);
    if (id == "remembrancesunday_uk") return NthWeekday(year, wxDateTime::Nov, wxDateTime::Sun, 2);
    if (id == "veteransday_us" || id == "remembranceday" || id == "armisticeday" || id == "stmartin") return Fixed(year, wxDateTime::Nov, 11);
    if (id == "thanksgiving_us") return NthWeekday(year, wxDateTime::Nov, wxDateTime::Thu, 4);
    // the fourth Sunday of Advent is the last Sunday on or before Christmas Eve
    if (id == "advent1") return OnOrBefore(Fixed(year, wxDateTime::Dec, 24), wxDateTime::Sun) - wxDateSpan::Days(21);
    if (id == "stnicholas") return Fixed(year, wxDateTime::Dec, 6);
    if (id == "reconciliationday_za") return Fixed(year, wxDateTime::Dec, 16);
    if (id == "christmaseve") return Fixed(year, wxDateTime::Dec, 24);
    if (id == "christmas") return Fixed(year, wxDateTime::Dec, 25);
    if (id == "boxingday") return Fixed(year, wxDateTime::Dec, 26);
    if (id == "newyearseve") return Fixed(year, wxDateTime::Dec, 31);
    return wxInvalidDateTime;
}

std::string NameFor(const std::string& id) {
    for (const auto& h : All()) {
        if (h.id == id) return h.name;
    }
    return id;
}

const std::vector<std::pair<std::string, std::string>>& Countries() {
    static const std::vector<std::pair<std::string, std::string>> countries = {
        { "AU", "Australia" }, { "BE", "Belgium" }, { "CA", "Canada" }, { "FR", "France" }, { "DE", "Germany" },
        { "IE", "Ireland" }, { "NZ", "New Zealand" }, { "ZA", "South Africa" }, { "GB", "United Kingdom" }, { "US", "United States" },
    };
    return countries;
}

std::string CountryCode(const std::string& countryName) {
    for (const auto& c : Countries()) {
        if (c.second == countryName) return c.first;
    }
    return "";
}

std::string CountryName(const std::string& code) {
    for (const auto& c : Countries()) {
        if (c.first == code) return c.second;
    }
    return "";
}

namespace {
std::string __country;
}

void SetCountry(const std::string& code) {
    __country = code;
}

const std::string& GetCountry() {
    return __country;
}

bool IsShown(const Holiday& holiday) {
    if (__country.empty() || holiday.countries.empty()) return true;
    return (" " + holiday.countries + " ").find(" " + __country + " ") != std::string::npos;
}

} // namespace Holidays
