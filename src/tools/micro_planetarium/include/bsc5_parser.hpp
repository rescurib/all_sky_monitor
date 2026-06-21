/**
 * @file bsc5_parser.hpp
 * @brief BSC5 Catalog parser library
 * 
 * This file defines the BSC5_Parser class which provides interfaces for loading,
 * querying, and extracting astrometric data from the Yale Bright Star Catalog (BSC5)
 * in JSON format.
 * 
 * @author Sky Monitor Project
 * @date 2026
 */

#ifndef BSC5_PARSER_HPP
#define BSC5_PARSER_HPP

#include <string>
#include <vector>
#include <json.hpp>
#include "star_t.hpp"

using json = nlohmann::json;

/**
 * @class BSC5_Parser
 * @brief Parser for the Yale Bright Star Catalog (BSC5) in JSON format
 * 
 * This class provides functionality to load, query, and extract astrometric
 * data from a BSC5 JSON database. It handles coordinate parsing, unit conversions,
 * and flexible star name matching (Flamsteed, Bayer, HD, HR, SAO, and common names).
 */
class BSC5_Parser {
private:
    /// @brief The loaded BSC5 catalog as a JSON array
    json catalog;
    
    /// @brief Indicates whether the catalog has been successfully loaded
    bool is_loaded;
    
    /**
     * @brief Case-insensitive substring search
     * @param str The string to search within
     * @param query The substring to search for (case-insensitive)
     * @return true if query is found in str, false otherwise
     */
    bool contains_ci(const std::string& str, const std::string& query) const;
    
    /**
     * @brief Clean coordinate strings, keeping only numeric, sign, and dot characters
     * @param str The coordinate string to clean
     * @return Cleaned coordinate string with non-numeric characters replaced by spaces
     */
    std::string clean_coord(const std::string& str) const;
    
    /**
     * @brief Safely extract a double from JSON field
     * 
     * Handles both numeric and string JSON types, with fallback to 0.0 on error.
     * For string fields, filters out non-numeric characters.
     * 
     * @param star The JSON star object
     * @param key The field name to extract
     * @return The extracted double value, or 0.0 if missing/invalid
     */
    double get_double_or_zero(const json& star, const std::string& key) const;
    
    /**
     * @brief Get a JSON field as a string regardless of its type
     * 
     * Converts numeric JSON types to strings with proper formatting.
     * 
     * @param j The JSON object
     * @param key The field name
     * @return String representation of the field, or empty string if missing
     */
    std::string get_field_string(const json& j, const std::string& key) const;
    
    /**
     * @brief Check if a star matches the given query string
     * 
     * Searches across multiple identifier types:
     * - Common names (Name field)
     * - Flamsteed/Bayer designations
     * - Henry Draper (HD) catalog numbers
     * - Bright Star (HR) catalog numbers
     * - Smithsonian Astrophysical Observatory (SAO) numbers
     * - Star name notes and aliases
     * 
     * All comparisons are case-insensitive.
     * 
     * @param star The JSON star object to match
     * @param query The search query string
     * @return true if the star matches the query, false otherwise
     */
    bool match_star(const json& star, const std::string& query) const;

public:
    /**
     * @brief Constructor
     * 
     * Creates an unloaded parser instance. Call loadCatalog() to load data.
     */
    BSC5_Parser();
    
    /**
     * @brief Load the BSC5 catalog from a JSON file
     * 
     * Attempts to load the catalog from the provided path. If the file cannot be
     * opened at the relative path, attempts to use an absolute fallback path.
     * 
     * @param json_path Path to the BSC5 JSON file (relative or absolute)
     * @return true if catalog loaded successfully, false otherwise
     */
    bool loadCatalog(const std::string& json_path);
    
    /**
     * @brief Retrieve astrometric data for a named star
     * 
     * Searches the catalog for a star matching the query string and returns
     * its astrometric data. The first match found is returned.
     * 
     * Coordinate format: "HH:MM:SS.SSS" for RA, "±DD:MM:SS.SSS" for Dec.
     * All SI units are used for physical quantities.
     * 
     * @param starName Query string for star identification
     * @return star_t structure with astrometric data if found
     * @throw std::runtime_error if star is not found or data is incomplete
     */
    star_t getAstrometrics(const std::string& starName);
    
    /**
     * @brief Query the catalog for multiple stars matching criteria
     * 
     * Returns all stars matching the query string.
     * 
     * @param query Search query string for star identification
     * @return Vector of star_t structures matching the query
     */
    std::vector<star_t> query(const std::string& query);
    
    /**
     * @brief Check if the catalog is loaded
     * @return true if catalog data is available, false otherwise
     */
    bool is_catalog_loaded() const { return is_loaded; }
    
    /**
     * @brief Get the number of stars in the loaded catalog
     * @return Number of stars, or 0 if catalog not loaded
     */
    size_t catalog_size() const { return is_loaded ? catalog.size() : 0; }
};

#endif // BSC5_PARSER_HPP
