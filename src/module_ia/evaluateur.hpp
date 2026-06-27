#pragma once
#include "module_simulation/grille.hpp"
#include <array>

constexpr int NB_CRITERES = 4;

using Genome = std::array<float, NB_CRITERES>;

struct ScoreEvaluation {
    float total;
    float lignes;         // cellules effacées ce tour
    float prog_lignes;    // sum(n_ligne²) : gradient vers complétion (lignes)
    float prog_colonnes;  // sum(n_col²)   : gradient vers complétion (colonnes)
    float isolement;      // cellules dans lignes/cols très vides (n ≤ 2) — pénalité
};

ScoreEvaluation evaluer(const Grille& grille, int cellules_effacees, const Genome& poids);
