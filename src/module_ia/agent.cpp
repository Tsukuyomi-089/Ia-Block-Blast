#include "agent.hpp"
#include <algorithm>

Agent::Agent(Genome genome) : genome_(genome) {}

struct CandidatPlacement {
    float score_heuristique;
    int piece;
    int ligne;
    int colonne;
};

float Agent::evaluer_sequence(const EtatJeu& etat, int clears_cumules, int profondeur) const {
    if (profondeur == 0) {
        // poids[0] contrôle le poids des lignes effacées → le GA peut l'optimiser.
        return evaluer(etat.grille, clears_cumules, genome_).total;
    }

    std::vector<CandidatPlacement> candidats;

    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        if (etat.pieces_utilisees[i]) continue;
        const Piece& piece = etat.pieces_courantes[i];

        for (int r = 0; r <= TAILLE_GRILLE - piece.hauteur; ++r) {
            for (int c = 0; c <= TAILLE_GRILLE - piece.largeur; ++c) {
                if (!etat.grille.peut_placer(piece, r, c)) continue;

                Grille copie = etat.grille;
                copie.placer(piece, r, c);
                int supp = copie.supprimer_lignes_colonnes();
                // Heuristique de tri uniquement contrôlée par le génome.
                float sc = evaluer(copie, supp, genome_).total;
                candidats.push_back({sc, i, r, c});
            }
        }
    }

    if (candidats.empty()) return -10000.0f;

    int K = std::min(static_cast<int>(candidats.size()), LARGEUR_FAISCEAU);
    std::partial_sort(
        candidats.begin(), candidats.begin() + K, candidats.end(),
        [](const CandidatPlacement& a, const CandidatPlacement& b) {
            return a.score_heuristique > b.score_heuristique;
        });

    float meilleur = -1e9f;

    for (int k = 0; k < K; ++k) {
        const CandidatPlacement& cand = candidats[k];

        EtatJeu nouvel_etat = etat;
        nouvel_etat.grille.placer(nouvel_etat.pieces_courantes[cand.piece],
                                  cand.ligne, cand.colonne);
        int supp = nouvel_etat.grille.supprimer_lignes_colonnes();
        nouvel_etat.pieces_utilisees[cand.piece] = true;

        float sc = evaluer_sequence(nouvel_etat, clears_cumules + supp, profondeur - 1);
        if (sc > meilleur) meilleur = sc;
    }

    return meilleur;
}

Coup Agent::choisir_coup(const EtatJeu& etat) const {
    int pieces_restantes = 0;
    for (bool u : etat.pieces_utilisees)
        if (!u) ++pieces_restantes;

    if (pieces_restantes == 0) return {-1, -1, -1, 0.0f};

    float meilleur_score = -1e9f;
    Coup  meilleur{-1, -1, -1, 0.0f};

    // Premier niveau exhaustif : on teste toutes les positions pour toutes les pièces.
    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        if (etat.pieces_utilisees[i]) continue;
        const Piece& piece = etat.pieces_courantes[i];

        for (int r = 0; r <= TAILLE_GRILLE - piece.hauteur; ++r) {
            for (int c = 0; c <= TAILLE_GRILLE - piece.largeur; ++c) {
                if (!etat.grille.peut_placer(piece, r, c)) continue;

                EtatJeu nouvel_etat = etat;
                nouvel_etat.grille.placer(piece, r, c);
                int supp = nouvel_etat.grille.supprimer_lignes_colonnes();
                nouvel_etat.pieces_utilisees[i] = true;

                // Beam search sur les pièces restantes après ce premier coup.
                float sc = evaluer_sequence(nouvel_etat, supp, pieces_restantes - 1);

                if (sc > meilleur_score) {
                    meilleur_score = sc;
                    meilleur = {i, r, c, sc};
                }
            }
        }
    }

    return meilleur;
}

int Agent::jouer_partie(std::mt19937& rng) const {
    Jeu jeu(rng);
    while (jeu.peut_jouer()) {
        auto coup = choisir_coup(jeu.etat());
        if (coup.index_piece < 0) break;
        jeu.jouer_coup(coup.index_piece, coup.ligne, coup.colonne);
    }
    return jeu.etat().score;
}
