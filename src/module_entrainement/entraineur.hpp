#pragma once
#include "module_ia/agent.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>

struct StatistiquesGeneration {
    int generation              = 0;
    float meilleur_score        = 0.0f;   // meilleur de la génération courante
    float meilleur_score_absolu = 0.0f;   // record absolu → jamais décroissant
    float score_moyen           = 0.0f;
    Genome meilleur_genome{};             // meilleur genome de la génération
    Genome meilleur_genome_absolu{};      // genome correspondant au record absolu
    EtatJeu dernier_jeu{};
    bool premiere_generation    = false;  // vrai uniquement au 1er callback de la session
};

class Entraineur {
public:
    Entraineur(int taille_population = 20, int parties_par_agent = 10);

    void demarrer(std::function<void(const StatistiquesGeneration&)> callback);
    void arreter();
    bool en_cours() const;
    StatistiquesGeneration derniere_stat() const;

    // sauvegarde manuelle : meilleur genome → chemin
    void sauvegarder(const std::string& chemin) const;
    // chargement manuel (ou depuis auto-save) : retourne true si réussi
    bool charger(const std::string& chemin);

private:
    static const std::string CHEMIN_AUTO;  // fichier de progression automatique

    int taille_population_;
    int parties_par_agent_;
    std::atomic<bool> en_cours_{false};
    std::jthread fil_;
    mutable std::mutex mutex_;
    StatistiquesGeneration stat_courante_;
    std::vector<Agent> population_;
    float meilleur_score_absolu_{0.0f};
    Genome meilleur_genome_absolu_{};
    int generation_offset_{0};

    void boucle(std::stop_token stop, std::function<void(const StatistiquesGeneration&)> cb);
    float evaluer_agent(const Agent& agent, std::mt19937& rng, EtatJeu& dernier_jeu) const;
    void evoluer(std::vector<std::pair<float, Agent>>& resultats, std::mt19937& rng);
    Agent selection_tournoi(const std::vector<std::pair<float, Agent>>& resultats,
                            int taille_tournoi, std::mt19937& rng) const;
    Agent croiser_blx(const Agent& p1, const Agent& p2, std::mt19937& rng) const;
    void muter(Agent& agent, std::mt19937& rng) const;
    void initialiser_population(std::mt19937& rng);

    // sauvegarde complète (population + meilleurs) appelée depuis boucle()
    void sauvegarder_etat(const std::string& chemin, int gen) const;
};
