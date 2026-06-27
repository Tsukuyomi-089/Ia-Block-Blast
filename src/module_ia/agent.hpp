#pragma once
#include "evaluateur.hpp"
#include "module_simulation/jeu.hpp"
#include <random>

struct Coup {
    int index_piece;
    int ligne;
    int colonne;
    float score;
};

class Agent {
public:
    explicit Agent(Genome genome = {10.0f, 0.6f, 0.6f, 0.3f});

    Coup choisir_coup(const EtatJeu& etat) const;
    int jouer_partie(std::mt19937& rng) const;

    const Genome& genome() const { return genome_; }
    Genome& genome()             { return genome_; }

private:
    Genome genome_;

    // Beam search récursif sur les pièces restantes.
    // Retourne le meilleur score atteignable en plaçant les pièces restantes.
    float evaluer_sequence(const EtatJeu& etat, int clears_cumules, int profondeur) const;

    static constexpr int LARGEUR_FAISCEAU = 5;
};
