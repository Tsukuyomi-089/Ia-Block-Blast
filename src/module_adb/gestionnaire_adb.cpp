#include "gestionnaire_adb.hpp"
#include <cstdio>
#include <sstream>
#include <array>

std::string GestionnaireAdb::executer(const std::string& commande) const {
    std::array<char, 256> buf;
    std::string resultat;
    FILE* pipe = popen(commande.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        resultat += buf.data();
    pclose(pipe);
    return resultat;
}

std::string GestionnaireAdb::commande_adb(const std::string& args) const {
    std::string cmd = "adb";
    if (!appareil_.empty()) cmd += " -s " + appareil_;
    return cmd + " " + args;
}

std::vector<std::string> GestionnaireAdb::lister_appareils() const {
    auto sortie = executer("adb devices 2>/dev/null");
    std::vector<std::string> appareils;
    std::istringstream flux(sortie);
    std::string ligne;
    bool premiere_ligne = true;
    while (std::getline(flux, ligne)) {
        if (premiere_ligne) { premiere_ligne = false; continue; }
        if (ligne.find("device") != std::string::npos &&
            ligne.find("offline") == std::string::npos) {
            auto pos = ligne.find('\t');
            if (pos != std::string::npos)
                appareils.push_back(ligne.substr(0, pos));
        }
    }
    return appareils;
}

bool GestionnaireAdb::connecter(const std::string& appareil) {
    auto appareils = lister_appareils();
    if (appareils.empty()) { connecte_ = false; return false; }
    appareil_ = appareil.empty() ? appareils[0] : appareil;
    connecte_ = true;
    return true;
}

void GestionnaireAdb::deconnecter() {
    appareil_.clear();
    connecte_ = false;
}

bool GestionnaireAdb::est_connecte() const { return connecte_; }
std::string GestionnaireAdb::appareil_actuel() const { return appareil_; }

bool GestionnaireAdb::capturer_ecran(const std::string& chemin_sortie) const {
    if (!connecte_) return false;
    std::string cmd = commande_adb("exec-out screencap -p") + " > " + chemin_sortie + " 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool GestionnaireAdb::taper(int x, int y) const {
    if (!connecte_) return false;
    std::ostringstream oss;
    oss << "shell input tap " << x << " " << y;
    executer(commande_adb(oss.str()));
    return true;
}

bool GestionnaireAdb::glisser(int x1, int y1, int x2, int y2, int duree_ms) const {
    if (!connecte_) return false;
    std::ostringstream oss;
    oss << "shell input swipe " << x1 << " " << y1
        << " " << x2 << " " << y2 << " " << duree_ms;
    executer(commande_adb(oss.str()));
    return true;
}
