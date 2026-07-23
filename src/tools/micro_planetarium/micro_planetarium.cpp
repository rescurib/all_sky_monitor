#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <memory>
#include <cctype>

#include <supernovas.h>
#include "include/bsc5_parser.hpp"
#include "cpptui.hpp"

using namespace supernovas;
using namespace cpptui;

// Earth Orientation Parameters (EOP) for robust offline usage
#define LEAP_SECONDS     37        ///< [s] current leap seconds
#define DUT1             0.114     ///< [s] UT1 - UTC time difference
#define POLAR_DX         230.0     ///< [mas] polar offset x
#define POLAR_DY         -62.0     ///< [mas] polar offset y

struct City {
    std::string name;
    double longitude; // degrees, West is negative
    double latitude;  // degrees, North is positive
    double elevation;  // meters
};

const std::vector<City> cities = {
    {"Mexico City", -99.1332, 19.4326, 2240.0},
    {"London",      -0.1278,  51.5074, 11.0},
    {"New York",    -74.0060, 40.7128, 10.0}
};

const std::vector<double> max_mags = {1.0, 3.0, 6.0};

// IAU 3-letter abbreviation to full English name map for constellations
const std::unordered_map<std::string, std::string> constellation_names = {
    {"And", "Andromeda"}, {"Ant", "Antlia"}, {"Aps", "Apus"}, {"Aql", "Aquila"},
    {"Aqr", "Aquarius"}, {"Ara", "Ara"}, {"Ari", "Aries"}, {"Aur", "Auriga"},
    {"Boo", "Boötes"}, {"Cae", "Caelum"}, {"Cam", "Camelopardalis"}, {"Cnc", "Cancer"},
    {"CVn", "Canes Venatici"}, {"CMa", "Canis Major"}, {"CMi", "Canis Minor"}, {"Cap", "Capricornus"},
    {"Car", "Carina"}, {"Cas", "Cassiopeia"}, {"Cen", "Centaurus"}, {"Cep", "Cepheus"},
    {"Cet", "Cetus"}, {"Cha", "Chamaeleon"}, {"Cir", "Circinus"}, {"Col", "Columba"},
    {"Com", "Coma Berenices"}, {"CrA", "Corona Australis"}, {"CrB", "Corona Borealis"}, {"Crt", "Crater"},
    {"Cru", "Crux"}, {"Crv", "Corvus"}, {"Cyg", "Cygnus"}, {"Del", "Delphinus"},
    {"Dor", "Dorado"}, {"Dra", "Draco"}, {"Equ", "Equuleus"}, {"Eri", "Eridanus"},
    {"For", "Fornax"}, {"Gem", "Gemini"}, {"Gru", "Grus"}, {"Her", "Hercules"},
    {"Hor", "Horologium"}, {"Hya", "Hydra"}, {"Hyi", "Hydrus"}, {"Ind", "Indus"},
    {"Lac", "Lacerta"}, {"Leo", "Leo"}, {"LMi", "Leo Minor"}, {"Lep", "Lepus"},
    {"Lib", "Libra"}, {"Lup", "Lupus"}, {"Lyn", "Lynx"}, {"Lyr", "Lyra"},
    {"Men", "Mensa"}, {"Mic", "Microscopium"}, {"Mon", "Monoceros"}, {"Mus", "Musca"},
    {"Nor", "Norma"}, {"Oct", "Octans"}, {"Oph", "Ophiuchus"}, {"Ori", "Orion"},
    {"Pav", "Pavo"}, {"Peg", "Pegasus"}, {"Per", "Perseus"}, {"Phe", "Phoenix"},
    {"Pic", "Pictor"}, {"Psc", "Pisces"}, {"PsA", "Piscis Austrinus"}, {"Pup", "Puppis"},
    {"Pyx", "Pyxis"}, {"Ret", "Reticulum"}, {"Sge", "Sagitta"}, {"Sgr", "Sagittarius"},
    {"Sco", "Scorpius"}, {"Scl", "Sculptor"}, {"Sct", "Scutum"}, {"Ser", "Serpens"},
    {"Sex", "Sextans"}, {"Tau", "Taurus"}, {"Tel", "Telescopium"}, {"Tri", "Triangulum"},
    {"TrA", "Triangulum Australe"}, {"Tuc", "Tucana"}, {"UMa", "Ursa Major"}, {"UMi", "Ursa Minor"},
    {"Vel", "Vela"}, {"Vir", "Virgo"}, {"Vol", "Volans"}, {"Vul", "Vulpecula"}
};

// Represents parsed star data ready for astrometry calculations
struct StarData {
    std::string name;
    double magnitude;
    std::string constellation;
    std::shared_ptr<CatalogSource> source;
};

std::vector<StarData> stars_db;

// Extracts constellation abbreviation from star name (e.g. "9Alp CMa" -> "CMa")
std::string get_constellation(const std::string& name) {
    if (name.empty()) return "Unnamed";
    size_t last_space = name.find_last_of(" \t");
    std::string last_word = (last_space == std::string::npos) ? name : name.substr(last_space + 1);
    if (last_word.length() == 3 && std::isalpha(last_word[0]) && std::isalpha(last_word[1]) && std::isalpha(last_word[2])) {
        return last_word;
    }
    if (name.length() >= 3) {
        std::string last_3 = name.substr(name.length() - 3);
        if (std::isalpha(last_3[0]) && std::isalpha(last_3[1]) && std::isalpha(last_3[2])) {
            return last_3;
        }
    }
    return "Unnamed";
}

// Function to perform calculations and update the TableScrollable widget rows
void recalculate_positions(const EOP& eop, int city_idx, int mag_idx,
                           TableScrollable& table, Border& main_border) {
    if (city_idx < 0 || city_idx >= (int)cities.size()) return;
    if (mag_idx < 0 || mag_idx >= (int)max_mags.size()) return;
    
    const City& city = cities[city_idx];
    double max_mag = max_mags[mag_idx];
    
    auto obs = Observer::on_earth(Site::from_GPS(city.longitude * Unit::deg,
                                                  city.latitude * Unit::deg,
                                                  city.elevation * Unit::m),
                                                  eop);
    Time t = Time::now(eop);
    auto frame = obs.frame_at(t, NOVAS_REDUCED_ACCURACY);
    
    // Update border title with observation time (UTC)
    main_border.set_title("Visible Stars (" + t.to_string() + " UTC)", Alignment::Center);
    
    struct VisibleStar {
        std::string name;
        double alt;
        double az;
        double mag;
    };
    
    std::unordered_map<std::string, std::vector<VisibleStar>> grouped;
    Weather weather(Temperature::celsius(15.0), Pressure::mbar(780.0), 50.0 * Unit::percent);
    
    for (const auto& star : stars_db) {
        if (star.magnitude > max_mag) continue;
        
        Apparent apparent = star.source->apparent_in(frame);
        if (!apparent) continue;
        
        supernovas::Horizontal hor = apparent.to_horizontal().to_refracted(novas_optical_refraction, weather);
        double alt = hor.elevation().deg();
        if (alt <= 0.0) continue; // Below the local horizon
        
        VisibleStar vs;
        vs.name = star.name;
        vs.alt = alt;
        vs.az = hor.azimuth().deg();
        vs.mag = star.magnitude;
        
        grouped[star.constellation].push_back(vs);
    }
    
    // Sort constellations alphabetically
    std::vector<std::string> constels;
    for (const auto& pair : grouped) {
        constels.push_back(pair.first);
    }
    std::sort(constels.begin(), constels.end(), [](const std::string& a, const std::string& b) {
        std::string name_a = a;
        std::string name_b = b;
        auto it_a = constellation_names.find(a);
        if (it_a != constellation_names.end()) name_a = it_a->second;
        auto it_b = constellation_names.find(b);
        if (it_b != constellation_names.end()) name_b = it_b->second;
        return name_a < name_b;
    });
    
    std::vector<std::vector<StyledText>> new_rows;
    for (const auto& abbrev : constels) {
        std::string display_name = abbrev;
        auto it = constellation_names.find(abbrev);
        if (it != constellation_names.end()) {
            display_name = it->second + " (" + abbrev + ")";
        } else if (abbrev == "Unnamed") {
            display_name = "Unnamed Stars / Other";
        }
        
        // Header row for constellation
        StyledText c_header;
        c_header.colored_bold("✦ " + display_name, Color::Cyan());
        new_rows.push_back({c_header, "", "", ""});
        
        // Sort stars in constellation by brightness (mag ascending)
        auto stars = grouped[abbrev];
        std::sort(stars.begin(), stars.end(), [](const VisibleStar& a, const VisibleStar& b) {
            return a.mag < b.mag;
        });
        
        for (const auto& s : stars) {
            std::ostringstream mag_oss;
            mag_oss << std::fixed << std::setprecision(2) << s.mag;
            
            std::ostringstream alt_oss;
            alt_oss << std::fixed << std::setprecision(2) << s.alt << "°";
            
            std::ostringstream az_oss;
            az_oss << std::fixed << std::setprecision(2) << s.az << "°";
            
            new_rows.push_back({
                "  " + s.name,
                mag_oss.str(),
                alt_oss.str(),
                az_oss.str()
            });
        }
    }
    
    table.rows = std::move(new_rows);
}

int main() {
    std::cout << "Loading Yale Bright Star Catalog (BSC5)..." << std::endl;
    BSC5_Parser parser;
    std::string json_path = "../../common/data/bsc5.json";
    if (!parser.loadCatalog(json_path)) {
        std::cerr << "Error: Could not load bsc5.json catalog." << std::endl;
        return 1;
    }
    
    std::vector<star_t> raw_stars = parser.query("");
    std::cout << "Parsing " << raw_stars.size() << " stars..." << std::endl;
    
    stars_db.reserve(raw_stars.size());
    for (const auto& star : raw_stars) {
        try {
            auto entry = CatalogEntry(star.name, Equatorial(star.ra_str, star.dec_str, Equinox::j2000()))
                    .proper_motion(star.pm_ra_si, star.pm_dec_si)
                    .parallax(Angle(star.parallax_si))
                    .radial_velocity(ScalarVelocity(star.radvel_si));
            if (!entry) continue;
            
            StarData sd;
            sd.name = star.name;
            sd.magnitude = star.magnitude;
            sd.constellation = get_constellation(star.name);
            sd.source = std::make_shared<CatalogSource>(entry.to_source());
            stars_db.push_back(std::move(sd));
        } catch (...) {
            // Skip invalid star database entries
        }
    }
    std::cout << "Successfully loaded " << stars_db.size() << " stars into memory." << std::endl;
    
    // TUI setup
    App app;
    Theme::set_theme(Theme::Dark());
    
    EOP eop(LEAP_SECONDS, DUT1, POLAR_DX * Unit::mas, POLAR_DY * Unit::mas);
    
    // Sidebar layouts
    auto sidebar = std::make_shared<Vertical>();
    sidebar->fixed_width = 25;
    
    auto title_lbl = std::make_shared<Label>(StyledText().bold("★ SKY MONITOR ★"));
    title_lbl->fixed_height = 1;
    sidebar->add(title_lbl);
    sidebar->add(std::make_shared<VerticalSpacer>(1));
    
    // City selection
    auto city_border = std::make_shared<Border>(BorderStyle::Rounded);
    city_border->set_title("Observer Location", Alignment::Left);
    city_border->fixed_height = 5;
    
    auto city_radio = std::make_shared<RadioSet>();
    city_radio->set_options({"Mexico City", "London", "New York"});
    city_radio->selected_index = 0; // Default to Mexico City
    city_border->add(city_radio);
    sidebar->add(city_border);
    sidebar->add(std::make_shared<VerticalSpacer>(1));
    
    // Magnitude selection
    auto mag_border = std::make_shared<Border>(BorderStyle::Rounded);
    mag_border->set_title("Max Magnitude", Alignment::Left);
    mag_border->fixed_height = 5;
    
    auto mag_radio = std::make_shared<RadioSet>();
    mag_radio->set_options({"Magnitude 1", "Magnitude 3", "Magnitude 6"});
    mag_radio->selected_index = 1; // Default to Mag 3
    mag_border->add(mag_radio);
    sidebar->add(mag_border);
    sidebar->add(std::make_shared<VerticalSpacer>(1));
    
    // Quick instructions
    auto info_border = std::make_shared<Border>(BorderStyle::Rounded);
    info_border->set_title("Info", Alignment::Left);
    info_border->fixed_height = 6;
    
    auto info_layout = std::make_shared<Vertical>();
    info_layout->add(std::make_shared<Label>("Use TAB to switch"));
    info_layout->add(std::make_shared<Label>("focus. ARROWS to"));
    info_layout->add(std::make_shared<Label>("select. Q to exit"));
    info_border->add(info_layout);
    sidebar->add(info_border);
    
    // Main star list layout
    auto main_border = std::make_shared<Border>(BorderStyle::Double);
    main_border->set_title("Visible Stars", Alignment::Center);
    
    auto table = std::make_shared<TableScrollable>();
    table->columns = {"Constellation / Star", "Vmag", "Elevation", "Azimuth"};
    table->col_widths = {24, 7, 12, 12};
    main_border->add(table);
    
    // Combined root layout
    auto root = std::make_shared<cpptui::Horizontal>();
    root->add(sidebar);
    root->add(std::make_shared<HorizontalSpacer>(1));
    root->add(main_border);
    
    // Recalculate initial positions
    recalculate_positions(eop, city_radio->selected_index, mag_radio->selected_index, *table, *main_border);
    
    // Define change callbacks to refresh positions immediately
    city_radio->on_change = [eop, city_radio, mag_radio, table, main_border](int idx) {
        recalculate_positions(eop, idx, mag_radio->selected_index, *table, *main_border);
    };
    
    mag_radio->on_change = [eop, city_radio, mag_radio, table, main_border](int idx) {
        recalculate_positions(eop, city_radio->selected_index, idx, *table, *main_border);
    };
    
    // Setup recurring timer (every 500ms) to update coordinates
    app.add_timer(500, [eop, city_radio, mag_radio, table, main_border]() {
        recalculate_positions(eop, city_radio->selected_index, mag_radio->selected_index, *table, *main_border);
    });
    
    app.register_exit_key('q');
    app.register_exit_key('Q');
    
    app.run(root);
    return 0;
}
