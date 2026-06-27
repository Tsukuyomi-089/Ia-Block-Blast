#pragma once
#include "piece.hpp"
#include "constantes.hpp"
#include <array>
#include <cstdint>

class Grille {
public:
    Grille();
    void reinitialiser();

    bool peut_placer(const Piece& piece, int ligne, int colonne) const;
    void placer(const Piece& piece, int ligne, int colonne);
    int supprimer_lignes_colonnes();

    bool est_vide(int ligne, int colonne) const;
    bool est_remplie(int ligne, int colonne) const;

    int hauteur_colonne(int col) const;
    int compter_trous() const;
    int calculer_bosselage() const;
    int hauteur_totale() const;

    const std::array<std::array<uint8_t, TAILLE_GRILLE>, TAILLE_GRILLE>& cellules() const;

private:
    std::array<std::array<uint8_t, TAILLE_GRILLE>, TAILLE_GRILLE> grille_;

    bool ligne_pleine(int ligne) const;
    bool colonne_pleine(int col) const;
    void effacer_ligne(int ligne);
    void effacer_colonne(int col);
};
