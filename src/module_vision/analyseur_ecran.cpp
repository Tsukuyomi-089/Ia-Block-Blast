#include "analyseur_ecran.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>

// ─── Chemins ─────────────────────────────────────────────────────────────────

std::string AnalyseurEcran::chemin_config() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/ia_bb_config.txt" : "ia_bb_config.txt";
}

void AnalyseurEcran::creer_config_exemple(const std::string& chemin) {
    std::ofstream f(chemin);
    if (!f) return;
    f << R"(# ============================================================
# ia_bb_config.txt  —  Calibration du mode réel IA Block Blast
# ============================================================
# Instructions :
#   1. Ouvrez ~/ia_bb_screenshot.png dans GIMP ou tout éditeur
#      (Cliquez "Calibrer" dans l'appli pour prendre le screenshot)
#   2. Repérez le rectangle exact de la grille 8×8
#   3. Repérez le centre de chacune des 3 pièces en bas
#   4. Remplissez les valeurs ci-dessous
#   5. Sauvegardez sous  ~/ia_bb_config.txt
#   6. Cliquez "Connecter" dans l'appli pour recharger

# Grille 8×8  (x  y  largeur  hauteur)   en pixels écran
grille 64 588 952 952

# Centre de chaque pièce  (x  y)  — les 3 slots affichés en bas
piece0 230 1831
piece1 548 1836
piece2 862 1831

# Taille de la zone d'analyse autour de chaque centre (pixels)
taille_piece 240

# Seuil de saturation pour détecter une case remplie  (0.0–1.0)
# Augmenter si l'IA voit des cases pleines qui sont vides, diminuer sinon.
seuil 0.25

# Décalage vertical de pose (pixels) : Block Blast affiche la pièce AU-DESSUS
# du doigt. Le doigt doit donc viser plus bas que la case cible.
# Point de départ ≈ 1,2 case. Augmenter si la pièce se pose trop haut.
# Valeurs valides pour une capture screencap de 1084×2412.
decalage_doigt_y 143
)";
}

// ─── Construction & config ────────────────────────────────────────────────────

AnalyseurEcran::AnalyseurEcran(ConfigVision config) : config_(config) {
    if (!config_.valide)
        charger_config(chemin_config());
}

bool AnalyseurEcran::charger_config(const std::string& chemin) {
    std::ifstream f(chemin);
    if (!f) return false;

    std::string ligne;
    while (std::getline(f, ligne)) {
        if (ligne.empty() || ligne[0] == '#') continue;
        std::istringstream iss(ligne);
        std::string cle;
        iss >> cle;

        if (cle == "grille") {
            int x, y, w, h;
            if (iss >> x >> y >> w >> h)
                config_.region_grille = {x, y, w, h};
        } else if (cle == "piece0") {
            int x, y;
            if (iss >> x >> y) config_.centres_pieces[0] = {x, y};
        } else if (cle == "piece1") {
            int x, y;
            if (iss >> x >> y) config_.centres_pieces[1] = {x, y};
        } else if (cle == "piece2") {
            int x, y;
            if (iss >> x >> y) config_.centres_pieces[2] = {x, y};
        } else if (cle == "taille_piece") {
            iss >> config_.taille_zone_piece;
        } else if (cle == "seuil") {
            iss >> config_.seuil_remplissage;
        } else if (cle == "decalage_doigt_y") {
            iss >> config_.decalage_doigt_y;
        }
    }

    config_.valide = (config_.region_grille.area() > 0);
    return config_.valide;
}

void AnalyseurEcran::definir_config(const ConfigVision& config) {
    config_ = config;
}

// ─── Détection de grille ──────────────────────────────────────────────────────

bool AnalyseurEcran::cellule_remplie(const cv::Mat& cellule) const {
    cv::Mat hsv;
    cv::cvtColor(cellule, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> canaux;
    cv::split(hsv, canaux);
    // Utilise S×V plutôt que S seule :
    //   fond marron vide → S≈0.45, V≈0.35 → S×V≈0.16 (sous le seuil)
    //   pièce colorée    → S≈0.70, V≈0.80 → S×V≈0.56 (au-dessus)
    //   fond cyan clair  → S≈0.15, V≈0.85 → S×V≈0.13 (sous le seuil)
    // Fonctionne pour tous les thèmes du jeu.
    double moy_s = cv::mean(canaux[1])[0];
    double moy_v = cv::mean(canaux[2])[0];
    double chroma = (moy_s / 255.0) * (moy_v / 255.0);
    return chroma > config_.seuil_remplissage;
}

Grille AnalyseurEcran::extraire_grille(const cv::Mat& region) const {
    Grille grille;
    grille.reinitialiser();

    int lc = region.cols / TAILLE_GRILLE;
    int hc = region.rows / TAILLE_GRILLE;

    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        for (int c = 0; c < TAILLE_GRILLE; ++c) {
            cv::Rect z(c * lc + 2, r * hc + 2, lc - 4, hc - 4);
            z &= cv::Rect(0, 0, region.cols, region.rows);
            if (z.width <= 0 || z.height <= 0) continue;
            if (cellule_remplie(region(z))) {
                Piece m{".", {{0, 0}}, 1, 1};
                grille.placer(m, r, c);
            }
        }
    }
    return grille;
}

// ─── Détection de pièce par correspondance avec le catalogue ─────────────────

Piece AnalyseurEcran::detecter_piece(const cv::Mat& region) const {
    const auto& cat = catalogue_pieces();
    if (region.empty() || region.rows < 20 || region.cols < 20)
        return cat[0];

    // Masque des pixels colorés — seuil 120 pour exclure le fond cyan du bac (sat≈94)
    cv::Mat hsv, masque;
    cv::cvtColor(region, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, cv::Scalar(0, 95, 80), cv::Scalar(180, 255, 255), masque);

    // Nettoyer le bruit
    cv::Mat noyau = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::morphologyEx(masque, masque, cv::MORPH_OPEN,  noyau);
    cv::morphologyEx(masque, masque, cv::MORPH_CLOSE, noyau);

    cv::Rect bbox = cv::boundingRect(masque);
    if (bbox.area() < 80) return cat[0];

    // Pour chaque pièce du catalogue : redimensionner à la bbox et mesurer
    // le taux de chevauchement pixel-à-pixel.
    int meilleur_idx = 0;
    float meilleur_score = -1.0f;

    for (int idx = 0; idx < static_cast<int>(cat.size()); ++idx) {
        const Piece& p = cat[idx];
        float cw = static_cast<float>(bbox.width)  / p.largeur;
        float ch = static_cast<float>(bbox.height) / p.hauteur;

        // Les cellules doivent être à peu près carrées (ratio ≤ 2.5)
        float ratio = std::max(cw, ch) / std::max(1.0f, std::min(cw, ch));
        if (ratio > 2.5f) continue;

        float score = 0.0f;
        for (const auto& cell : p.cellules) {
            int x = bbox.x + static_cast<int>(cell.colonne * cw + cw * 0.1f);
            int y = bbox.y + static_cast<int>(cell.ligne   * ch + ch * 0.1f);
            cv::Rect cr(x, y,
                        std::max(1, static_cast<int>(cw * 0.8f)),
                        std::max(1, static_cast<int>(ch * 0.8f)));
            cr &= cv::Rect(0, 0, masque.cols, masque.rows);
            if (cr.area() <= 0) continue;
            float remplissage = static_cast<float>(cv::countNonZero(masque(cr))) / cr.area();
            score += remplissage;
        }
        score /= static_cast<float>(p.cellules.size());

        if (score > meilleur_score) {
            meilleur_score = score;
            meilleur_idx   = idx;
        }
    }

    return cat[meilleur_idx];
}

// ─── Analyse complète ─────────────────────────────────────────────────────────

std::optional<AnalyseEcran> AnalyseurEcran::analyser(const cv::Mat& capture) const {
    if (capture.empty()) return std::nullopt;
    if (!config_.valide)  return std::nullopt;  // config obligatoire

    // Grille
    cv::Rect zone = config_.region_grille;
    zone &= cv::Rect(0, 0, capture.cols, capture.rows);
    if (zone.area() == 0) return std::nullopt;

    AnalyseEcran analyse;
    analyse.zone_grille = zone;
    analyse.grille = extraire_grille(capture(zone));

    // 3 pièces
    int half = config_.taille_zone_piece / 2;
    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        const cv::Point& c = config_.centres_pieces[i];
        analyse.positions_pieces[i] = c;
        cv::Rect z(c.x - half, c.y - half,
                   config_.taille_zone_piece, config_.taille_zone_piece);
        z &= cv::Rect(0, 0, capture.cols, capture.rows);
        if (z.area() <= 0) {
            analyse.pieces[i]      = catalogue_pieces()[0];
            analyse.bbox_pieces[i] = z;
            continue;
        }

        // Calculer le bbox réel de la pièce dans la zone
        cv::Mat region = capture(z);
        cv::Mat hsv, masque;
        cv::cvtColor(region, hsv, cv::COLOR_BGR2HSV);
        // Seuil 120/255 : exclut le fond cyan du bac (sat≈94), garde les pièces (sat≈155+)
        cv::inRange(hsv, cv::Scalar(0, 95, 80), cv::Scalar(180, 255, 255), masque);
        cv::Mat noyau = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
        cv::morphologyEx(masque, masque, cv::MORPH_OPEN,  noyau);
        cv::morphologyEx(masque, masque, cv::MORPH_CLOSE, noyau);

        cv::Rect bbox_local = cv::boundingRect(masque);
        bool slot_present = (bbox_local.area() > 80);
        analyse.pieces_presentes[i] = slot_present;

        // Convertir bbox en coordonnées image complète
        analyse.bbox_pieces[i] = slot_present
            ? cv::Rect(z.x + bbox_local.x, z.y + bbox_local.y,
                       bbox_local.width,   bbox_local.height)
            : z;  // fallback: zone entière

        analyse.pieces[i] = slot_present ? detecter_piece(region) : catalogue_pieces()[0];
    }

    analyse.valide = true;
    return analyse;
}

cv::Mat AnalyseurEcran::visualiser(const cv::Mat& capture, const AnalyseEcran& analyse) const {
    cv::Mat res = capture.clone();
    if (!analyse.valide) return res;

    cv::rectangle(res, analyse.zone_grille, cv::Scalar(0, 255, 0), 2);

    int lc = analyse.zone_grille.width  / TAILLE_GRILLE;
    int hc = analyse.zone_grille.height / TAILLE_GRILLE;

    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        for (int c = 0; c < TAILLE_GRILLE; ++c) {
            if (!analyse.grille.est_remplie(r, c)) continue;
            cv::Point tl(analyse.zone_grille.x + c * lc,
                         analyse.zone_grille.y + r * hc);
            cv::rectangle(res, tl, tl + cv::Point(lc, hc),
                          cv::Scalar(0, 0, 255), 1);
        }
    }

    int half = config_.taille_zone_piece / 2;
    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        cv::Rect z(config_.centres_pieces[i].x - half,
                   config_.centres_pieces[i].y - half,
                   config_.taille_zone_piece, config_.taille_zone_piece);
        cv::rectangle(res, z, cv::Scalar(255, 165, 0), 2);
    }

    return res;
}
