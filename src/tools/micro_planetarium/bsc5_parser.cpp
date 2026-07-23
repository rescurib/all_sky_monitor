/**
 * @file bsc5_parser.cpp
 * @brief Implementation of the BSC5 catalog parser
 * 
 * @author Sky Monitor Project
 * @date 2026
 */

#include "bsc5_parser.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

/**
 * @brief Constructor - initializes an empty, unloaded parser
 */
BSC5_Parser::BSC5_Parser() : is_loaded(false) {}

/**
 * @brief Case-insensitive substring search
 */
bool BSC5_Parser::contains_ci(const std::string& str, const std::string& query) const {
    if (query.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        query.begin(), query.end(),
        [](unsigned char ch1, unsigned char ch2) { 
            return std::tolower(ch1) == std::tolower(ch2); 
        }
    );
    return it != str.end();
}

/**
 * @brief Clean coordinate strings, keeping only numeric, sign, and dot characters
 */
std::string BSC5_Parser::clean_coord(const std::string& str) const {
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

/**
 * @brief Safely extract a double from JSON field
 */
double BSC5_Parser::get_double_or_zero(const json& star, const std::string& key) const {
    if (!star.contains(key)) return 0.0;
    
    // If numeric in JSON, return directly
    if (star[key].is_number()) {
        try { 
            return star[key].get<double>(); 
        } catch (...) { 
            return 0.0; 
        }
    }
    
    // If string, parse it
    if (star[key].is_string()) {
        std::string val_str = star[key].get<std::string>();
        std::string filtered;
        for (char c : val_str) {
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || 
                c == '+' || c == '-' || c == 'e' || c == 'E') {
                filtered += c;
            }
        }
        if (!filtered.empty()) {
            try { 
                return std::stod(filtered); 
            } catch (...) { }
        }
    }
    return 0.0;
}

/**
 * @brief Get a JSON field as a string regardless of its type
 */
std::string BSC5_Parser::get_field_string(const json& j, const std::string& key) const {
    if (!j.contains(key)) return std::string();
    
    if (j[key].is_string()) {
        return j[key].get<std::string>();
    }
    
    if (j[key].is_number()) {
        try {
            double v = j[key].get<double>();
            std::ostringstream oss; 
            oss << std::fixed << v; 
            std::string s = oss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        } catch (...) { 
            return std::string(); 
        }
    }
    return std::string();
}

/**
 * @brief Check if a star matches the given query string
 */
bool BSC5_Parser::match_star(const json& star, const std::string& query) const {
    // Search in common name
    if (star.contains("Name") && star["Name"].is_string()) {
        if (contains_ci(star["Name"].get<std::string>(), query)) return true;
    }
    
    // Search in HD number
    std::string hd = get_field_string(star, "HD");
    if (!hd.empty()) {
        if (contains_ci(hd, query) || contains_ci("HD " + hd, query) || 
            contains_ci("HD" + hd, query)) {
            return true;
        }
    }
    
    // Search in HR number
    std::string hr = get_field_string(star, "HR");
    if (!hr.empty()) {
        if (contains_ci(hr, query) || contains_ci("HR " + hr, query) || 
            contains_ci("HR" + hr, query)) {
            return true;
        }
    }
    
    // Search in SAO number
    std::string sao = get_field_string(star, "SAO");
    if (!sao.empty()) {
        if (contains_ci(sao, query) || contains_ci("SAO " + sao, query) || 
            contains_ci("SAO" + sao, query)) {
            return true;
        }
    }
    
    // Search in star name notes (explicit aliases)
    if (star.contains("Notes") && star["Notes"].is_array()) {
        for (const auto& note : star["Notes"]) {
            if (!note.contains("Category") || !note["Category"].is_string()) continue;
            if (!note.contains("Remark") || !note["Remark"].is_string()) continue;
            
            std::string category = note["Category"].get<std::string>();
            std::string remark = note["Remark"].get<std::string>();
            
            // Only use explicit star-name aliases from the "Star names" category
            if (category == "Star names" && contains_ci(remark, query)) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief Load the BSC5 catalog from a JSON file
 */
bool BSC5_Parser::loadCatalog(const std::string& json_path) {
    std::ifstream f(json_path);
    
    if (!f.is_open()) {
        // Try absolute fallback path
        std::string fallback_path = 
            "/home/rodolfo/Proyectos/Sky_Monitor/Repositorios/all_sky_monitor/src/common/data/bsc5.json";
        f.open(fallback_path);
        if (!f.is_open()) {
            return false;
        }
    }
    
    try {
        f >> catalog;
        is_loaded = true;
        return true;
    } catch (const std::exception& e) {
        is_loaded = false;
        return false;
    }
}

/**
 * @brief Retrieve astrometric data for a named star
 */
star_t BSC5_Parser::getAstrometrics(const std::string& starName) {
    if (!is_loaded) {
        throw std::runtime_error("Catalog not loaded. Call loadCatalog() first.");
    }
    
    // Search for matching star
    for (const auto& star : catalog) {
        if (!match_star(star, starName)) continue;
        
        // Extract name
        std::string name = star.contains("Name") && star["Name"].is_string() 
                          ? star["Name"].get<std::string>() 
                          : starName;
        
        // Extract coordinates
        std::string ra_str = star.contains("RA") && star["RA"].is_string() 
                            ? star["RA"].get<std::string>() 
                            : "";
        std::string dec_str = star.contains("Dec") && star["Dec"].is_string() 
                             ? star["Dec"].get<std::string>() 
                             : "";
        
        if (ra_str.empty() || dec_str.empty()) {
            throw std::runtime_error("Coordinates missing for star: " + name);
        }
        
        // Extract and convert proper motion (arcsec/yr -> rad/s)
        // Note: This assumes the arcsec/yr values need to be converted to rad/s
        // You'll need to include the Unit library for actual conversion
        double pm_ra_arcsec  = get_double_or_zero(star, "pmRA");
        double pm_dec_arcsec = get_double_or_zero(star, "pmDE");
        
        // Conversion factors (approximate, assuming standard constants from supernovas lib)
        const double arcsec_to_rad = 4.84814e-6;  // 1 arcsec in radians
        const double yr_to_s = 31557600.0;        // 1 year in seconds
        
        double pm_ra_si  = pm_ra_arcsec * arcsec_to_rad / yr_to_s;
        double pm_dec_si = pm_dec_arcsec * arcsec_to_rad / yr_to_s;
        
        // Extract and convert parallax (arcsec -> rad)
        double parallax_arcsec = get_double_or_zero(star, "Parallax");
        double parallax_si = parallax_arcsec * arcsec_to_rad;
        
        // Extract and convert radial velocity (km/s -> m/s)
        double radvel_km_per_s = get_double_or_zero(star, "RadVel");
        double radvel_si = radvel_km_per_s * 1000.0;  // km/s to m/s
        
        double magnitude = get_double_or_zero(star, "Vmag");
        
        return star_t(name, ra_str, dec_str, pm_ra_si, pm_dec_si, parallax_si, radvel_si, magnitude);
    }
    
    throw std::runtime_error("Star matching \"" + starName + "\" not found in catalog.");
}

/**
 * @brief Query the catalog for multiple stars matching criteria
 */
std::vector<star_t> BSC5_Parser::query(const std::string& query) {
    std::vector<star_t> results;
    
    if (!is_loaded) {
        return results;  // Return empty vector if not loaded
    }
    
    for (const auto& star : catalog) {
        if (!match_star(star, query)) continue;
        
        try {
            // Extract name
            std::string name = star.contains("Name") && star["Name"].is_string() 
                              ? star["Name"].get<std::string>() 
                              : "";
            if (name.empty()) {
                std::string hd = get_field_string(star, "HD");
                std::string hr = get_field_string(star, "HR");
                if (!hd.empty()) name = "HD " + hd;
                else if (!hr.empty()) name = "HR " + hr;
                else name = "Unknown";
            }
            
            // Extract coordinates
            std::string ra_str = star.contains("RA") && star["RA"].is_string() 
                                ? star["RA"].get<std::string>() 
                                : "";
            std::string dec_str = star.contains("Dec") && star["Dec"].is_string() 
                                 ? star["Dec"].get<std::string>() 
                                 : "";
            
            if (ra_str.empty() || dec_str.empty()) continue;
            
            // Extract and convert values
            double pm_ra_arcsec  = get_double_or_zero(star, "pmRA");
            double pm_dec_arcsec = get_double_or_zero(star, "pmDE");
            
            const double arcsec_to_rad = 4.84814e-6;
            const double yr_to_s = 31557600.0;
            
            double pm_ra_si  = pm_ra_arcsec * arcsec_to_rad / yr_to_s;
            double pm_dec_si = pm_dec_arcsec * arcsec_to_rad / yr_to_s;
            
            double parallax_arcsec = get_double_or_zero(star, "Parallax");
            double parallax_si = parallax_arcsec * arcsec_to_rad;
            
            double radvel_km_per_s = get_double_or_zero(star, "RadVel");
            double radvel_si = radvel_km_per_s * 1000.0;
            
            double magnitude = get_double_or_zero(star, "Vmag");
            
            results.push_back(star_t(name, ra_str, dec_str, pm_ra_si, pm_dec_si, 
                                     parallax_si, radvel_si, magnitude));
        } catch (...) {
            // Skip stars with incomplete data
            continue;
        }
    }
    
    return results;
}
