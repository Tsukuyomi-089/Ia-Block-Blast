#pragma once
#include "module_simulation/grille.hpp"
#include "module_simulation/piece.hpp"
#include "module_simulation/constantes.hpp"
#include <opencv2/opencv.hpp>
#include <array>
#include <optional>
#include <string>

struct AnalyseEcran {
    Grille grille;
    std::array<Piece, NB_PIECES_SIMULTANEES> pieces;
    cv::Rect zone_grille;
    std::array<cv::Point, NB_PIECES_SIMULTANEES> positions_pieces;  // centre zone
    std::array<cv::Rect,  NB_PIECES_SIMULTANEES> bbox_pieces;       // bbox réelle dans l'image
    std::array<bool,      NB_PIECES_SIMULTANEES> pieces_presentes = {true, true, true};
    bool valide = false;
};

struct ConfigVision {
    cv::Rect region_grille    = {0, 0, 0, 0};
    std::array<cv::Point, NB_PIECES_SIMULTANEES> centres_pieces = {};
    int    taille_zone_piece  = 200;   // px autour de chaque centre de pièce
    double seuil_remplissage  = 0.32;
    int    decalage_doigt_y   = 0;     // px : le doigt vise plus bas que la pièce (lift visuel)
    bool   valide             = false; // true si config chargée depuis fichier
};

class AnalyseurEcran {
public:
    explicit AnalyseurEcran(ConfigVision config = {});

    std::optional<AnalyseEcran> analyser(const cv::Mat& capture) const;
    cv::Mat visualiser(const cv::Mat& capture, const AnalyseEcran& analyse) const;
    void definir_config(const ConfigVision& config);

    bool charger_config(const std::string& chemin);
    bool   config_valide()     const { return config_.valide; }
    int    decalage_doigt_y()  const { return config_.decalage_doigt_y; }
    double seuil_remplissage() const { return config_.seuil_remplissage; }

    static std::string chemin_config();
    static void creer_config_exemple(const std::string& chemin);

private:
    ConfigVision config_;

    Grille extraire_grille(const cv::Mat& region) const;
    bool   cellule_remplie(const cv::Mat& cellule) const;
    Piece  detecter_piece(const cv::Mat& region_piece) const;
};
