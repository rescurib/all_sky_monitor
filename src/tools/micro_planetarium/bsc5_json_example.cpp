/**
 * bsc5_json_example.cpp
 *
 * Simple example showing how to use nlohmann::json (json.hpp) to read
 * the BSC5 star catalog, find Sirius, and list the 10 brightest stars.
 *
 * The code is written to be easy to follow for people familiar with C,
 * but who are learning readable modern C++ with clear steps.
 * 
 * Build with:
 *  g++ -std=c++17 -I../../common/inc -o bsc5_json_example bsc5_json_example.cpp
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <json.hpp>

using json = nlohmann::json;

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

// Convert a string to lowercase for case-insensitive comparison.
std::string to_lower(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

// Read the entire file into a json object.
// Returns an empty json object if reading or parsing fails.
json load_catalog(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file '" << path << "'\n";
        return json();
    }

    json catalog;
    try {
        file >> catalog;
    } catch (const std::exception& e) {
        std::cerr << "Error: failed to parse JSON: " << e.what() << "\n";
        return json();
    }

    return catalog;
}

// Get a string field from a json object safely.
// If the field is missing or not a string, return an empty string.
std::string get_string(const json& star_entry, const std::string& key) {
    if (!star_entry.contains(key)) {
        return std::string();
    }
    if (star_entry[key].is_string()) {
        return star_entry[key].get<std::string>();
    }
    if (star_entry[key].is_number()) {
        return std::to_string(star_entry[key].get<double>());
    }
    return std::string();
}

// Convert a BSC5 magnitude field from JSON to a double.
// If the field is missing or malformed, return a large positive value.
double get_double(const json& star_entry, const std::string& key) {
    if (!star_entry.contains(key)) {
        return 1e6;
    }
    if (star_entry[key].is_number()) {
        return star_entry[key].get<double>();
    }
    if (star_entry[key].is_string()) {
        std::string text = star_entry[key].get<std::string>();
        for (char& c : text) {
            if (c == '+') {
                c = ' ';
            }
        }
        try {
            return std::stod(text);
        } catch (...) {
            return 1e6;
        }
    }
    return 1e6;
}

// Case-insensitive search inside a string.
bool contains_ci(const std::string& haystack, const std::string& needle) {
    return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

// Match a user search query against common star fields.
// This search uses Name, HD, HR, DM, and Star names inside Notes.
bool star_matches_query(const json& star_entry, const std::string& query) {
    std::string q = to_lower(query);

    const std::vector<std::string> keys = {"Name", "HD", "HR", "DM"};
    for (const std::string& key : keys) {
        std::string value = get_string(star_entry, key);
        if (!value.empty() && contains_ci(value, q)) {
            return true;
        }
        // Search also with the numeric label prefix, such as "HD 48915".
        if (!value.empty()) {
            if (contains_ci(key + " " + value, q) || contains_ci(key + value, q)) {
                return true;
            }
        }
    }

    // Search the Notes array for star names or other aliases.
    if (star_entry.contains("Notes") && star_entry["Notes"].is_array()) {
        for (const auto& note : star_entry["Notes"]) {
            if (note.contains("Category") && note.contains("Remark") &&
                note["Category"].is_string() && note["Remark"].is_string()) {
                std::string category = note["Category"].get<std::string>();
                std::string remark = note["Remark"].get<std::string>();
                if (contains_ci(category, "star names") && contains_ci(remark, q)) {
                    return true;
                }
            }
        }
    }

    return false;
}

// Helper to keep only digits, sign, decimal points, and spaces.
// This is useful for parsing RA and Dec text strings.
std::string normalize_coordinate_text(const std::string& text) {
    std::string result;
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.' || c == ' ') {
            result += c;
        } else {
            result += ' ';
        }
    }
    return result;
}

// Convert RA string like "06h 45m 08.9s" into decimal degrees.
// This function is not perfect for every format, but it works for the BSC5 data.
double parse_ra_degrees(const std::string& ra_text) {
    std::string normalized = normalize_coordinate_text(ra_text);
    std::istringstream stream(normalized);
    double hours = 0.0;
    double minutes = 0.0;
    double seconds = 0.0;

    stream >> hours;
    stream >> minutes;
    stream >> seconds;

    return (hours + minutes / 60.0 + seconds / 3600.0) * 15.0;
}

// Convert Dec string like "+16° 30′ 33\"" into decimal degrees.
double parse_dec_degrees(const std::string& dec_text) {
    std::string normalized = normalize_coordinate_text(dec_text);
    std::istringstream stream(normalized);
    double sign = 1.0;
    double degrees = 0.0;
    double minutes = 0.0;
    double seconds = 0.0;

    stream >> degrees;
    stream >> minutes;
    stream >> seconds;

    if (!dec_text.empty() && dec_text[0] == '-') {
        sign = -1.0;
    }

    return sign * (std::abs(degrees) + minutes / 60.0 + seconds / 3600.0);
}

// Print the most important catalog values for one star.
void print_star_summary(const json& star) {
    std::string name     = get_string(star, "Name");
    std::string hd       = get_string(star, "HD");
    std::string hr       = get_string(star, "HR");
    std::string ra       = get_string(star, "RA");
    std::string dec      = get_string(star, "Dec");
    std::string spectral = get_string(star, "SpectralCls");
    std::string vmag     = get_string(star, "Vmag");

    std::cout << "  Name: " << (name.empty() ? "<none>" : name) << "\n";
    std::cout << "  HD: " << (hd.empty() ? "<none>" : hd)
              << "  HR: " << (hr.empty() ? "<none>" : hr) << "\n";
    std::cout << "  RA: " << (ra.empty() ? "<none>" : ra)
              << "  Dec: " << (dec.empty() ? "<none>" : dec) << "\n";
    std::cout << "  Spectral type: " << (spectral.empty() ? "<none>" : spectral) << "\n";
    std::cout << "  V magnitude: " << (vmag.empty() ? "<none>" : vmag) << "\n";

    if (!ra.empty() && !dec.empty()) {
        double ra_deg  = parse_ra_degrees(ra);
        double dec_deg = parse_dec_degrees(dec);
        std::cout << "  RA  (decimal degrees): " << ra_deg << "\n";
        std::cout << "  Dec (decimal degrees): " << dec_deg << "\n";
    }
}

int main() {

    // Load YBSC5 catalog
    const std::string catalog_path = "../../common/data/bsc5.json";
    json catalog = load_catalog(catalog_path);

    // =========================================================================
    // Part 1: Find Sirius in the catalog.
    // =========================================================================
    const std::string search_name = "Sirius";
    int found_index = -1;

    for (size_t i = 0; i < catalog.size(); ++i) 
    {
        if (star_matches_query(catalog[i], search_name)) 
        {
            found_index = static_cast<int>(i);
            break;
        }
    }

    std::cout << "=== Search for '" << search_name << "' ===\n";
    if (found_index < 0) 
    {
        std::cout << "Sirius was not found in the catalog.\n";
    } else 
      {
        print_star_summary(catalog[found_index]);
      }
    std::cout << "\n";

    // =========================================================================
    // Part 2: Find the 10 brightest stars in the catalog.
    // =========================================================================
    std::vector<size_t> indices;
    indices.reserve(catalog.size());

    for (size_t i = 0; i < catalog.size(); ++i) {
        indices.push_back(i);
    }

    /* std::sort with a lambda comparator:
     - std::sort(first, last, comparator) sorts elements in the range [first, last)
     - The comparator is a lambda: [](type a, type b) { return ...; }
     - [&] means: capture all variables of the scope by reference (e.g., catalog, get_double)
     - It takes TWO elements and returns true if 'a' should come BEFORE 'b' in the sorted order
     - std::sort calls this lambda repeatedly to determine the ordering (uses a hybrid algorithm: quicksort + heapsort + insertion sort)
    */
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        double mag_a = get_double(catalog[a], "Vmag");
        double mag_b = get_double(catalog[b], "Vmag");
        return mag_a < mag_b;
    });

    std::cout << "=== 10 brightest stars in the catalog ===\n";
    for (int rank = 0; rank < 10 && rank < static_cast<int>(indices.size()); ++rank) {
        const json& star     = catalog[indices[rank]];
        std::string name     = get_string(star, "Name");
        std::string hd       = get_string(star, "HD");
        std::string hr       = get_string(star, "HR");
        std::string vmag     = get_string(star, "Vmag");
        std::string spectral = get_string(star, "SpectralCls");
        std::string ra       = get_string(star, "RA");
        std::string dec      = get_string(star, "Dec");

        std::cout << rank + 1 << ") ";
        if (!name.empty()) {
            std::cout << name;
        } else if (!hd.empty()) {
            std::cout << "HD " << hd;
        } else if (!hr.empty()) {
            std::cout << "HR " << hr;
        } else {
            std::cout << "<unknown star>";
        }

        std::cout << "  Vmag=" << (vmag.empty() ? "<missing>" : vmag)
                  << "  Spectral=" << (spectral.empty() ? "<missing>" : spectral) << "\n";
        std::cout << "  RA=" << (ra.empty() ? "<missing>" : ra)
                  << "  Dec=" << (dec.empty() ? "<missing>" : dec) << "\n";
    }

    return 0;
}
