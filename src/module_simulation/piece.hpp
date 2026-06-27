#pragma once
#include <vector>
#include <string>
#include <random>

struct Cellule {
    int ligne;
    int colonne;
};

struct Piece {
    std::string nom;
    std::vector<Cellule> cellules;
    int largeur;
    int hauteur;
};

const std::vector<Piece>& catalogue_pieces();
Piece piece_aleatoire(std::mt19937& rng);
