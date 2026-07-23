/**
 * @file hello_bsc5.cpp
 * @brief Example program computing alt-az coordinates using BSC5 catalog parser
 *
 * This example program demonstrates the usage of the BSC5_Parser library.
 * It queries the Yale Bright Star Catalog (BSC5) for a star and computes
 * its horizontal (alt-az) coordinates at the current time in Mexico City.
 * 
 * The program showcases:
 * - Loading the BSC5 catalog using BSC5_Parser
 * - Retrieving astrometric data with unit conversions
 * - Computing apparent place and horizontal coordinates using supernovas library
 * - Atmospheric refraction corrections
 * 
 * @author Sky Monitor Project
 * @date 2026
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <supernovas.h>
#include "include/bsc5_parser.hpp"

using namespace supernovas;

/**
 * @brief Define Earth Orientation Parameters (EOP) for robust offline usage
 * @{
 */
#define LEAP_SECONDS     37        ///< [s] current leap seconds
#define DUT1             0.114     ///< [s] UT1 - UTC time difference
#define POLAR_DX         230.0     ///< [mas] polar offset x
#define POLAR_DY         -62.0     ///< [mas] polar offset y
/// @}

/**
 * @brief Main program - demonstrates BSC5_Parser usage
 * 
 * This program:
 * 1. Parses command-line arguments (default star: "Sirius")
 * 2. Loads the BSC5 catalog using BSC5_Parser
 * 3. Retrieves astrometric data for the queried star
 * 4. Computes horizontal coordinates at current time in Mexico City
 * 5. Displays results with atmospheric refraction corrections
 * 
 * @param argc Number of command-line arguments
 * @param argv Command-line arguments: [program] [star_name]
 * @return 0 on success, 1 on error
 */
int main(int argc, char* argv[]) {
    // Get query string from command line argument, default to "Sirius"
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

    // Initialize parser and load catalog
    BSC5_Parser parser;
    std::string json_path = "../../common/data/bsc5.json";
    
    if (!parser.loadCatalog(json_path)) {
        std::cerr << "Error: Could not load bsc5.json catalog.\n";
        return 1;
    }

    // Retrieve star astrometrics
    star_t star;
    try {
        star = parser.getAstrometrics(query);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Instantiate CatalogEntry with parsed data
    auto entry = CatalogEntry(star.name, Equatorial(star.ra_str, star.dec_str, Equinox::j2000()))
            .proper_motion(star.pm_ra_si, star.pm_dec_si)
            .parallax(Angle(star.parallax_si))
            .radial_velocity(ScalarVelocity(star.radvel_si));

    if (!entry) {
        std::cerr << "Error: Invalid catalog entry created for star: " << star.name << ".\n";
        return 1;
    }
    
    /// @brief CatalogSource is an optimized object for astrometric calculations
    auto source = entry.to_source();

    // Setup Earth Orientation Parameters and observer location
    EOP eop(LEAP_SECONDS,
            DUT1,
            POLAR_DX * Unit::mas,
            POLAR_DY * Unit::mas);
   
    Time t = Time::now(eop);
    
    /// @brief Observer at Mexico City (approximate location)
    auto obs = Observer::on_earth(Site::from_GPS(-99.1332 * Unit::deg,
                                                  19.4326 * Unit::deg,
                                                  2240.0  * Unit::m),
                                                  eop);

    // Compute frame and apparent location
    /// @brief frame_at() calculates Earth's orientation matrices
    auto frame = obs.frame_at(t, NOVAS_REDUCED_ACCURACY);

    /// @brief apparent_in() transforms coordinates to apparent place
    Apparent apparent = source.apparent_in(frame);
  
    // Calculate refracted Alt-Az coordinates
    Weather    weather(Temperature::celsius(15.0), Pressure::mbar(780.0), 50.0 * Unit::percent);
    Horizontal hor = apparent.to_horizontal().to_refracted(novas_optical_refraction, weather);

    // Print results
    std::cout << "Observation Time (UTC): " << t.to_string() << "\n";
    std::cout << "Star Catalog Name:      " << star.name << "\n";
    std::cout << "Apparent place:         " << apparent.equatorial().to_string() << "\n";
    std::cout << "Proper Motion (RA, Dec):" 
              << (star.pm_ra_si * 31557600.0 / 4.84814e-6) << ", " 
              << (star.pm_dec_si * 31557600.0 / 4.84814e-6) << " arcsec/yr\n";
    std::cout << "Parallax:               " << (star.parallax_si / 4.84814e-6) << " arcsec\n";
    std::cout << "Radial Velocity:        " << (star.radvel_si / 1000.0) << " km/s\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(5);
    std::cout << "Azimuth (East of North):\n";
    std::cout << "  DMS Format:   " << hor.azimuth().to_string() << "\n";
    std::cout << "  Decimal Deg:    " << hor.azimuth().deg() << "°\n\n";
    std::cout << "Elevation:\n";
    std::cout << "  DMS Format:   " << hor.elevation().to_string() << "\n";
    std::cout << "  Decimal Deg:    " << hor.elevation().deg() << "°\n";
    std::cout << "========================================================\n";

    /* NOTE about star naming conventions. For Sirius, the BSC5 catalog lists:
     * Name: "9Alp CMa"
     * 9 (Flamsteed Designation): This is the leading number. It tells you that Sirius is the 9th star in 
     * the constellation Canis Major when moving from west to east (ordered by increasing Right Ascension).
     * Alp (Bayer Designation): Short for Alpha (α). This indicates that Sirius is typically the brightest 
     * star in that constellation.
     * CMa (Constellation): The standard IAU 3-letter abbreviation for Canis Major (The Greater Dog).
    */

    return 0;
}
