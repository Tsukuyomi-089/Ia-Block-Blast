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

cv::Mat AnalyseurEcran::masque_piece_vs_fond(const cv::Mat& region) {
    // Échantillonne les 4 coins (10×10 px) pour estimer la couleur de fond.
    // Les pièces sont toujours centrées dans la zone, les coins sont du fond pur.
    int m = 10;
    int W = region.cols, H = region.rows;
    cv::Scalar fond =
        (cv::mean(region(cv::Rect(0,   0,   m, m))) +
         cv::mean(region(cv::Rect(W-m, 0,   m, m))) +
         cv::mean(region(cv::Rect(0,   H-m, m, m))) +
         cv::mean(region(cv::Rect(W-m, H-m, m, m)))) * 0.25;

    // Pixels qui s'écartent du fond de plus de 35 (BGR L∞)
    cv::Mat diff;
    cv::absdiff(region,
                cv::Mat(region.size(), region.type(), fond),
                diff);
    cv::Mat gris;
    cv::cvtColor(diff, gris, cv::COLOR_BGR2GRAY);
    cv::Mat masque;
    cv::threshold(gris, masque, 35, 255, cv::THRESH_BINARY);

    // Éliminer le bruit et boucher les trous
    cv::Mat k = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    cv::morphologyEx(masque, masque, cv::MORPH_OPEN,  k);
    cv::morphologyEx(masque, masque, cv::MORPH_CLOSE, k);
    return masque;
}

bool AnalyseurEcran::cellule_remplie(const cv::Mat& cellule) const {
    cv::Mat hsv;
    cv::cvtColor(cellule, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> canaux;
    cv::split(hsv, canaux);

    double moy_s = cv::mean(canaux[1])[0] / 255.0;
    double moy_v = cv::mean(canaux[2])[0] / 255.0;

    // Test 1 : pièce vive (rouge, jaune, vert…) — S×V élevé
    if (moy_s * moy_v > config_.seuil_remplissage) return true;

    // Test 2 : pièce sombre (bleu marine, violet…) — S×V faible mais présence d'un
    // dégradé highlight→ombre caractéristique de toute pièce Block Blast.
    // Un fond uniforme a un écart-type V ≈ 0.01, une pièce ≈ 0.05+.
    cv::Scalar moy_ch, ecart_ch;
    cv::meanStdDev(canaux[2], moy_ch, ecart_ch);
    return (ecart_ch[0] / 255.0) > 0.04;
}

Grille AnalyseurEcran::extraire_grille(const cv::Mat& region) const {
    Grille grille;
    grille.reinitialiser();

    int lc = region.cols / TAILLE_GRILLE;
    int hc = region.rows / TAILLE_GRILLE;
    // Marge de 28% : on ne regarde que le centre de la case.
    // Les pièces ont un dégradé d'ombre en bas/côtés (S*V faible),
    // mais leur CENTRE est toujours vif. Le fond est uniforme partout.
    int mx = lc * 28 / 100;
    int my = hc * 28 / 100;

    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        for (int c = 0; c < TAILLE_GRILLE; ++c) {
            cv::Rect z(c * lc + mx, r * hc + my, lc - 2*mx, hc - 2*my);
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

    // Masque : pixels qui s'écartent du fond (coins) → isole la pièce
    cv::Mat masque = masque_piece_vs_fond(region);

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

        // Masque : soustraction du fond (coins de la zone) → isole la pièce
        cv::Mat region = capture(z);
        cv::Mat masque = masque_piece_vs_fond(region);

        // Slot présent si la pièce occupe plus de 300 pixels distincts du fond
        bool slot_present = (cv::countNonZero(masque) > 300);
        analyse.pieces_presentes[i] = slot_present;

        if (slot_present) {
            cv::Rect bbox_local = cv::boundingRect(masque);
            analyse.bbox_pieces[i] = cv::Rect(z.x + bbox_local.x, z.y + bbox_local.y,
                                               bbox_local.width,   bbox_local.height);
        } else {
            analyse.bbox_pieces[i] = z;
        }

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
