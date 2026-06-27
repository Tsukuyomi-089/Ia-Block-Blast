#include "grille.hpp"
#include <cmath>
#include <array>

Grille::Grille() {
    reinitialiser();
}

void Grille::reinitialiser() {
    for (auto& ligne : grille_)
        ligne.fill(0);
}

bool Grille::peut_placer(const Piece& piece, int ligne, int colonne) const {
    for (const auto& c : piece.cellules) {
        int r = ligne + c.ligne;
        int col = colonne + c.colonne;
        if (r < 0 || r >= TAILLE_GRILLE || col < 0 || col >= TAILLE_GRILLE)
            return false;
        if (grille_[r][col] != 0)
            return false;
    }
    return true;
}

void Grille::placer(const Piece& piece, int ligne, int colonne) {
    for (const auto& c : piece.cellules)
        grille_[ligne + c.ligne][colonne + c.colonne] = 1;
}

bool Grille::ligne_pleine(int ligne) const {
    for (int c = 0; c < TAILLE_GRILLE; ++c)
        if (grille_[ligne][c] == 0) return false;
    return true;
}

bool Grille::colonne_pleine(int col) const {
    for (int r = 0; r < TAILLE_GRILLE; ++r)
        if (grille_[r][col] == 0) return false;
    return true;
}

void Grille::effacer_ligne(int ligne) {
    for (int c = 0; c < TAILLE_GRILLE; ++c)
        grille_[ligne][c] = 0;
}

void Grille::effacer_colonne(int col) {
    for (int r = 0; r < TAILLE_GRILLE; ++r)
        grille_[r][col] = 0;
}

int Grille::supprimer_lignes_colonnes() {
    std::array<bool, TAILLE_GRILLE> lignes_pleines{}, colonnes_pleines{};

    for (int r = 0; r < TAILLE_GRILLE; ++r)
        lignes_pleines[r] = ligne_pleine(r);
    for (int c = 0; c < TAILLE_GRILLE; ++c)
        colonnes_pleines[c] = colonne_pleine(c);

    int supprimees = 0;
    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        if (lignes_pleines[r]) { effacer_ligne(r); supprimees += TAILLE_GRILLE; }
    }
    for (int c = 0; c < TAILLE_GRILLE; ++c) {
        if (colonnes_pleines[c]) { effacer_colonne(c); supprimees += TAILLE_GRILLE; }
    }

    return supprimees;
}

bool Grille::est_vide(int ligne, int colonne) const {
    return grille_[ligne][colonne] == 0;
}

bool Grille::est_remplie(int ligne, int colonne) const {
    return grille_[ligne][colonne] != 0;
}

int Grille::hauteur_colonne(int col) const {
    for (int r = 0; r < TAILLE_GRILLE; ++r)
        if (grille_[r][col] != 0) return TAILLE_GRILLE - r;
    return 0;
}

int Grille::compter_trous() const {
    int trous = 0;
    for (int c = 0; c < TAILLE_GRILLE; ++c) {
        bool bloc_au_dessus = false;
        for (int r = 0; r < TAILLE_GRILLE; ++r) {
            if (grille_[r][c] != 0) bloc_au_dessus = true;
            else if (bloc_au_dessus) ++trous;
        }
    }
    return trous;
}

int Grille::calculer_bosselage() const {
    int bosselage = 0;
    for (int c = 0; c < TAILLE_GRILLE - 1; ++c)
        bosselage += std::abs(hauteur_colonne(c) - hauteur_colonne(c + 1));
    return bosselage;
}

int Grille::hauteur_totale() const {
    int total = 0;
    for (int c = 0; c < TAILLE_GRILLE; ++c)
        total += hauteur_colonne(c);
    return total;
}

const std::array<std::array<uint8_t, TAILLE_GRILLE>, TAILLE_GRILLE>& Grille::cellules() const {
    return grille_;
}
