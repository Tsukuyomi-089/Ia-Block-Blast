#include "gestionnaire_adb.hpp"
#include <cstdio>
#include <sstream>
#include <array>
#include <format>

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

bool GestionnaireAdb::glisser_avec_selection(
        int x1, int y1, int x2, int y2, int hold_ms, int move_ms) const {
    if (!connecte_) return false;

    // Protocole B (slots) via sendevent — un seul appel adb shell, pas de coupure.
    // /dev/input/event2 détecté sur cet appareil.
    // Types : 0=EV_SYN  1=EV_KEY  3=EV_ABS
    // Codes ABS : 47=SLOT  48=TOUCH_MAJOR  53=POSITION_X  54=POSITION_Y  57=TRACKING_ID
    // Code KEY  : 330=BTN_TOUCH
    static const char* DEV = "/dev/input/event2";
    std::string s;
    auto se = [&](int type, int code, int val) {
        s += std::format("sendevent {} {} {} {}; ", DEV, type, code, val);
    };
    auto syn = [&] { se(0, 0, 0); };

    // ── Touch DOWN ──────────────────────────────────────────────────────────
    se(3, 47, 0);        // ABS_MT_SLOT = 0
    se(3, 57, 1);        // ABS_MT_TRACKING_ID = 1  (début du contact)
    se(3, 48, 80);       // ABS_MT_TOUCH_MAJOR = 80 (taille du doigt)
    se(3, 53, x1);       // ABS_MT_POSITION_X
    se(3, 54, y1);       // ABS_MT_POSITION_Y
    se(1, 330, 1);       // BTN_TOUCH = 1
    syn();

    // ── Hold : laisser le doigt sur la pièce le temps qu'elle grossisse ─────
    s += std::format("sleep 0.{:03d}; ", hold_ms);  // ex: sleep 0.350

    // ── Déplacement progressif vers la destination (10 étapes) ──────────────
    const int etapes = 10;
    for (int i = 1; i <= etapes; ++i) {
        int mx = x1 + (x2 - x1) * i / etapes;
        int my = y1 + (y2 - y1) * i / etapes;
        se(3, 53, mx);
        se(3, 54, my);
        syn();
        // Pause entre étapes : move_ms / etapes millisecondes
        s += std::format("sleep 0.{:03d}; ", move_ms / etapes);
    }

    // ── Touch UP ─────────────────────────────────────────────────────────────
    se(3, 47, 0);        // ABS_MT_SLOT = 0
    se(3, 57, 65535);    // ABS_MT_TRACKING_ID = 0xFFFF → fin du contact
    se(1, 330, 0);       // BTN_TOUCH = 0
    syn();

    executer(commande_adb("shell \"" + s + "\""));
    return true;
}
