/**
 * @file hello_bsc5.cpp
 *
 * Simple program that prints the alt-az coordinates of a star from the
 * Yale Bright Star Catalog (BSC5) at the current time in Mexico City.
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <json.hpp>
#include <supernovas.h>

using namespace supernovas;
using json = nlohmann::json;

// Define Earth Orientation Parameters (EOP) for robust offline usage.
#define LEAP_SECONDS     37        // [s] current leap seconds
#define DUT1             0.114     // [s] UT1 - UTC time difference
#define POLAR_DX         230.0     // [mas] polar offset x
#define POLAR_DY         -62.0     // [mas] polar offset y

// Helper for case-insensitive string contains
bool contains_ci(const std::string& str, const std::string& query) {
    if (query.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        query.begin(), query.end(),
        [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != str.end();
}

// Cleans RA and Dec strings, keeping only numeric, sign, and dots, replacing rest with spaces
std::string clean_coord(std::string str) {
    std::string result = "";
    for (char c : str) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '+' || c == '-') {
            result += c;
        } else {
            result += ' ';
        }
    }
    return result;
}

// Safely converts JSON string to double, defaulting to 0.0 on error/missing keys
double get_double_or_zero(const json& star, const std::string& key) {
    if (!star.contains(key)) return 0.0;
    // If numeric in JSON, return directly
    if (star[key].is_number()) {
        try { return star[key].get<double>(); } catch (...) { return 0.0; }
    }
    if (star[key].is_string()) {
        std::string val_str = star[key].get<std::string>();
        std::string filtered;
        for (char c : val_str) {
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E') filtered += c;
        }
        if (!filtered.empty()) {
            try { return std::stod(filtered); } catch (...) { }
        }
    }
    return 0.0;
}

// Matches a star entry against a query (Name, HD, HR, SAO, or Star names notes)
bool match_star(const json& star, const std::string& query) {
    if (star.contains("Name") && star["Name"].is_string()) {
        if (contains_ci(star["Name"].get<std::string>(), query)) return true;
    }
    // Helper to get field as string regardless of JSON type
    auto get_field_string = [&](const json& j, const std::string& key) -> std::string {
        if (!j.contains(key)) return std::string();
        if (j[key].is_string()) return j[key].get<std::string>();
        if (j[key].is_number()) {
            try {
                double v = j[key].get<double>();
                std::ostringstream oss; oss << std::fixed << v; std::string s = oss.str();
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (!s.empty() && s.back() == '.') s.pop_back();
                return s;
            } catch (...) { return std::string(); }
        }
        return std::string();
    };

    std::string hd = get_field_string(star, "HD");
    if (!hd.empty()) {
        if (contains_ci(hd, query) || contains_ci("HD " + hd, query) || contains_ci("HD" + hd, query)) return true;
    }
    std::string hr = get_field_string(star, "HR");
    if (!hr.empty()) {
        if (contains_ci(hr, query) || contains_ci("HR " + hr, query) || contains_ci("HR" + hr, query)) return true;
    }
    std::string sao = get_field_string(star, "SAO");
    if (!sao.empty()) {
        if (contains_ci(sao, query) || contains_ci("SAO " + sao, query) || contains_ci("SAO" + sao, query)) return true;
    }

    if (star.contains("Notes") && star["Notes"].is_array()) {
        for (auto& note : star["Notes"]) {
            if (!note.contains("Category") || !note["Category"].is_string()) continue;
            if (!note.contains("Remark") || !note["Remark"].is_string()) continue;
            std::string category = note["Category"].get<std::string>();
            std::string remark = note["Remark"].get<std::string>();
            // Only use explicit star-name aliases from the Star names category.
            if (category == "Star names" && contains_ci(remark, query)) {
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    // 1. Get query string from command line argument, default to "Sirius"
    std::string query = "Sirius";
    if (argc > 1) {
        query = argv[1];
    }

    // Enable error tracing and debugging messages
    novas_debug(NOVAS_DEBUG_ON);

    std::cout << "========================================================\n";
    std::cout << "        BSC5 Catalog Monitor (Mexico City)              \n";
    std::cout << "========================================================\n";
    std::cout << "Searching for star: \"" << query << "\"\n\n";

    // 2. Load the JSON database
    std::string json_path = "../../common/data/bsc5.json";
    std::ifstream f(json_path);
    if (!f.is_open()) {
        // Fall back to absolute path
        json_path = "/home/rodolfo/Proyectos/Sky_Monitor/Repositorios/all_sky_monitor/src/common/data/bsc5.json";
        f.open(json_path);
    }
    if (!f.is_open()) {
        std::cerr << "Error: Could not open bsc5.json at either relative or absolute paths.\n";
        return 1;
    }

    json db;
    try {
        f >> db;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing bsc5.json: " << e.what() << "\n";
        return 1;
    }

    // 3. Search for the star in database
    json matched_star;
    bool found = false;
    for (const auto& star : db) {
        if (match_star(star, query)) {
            matched_star = star;
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Error: Star matching \"" << query << "\" not found in catalog.\n";
        return 1;
    }

    // 4. Extract parameters
    std::string name = matched_star.contains("Name") && matched_star["Name"].is_string() 
                       ? matched_star["Name"].get<std::string>() 
                       : query;

    std::string ra_str = matched_star.contains("RA") && matched_star["RA"].is_string() 
                         ? matched_star["RA"].get<std::string>() 
                         : "";
    std::string dec_str = matched_star.contains("Dec") && matched_star["Dec"].is_string() 
                          ? matched_star["Dec"].get<std::string>() 
                          : "";

    if (ra_str.empty() || dec_str.empty()) {
        std::cerr << "Error: Coordinates missing for matched star: " << name << "\n";
        return 1;
    }

    // Proper motion conversion (arcsec/yr -> rad/s)
    double pm_ra_arcsec = get_double_or_zero(matched_star, "pmRA");
    double pm_dec_arcsec = get_double_or_zero(matched_star, "pmDE");
    double pm_ra_si = pm_ra_arcsec * Unit::arcsec / Unit::yr;
    double pm_dec_si = pm_dec_arcsec * Unit::arcsec / Unit::yr;

    // Parallax conversion (arcsec -> rad)
    double parallax_arcsec = get_double_or_zero(matched_star, "Parallax");
    double parallax_si = parallax_arcsec * Unit::arcsec;

    // Radial velocity conversion (km/s -> m/s)
    double radvel_km_per_s = get_double_or_zero(matched_star, "RadVel");
    double radvel_si = radvel_km_per_s * Unit::km_per_s;

    // 5. Instantiate CatalogEntry
    auto entry = CatalogEntry(name, Equatorial(ra_str, dec_str, Equinox::j2000()))
            .proper_motion(pm_ra_si, pm_dec_si)
            .parallax(Angle(parallax_si))
            .radial_velocity(ScalarVelocity(radvel_si));

    if (!entry) {
        std::cerr << "Error: Invalid catalog entry created for star: " << name << ".\n";
        return 1;
    }

    auto source = entry.to_source();

    // 6. Setup time & observer
    EOP eop(LEAP_SECONDS, DUT1, POLAR_DX * Unit::mas, POLAR_DY * Unit::mas);
    Time t = Time::now(eop);
    auto obs = Observer::on_earth(Site::from_GPS(-99.1332 * Unit::deg, 19.4326 * Unit::deg, 2240.0 * Unit::m), eop);

    if (!obs) {
        std::cerr << "Error: Invalid observer site initialized.\n";
        return 1;
    }

    // 7. Initialize reduced accuracy frame and apparent location
    auto frame = obs.frame_at(t, NOVAS_REDUCED_ACCURACY);
    if (!frame) {
        std::cerr << "Error: Could not initialize observing frame.\n";
        return 1;
    }

    Apparent apparent = source.apparent_in(frame);
    if (!apparent) {
        std::cerr << "Error: Could not calculate apparent coordinates.\n";
        return 1;
    }

    // 8. Calculate refracted Alt-Az coordinates
    Weather weather(Temperature::celsius(15.0), Pressure::mbar(780.0), 50.0 * Unit::percent);
    Horizontal hor = apparent.to_horizontal().to_refracted(novas_optical_refraction, weather);

    if (!hor) {
        std::cerr << "Error: Failed to calculate horizontal coordinates.\n";
        return 1;
    }

    // 9. Print results
    std::cout << "Observation Time (UTC): " << t.to_string() << "\n";
    std::cout << "Star Catalog Name:      " << name << "\n";
    std::cout << "Apparent place:         " << apparent.equatorial().to_string() << "\n";
    std::cout << "Proper Motion (RA, Dec):" << pm_ra_arcsec << ", " << pm_dec_arcsec << " arcsec/yr\n";
    std::cout << "Parallax:               " << parallax_arcsec << " arcsec\n";
    std::cout << "Radial Velocity:        " << radvel_km_per_s << " km/s\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(5);
    std::cout << "Azimuth (East of North):\n";
    std::cout << "  DMS Format:     " << hor.azimuth().to_string() << "\n";
    std::cout << "  Decimal Deg:    " << hor.azimuth().deg() << "°\n\n";
    std::cout << "Elevation:\n";
    std::cout << "  DMS Format:     " << hor.elevation().to_string() << "\n";
    std::cout << "  Decimal Deg:    " << hor.elevation().deg() << "°\n";
    std::cout << "========================================================\n";

    return 0;
}
