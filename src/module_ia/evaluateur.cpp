#include "evaluateur.hpp"
#include "module_simulation/constantes.hpp"

ScoreEvaluation evaluer(const Grille& grille, int cellules_effacees, const Genome& poids) {
    ScoreEvaluation s;
    s.lignes = static_cast<float>(cellules_effacees);

    float prog_l = 0.0f, prog_c = 0.0f;
    int   isole  = 0;

    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        int n = 0;
        for (int c = 0; c < TAILLE_GRILLE; ++c)
            if (grille.est_remplie(r, c)) ++n;
        prog_l += static_cast<float>(n * n);
        // Pénalise les cellules vraiment isolées (n ≤ 2 dans leur ligne)
        if (n > 0 && n <= 2) isole += n;
    }

    for (int c = 0; c < TAILLE_GRILLE; ++c) {
        int n = 0;
        for (int r = 0; r < TAILLE_GRILLE; ++r)
            if (grille.est_remplie(r, c)) ++n;
        prog_c += static_cast<float>(n * n);
        if (n > 0 && n <= 2) isole += n;
    }

    s.prog_lignes   = prog_l;
    s.prog_colonnes = prog_c;
    s.isolement     = static_cast<float>(isole);

    // Gradient continu dès la 1re pièce :
    // n=1 → 1×w1, n=4 → 16×w1, n=7 → 49×w1 (3× mieux que 4 lignes à n=1)
    // Cela pousse l'IA à concentrer ses pièces dès le début.
    s.total = poids[0] * s.lignes
            + poids[1] * s.prog_lignes
            + poids[2] * s.prog_colonnes
            - poids[3] * s.isolement;

    return s;
}
