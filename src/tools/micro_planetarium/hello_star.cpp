/**
 * @file hello_star.cpp
 *
 * Simple program that prints the alt-az coordinates of Sirius
 * at the current time in Mexico City using SuperNOVAS.
 */

#include <iostream>
#include <iomanip>
#include <supernovas.h>

using namespace supernovas;

// Define Earth Orientation Parameters (EOP) for robust offline usage.
// Standard/recent EOP values:
#define LEAP_SECONDS     37        // [s] current leap seconds
#define DUT1             0.114     // [s] UT1 - UTC time difference
#define POLAR_DX         230.0     // [mas] polar offset x
#define POLAR_DY         -62.0     // [mas] polar offset y

int main() {
    // Enable error tracing and debugging messages
    novas_debug(NOVAS_DEBUG_ON);

    std::cout << "========================================================\n";
    std::cout << "          Sirius Alt-Az Monitor (Mexico City)           \n";
    std::cout << "========================================================\n\n";

    /**********************************************/
    /*  1. Setup EOP and current observation time */
    /**********************************************/

    EOP eop(LEAP_SECONDS, /* handles the variance between atomic time (TAI) and UTC*/
            DUT1, /* the difference between Universal Time (UT1) and Coordinated Universal Time (UTC) */
            POLAR_DX * Unit::mas,  /* IERS xp mean (interpolated) pole offset in the ITRS X direction */
            POLAR_DY * Unit::mas); /* IERS yp mean (interpolated) pole offset in the ITRS y direction */

    Time t = Time::now(eop);

    // Print current time
    std::cout << "Observation Time (UTC): " << t.to_string() << "\n";

    /**********************************************/
    /*  2. Define Sirius (Alp CMa) catalog entry  */
    /**********************************************/

    // Astrometric data for J2000 epoch
    // RA: 06h 45m 08.9173s, Dec: -16d 42m 58.017s
    auto entry = CatalogEntry("Sirius", Equatorial("06h 45m 08.9173s", "-16d 42m 58.017s", Equinox::j2000()))
            .proper_motion(-546.05  * Unit::mas / Unit::yr, /*RA*/
                           -1223.14 * Unit::mas / Unit::yr) /*Dec*/
            .parallax(379.21 * Unit::mas)
            .radial_velocity(-7.6 * Unit::km_per_s);

    if (!entry) {
        std::cerr << "Error: Invalid catalog entry created for Sirius.\n";
        return 1;
    }

    /*A CatalogSource is an optimized object prepared to 
      interact directly with the astrometric calculation core.*/
    auto source = entry.to_source();

    /***********************************************/
    /* 3. Define observer location for Mexico City */
    /***********************************************/

    // Latitude: 19.4326 N, Longitude: 99.1332 W (represented as -99.1332 E)
    // Elevation: 2240 m
    auto obs = Observer::on_earth(Site::from_GPS(-99.1332 * Unit::deg,
                                                  19.4326 * Unit::deg, 
                                                  2240.0 * Unit::m), 
                                                  eop);
                                                  
    /**********************************************/
    /*  4. Initialize observing frame             */
    /**********************************************/

    /*frame_at() calculates the Earth's orientation matrices—accounting for precession, 
      nutation, spin, and polar motion—at that exact instant from the observer's perspective.*/
    auto frame = obs.frame_at(t, NOVAS_REDUCED_ACCURACY); // (milliarcsecond level, offline)

    /*******************************************/
    /*  5. Calculate apparent position         */
    /*******************************************/

    Apparent apparent = source.apparent_in(frame);
    if (!apparent) {
        std::cerr << "Error: Could not calculate apparent coordinates.\n";
        return 1;
    }

    /**********************************************/
    /*  6. Calculate refracted Alt-Az coordinates */
    /**********************************************/

    // Using standard weather conditions for Mexico City:
    // Temperature: 15 C, Pressure: 780 mbar (typical for ~2240m altitude), Humidity: 50%
    Weather weather(Temperature::celsius(15.0), Pressure::mbar(780.0), 50.0 * Unit::percent);
    Horizontal hor = apparent.to_horizontal().to_refracted(novas_optical_refraction, weather);

    /**********************************************/
    /*  7. Print the results nicely               */
    /**********************************************/

    std::cout << "Star Name: " << source.name() << "\n";
    std::cout << "Apparent place: " << apparent.equatorial().to_string() << "\n";
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
