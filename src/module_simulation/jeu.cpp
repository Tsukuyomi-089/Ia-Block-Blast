#include "jeu.hpp"

Jeu::Jeu(std::mt19937& rng) : rng_(rng) {
    reinitialiser();
}

void Jeu::reinitialiser() {
    etat_.grille.reinitialiser();
    etat_.score = 0;
    etat_.termine = false;
    etat_.nb_coups = 0;
    etat_.pieces_utilisees.fill(false);
    for (auto& p : etat_.pieces_courantes)
        p = piece_aleatoire(rng_);
}

bool Jeu::peut_jouer() const {
    return !etat_.termine;
}

bool Jeu::toutes_pieces_utilisees() const {
    for (bool u : etat_.pieces_utilisees)
        if (!u) return false;
    return true;
}

bool Jeu::jouer_coup(int index_piece, int ligne, int colonne) {
    if (etat_.termine) return false;
    if (index_piece < 0 || index_piece >= NB_PIECES_SIMULTANEES) return false;
    if (etat_.pieces_utilisees[index_piece]) return false;

    const Piece& piece = etat_.pieces_courantes[index_piece];
    if (!etat_.grille.peut_placer(piece, ligne, colonne)) return false;

    etat_.grille.placer(piece, ligne, colonne);
    int supprimees = etat_.grille.supprimer_lignes_colonnes();
    etat_.score += supprimees;  // seules les lignes effacées comptent → signal propre pour le GA
    etat_.pieces_utilisees[index_piece] = true;
    ++etat_.nb_coups;

    if (toutes_pieces_utilisees())
        recharger_pieces();

    verifier_fin();
    return true;
}

void Jeu::recharger_pieces() {
    for (auto& p : etat_.pieces_courantes)
        p = piece_aleatoire(rng_);
    etat_.pieces_utilisees.fill(false);
}

void Jeu::verifier_fin() {
    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        if (etat_.pieces_utilisees[i]) continue;
        const Piece& piece = etat_.pieces_courantes[i];
        for (int r = 0; r <= TAILLE_GRILLE - piece.hauteur; ++r)
            for (int c = 0; c <= TAILLE_GRILLE - piece.largeur; ++c)
                if (etat_.grille.peut_placer(piece, r, c)) return;
    }
    etat_.termine = true;
}

const EtatJeu& Jeu::etat() const {
    return etat_;
}
