#pragma once
#include <string>
#include <vector>

class GestionnaireAdb {
public:
    bool connecter(const std::string& appareil = "");
    void deconnecter();
    bool est_connecte() const;
    std::string appareil_actuel() const;
    std::vector<std::string> lister_appareils() const;

    bool capturer_ecran(const std::string& chemin_sortie) const;
    bool taper(int x, int y) const;
    bool glisser(int x1, int y1, int x2, int y2, int duree_ms = 200) const;
    // Geste en deux phases : sélection (hold) puis déplacement.
    // La pièce grossit pendant le hold ; on la déplace ensuite vers la destination.
    bool glisser_avec_selection(int x1, int y1, int x2, int y2,
                                int hold_ms = 350, int move_ms = 500) const;

private:
    std::string appareil_;
    bool connecte_ = false;

    std::string executer(const std::string& commande) const;
    std::string commande_adb(const std::string& args) const;
};
