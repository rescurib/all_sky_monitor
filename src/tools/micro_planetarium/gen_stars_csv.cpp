#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <supernovas.h>
#include "include/bsc5_parser.hpp"

using namespace supernovas;

#define LEAP_SECONDS 37
#define DUT1 0.114
#define POLAR_DX 230.0
#define POLAR_DY -62.0

struct OutputStar {
    std::string name;
    std::string constellation;
    double magnitude;
    double altitude_deg;
    double azimuth_deg;
};

std::string get_constellation(const std::string& name) {
    if (name.empty()) return "Unnamed";
    size_t last_space = name.find_last_of(" \t");
    std::string last_word = (last_space == std::string::npos) ? name : name.substr(last_space + 1);
    if (last_word.length() == 3 && std::isalpha(static_cast<unsigned char>(last_word[0])) &&
        std::isalpha(static_cast<unsigned char>(last_word[1])) &&
        std::isalpha(static_cast<unsigned char>(last_word[2]))) {
        return last_word;
    }
    if (name.length() >= 3) {
        std::string last_3 = name.substr(name.length() - 3);
        if (std::isalpha(static_cast<unsigned char>(last_3[0])) &&
            std::isalpha(static_cast<unsigned char>(last_3[1])) &&
            std::isalpha(static_cast<unsigned char>(last_3[2]))) {
            return last_3;
        }
    }
    return "Unnamed";
}

std::string csv_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            out.push_back('"');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

std::filesystem::path find_catalog_path() {
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("../../common/data/bsc5.json"),
        std::filesystem::path("../common/data/bsc5.json"),
        std::filesystem::path("src/common/data/bsc5.json"),
        std::filesystem::path("common/data/bsc5.json")
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::filesystem::path("../../common/data/bsc5.json");
}

int main(int argc, char** argv) {
    const std::string output_path = (argc > 1) ? argv[1] : "visible_stars_mag3.csv";

    BSC5_Parser parser;
    const auto catalog_path = find_catalog_path();
    if (!parser.loadCatalog(catalog_path.string())) {
        std::cerr << "Error: Could not load catalog from " << catalog_path << std::endl;
        return 1;
    }

    std::vector<star_t> raw_stars = parser.query("");
    std::cout << "Loaded " << raw_stars.size() << " stars from the catalog." << std::endl;

    EOP eop(LEAP_SECONDS, DUT1, POLAR_DX * Unit::mas, POLAR_DY * Unit::mas);
    Time t = Time::now(eop);

    // Tepoztlan, Mexico coordinates
    const double longitude = -99.10119625932298;
    const double latitude = 18.986001321740556;
    const double elevation_m = 1450.0;

    auto obs = Observer::on_earth(
        Site::from_GPS(longitude * Unit::deg, latitude * Unit::deg, elevation_m * Unit::m),
        eop);
    auto frame = obs.frame_at(t, NOVAS_REDUCED_ACCURACY);

    Weather weather(Temperature::celsius(15.0), Pressure::mbar(780.0), 50.0 * Unit::percent);

    std::vector<OutputStar> visible_stars;
    visible_stars.reserve(raw_stars.size());

    for (const auto& star : raw_stars) {
        if (star.magnitude > 3.0) {
            continue;
        }

        try {
            auto entry = CatalogEntry(star.name, Equatorial(star.ra_str, star.dec_str, Equinox::j2000()))
                             .proper_motion(star.pm_ra_si, star.pm_dec_si)
                             .parallax(Angle(star.parallax_si))
                             .radial_velocity(ScalarVelocity(star.radvel_si));
            if (!entry) {
                continue;
            }

            auto source = entry.to_source();
            Apparent apparent = source.apparent_in(frame);
            if (!apparent) {
                continue;
            }

            auto hor = apparent.to_horizontal().to_refracted(novas_optical_refraction, weather);
            const double altitude = hor.elevation().deg();
            if (altitude <= 0.0) {
                continue;
            }

            visible_stars.push_back({
                star.name,
                get_constellation(star.name),
                star.magnitude,
                altitude,
                hor.azimuth().deg()
            });
        } catch (...) {
            continue;
        }
    }

    std::sort(visible_stars.begin(), visible_stars.end(), [](const OutputStar& a, const OutputStar& b) {
        if (a.magnitude != b.magnitude) {
            return a.magnitude < b.magnitude;
        }
        if (a.altitude_deg != b.altitude_deg) {
            return a.altitude_deg > b.altitude_deg;
        }
        return a.name < b.name;
    });

    std::ofstream out(output_path);
    if (!out) {
        std::cerr << "Error: Could not open output file " << output_path << std::endl;
        return 1;
    }

    out << "name,constellation,magnitude,altitude_deg,azimuth_deg\n";
    for (const auto& star : visible_stars) {
        out << csv_escape(star.name) << ','
            << csv_escape(star.constellation) << ','
            << std::fixed << std::setprecision(3) << star.magnitude << ','
            << std::fixed << std::setprecision(3) << star.altitude_deg << ','
            << std::fixed << std::setprecision(3) << star.azimuth_deg << '\n';
    }

    out.flush();
    std::cout << "Wrote " << visible_stars.size() << " visible stars to " << output_path
              << " for observation time " << t.to_string() << "." << std::endl;
    return 0;
}
