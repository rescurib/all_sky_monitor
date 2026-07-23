/**
 * @file star_t.hpp
 * @brief Star data structure for astrometric information
 * 
 * This file defines the star_t structure that holds astrometric and kinematic
 * data extracted from the Yale Bright Star Catalog (BSC5).
 * 
 * @author Sky Monitor Project
 * @date 2026
 */

#ifndef STAR_T_HPP
#define STAR_T_HPP

#include <string>

/**
 * @struct star_t
 * @brief Contains astrometric and kinematic data for a catalog star
 * 
 * This structure holds all the essential information needed to perform
 * astrometric calculations for a star, including coordinates, proper motion,
 * parallax, and radial velocity.
 * 
 * @note All SI units are used for physical quantities:
 * - Angular quantities: radians
 * - Velocity: meters per second or radians per second
 * - Distance: meters (derived from parallax)
 */
struct star_t {
    /// @brief Star name/designation from catalog
    std::string name;
    
    /// @brief Right Ascension in string format (HH:MM:SS.SSS)
    std::string ra_str;
    
    /// @brief Declination in string format (±DD:MM:SS.SSS)
    std::string dec_str;
    
    /// @brief Proper motion in RA (rad/s), includes cos(dec) factor if applicable
    double pm_ra_si;
    
    /// @brief Proper motion in Dec (rad/s)
    double pm_dec_si;
    
    /// @brief Parallax (radians)
    double parallax_si;
    
    /// @brief Radial velocity (m/s), positive for receding objects
    double radvel_si;
    
    /// @brief Visual magnitude (Vmag)
    double magnitude;
    
    /**
     * @brief Default constructor initializing all fields to zero/empty
     */
    star_t() 
        : name(""), ra_str(""), dec_str(""),
          pm_ra_si(0.0), pm_dec_si(0.0),
          parallax_si(0.0), radvel_si(0.0), magnitude(0.0) {}
    
    /**
     * @brief Construct a star_t with all parameters
     * @param n Star name
     * @param ra Right Ascension string
     * @param dec Declination string
     * @param pm_ra Proper motion RA (rad/s)
     * @param pm_dec Proper motion Dec (rad/s)
     * @param parallax Parallax (rad)
     * @param radvel Radial velocity (m/s)
     * @param mag Visual magnitude
     */
    star_t(const std::string& n, const std::string& ra, const std::string& dec,
            double pm_ra, double pm_dec, double parallax, double radvel, double mag = 0.0)
        : name(n), ra_str(ra), dec_str(dec),
          pm_ra_si(pm_ra), pm_dec_si(pm_dec),
          parallax_si(parallax), radvel_si(radvel), magnitude(mag) {}
};

#endif // STAR_T_HPP
