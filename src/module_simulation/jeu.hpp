#pragma once
#include "grille.hpp"
#include "piece.hpp"
#include "constantes.hpp"
#include <array>
#include <random>

struct EtatJeu {
    Grille grille;
    std::array<Piece, NB_PIECES_SIMULTANEES> pieces_courantes;
    std::array<bool, NB_PIECES_SIMULTANEES> pieces_utilisees;
    int score;
    bool termine;
    int nb_coups;
};

class Jeu {
public:
    explicit Jeu(std::mt19937& rng);
    void reinitialiser();
    bool peut_jouer() const;
    bool jouer_coup(int index_piece, int ligne, int colonne);
    void recharger_pieces();
    const EtatJeu& etat() const;

private:
    EtatJeu etat_;
    std::mt19937& rng_;

    bool toutes_pieces_utilisees() const;
    void verifier_fin();
};
