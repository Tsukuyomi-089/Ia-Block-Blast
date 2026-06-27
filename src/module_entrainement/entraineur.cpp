#include "entraineur.hpp"
#include <algorithm>
#include <fstream>
#include <future>
#include <cstdlib>

// Fichier de progression automatique dans le répertoire home de l'utilisateur.
const std::string Entraineur::CHEMIN_AUTO = [] {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/ia_bb_sauvegarde.txt" : "ia_bb_sauvegarde.txt";
}();

Entraineur::Entraineur(int taille_population, int parties_par_agent)
    : taille_population_(taille_population)
    , parties_par_agent_(parties_par_agent)
{}

void Entraineur::demarrer(std::function<void(const StatistiquesGeneration&)> callback) {
    if (en_cours_) return;
    en_cours_ = true;
    fil_ = std::jthread([this, cb = std::move(callback)](std::stop_token stop) {
        boucle(stop, cb);
    });
}

void Entraineur::arreter() {
    if (!en_cours_) return;
    en_cours_ = false;
    fil_.request_stop();
    if (fil_.joinable()) fil_.join();
}

bool Entraineur::en_cours() const { return en_cours_; }

StatistiquesGeneration Entraineur::derniere_stat() const {
    std::lock_guard lock(mutex_);
    return stat_courante_;
}

void Entraineur::initialiser_population(std::mt19937& rng) {
    // Plages adaptées à la progression quadratique sum(n²)
    const std::array<float, NB_CRITERES> lo = {5.0f, 0.2f, 0.2f, 0.0f};
    const std::array<float, NB_CRITERES> hi = {18.0f, 2.0f, 2.0f, 1.5f};

    population_.clear();
    population_.reserve(taille_population_);
    // Agent de référence calibré pour Block Blast avec progression quadratique
    population_.emplace_back(Genome{10.0f, 0.6f, 0.6f, 0.3f});
    for (int i = 1; i < taille_population_; ++i) {
        Genome g;
        for (int k = 0; k < NB_CRITERES; ++k) {
            std::uniform_real_distribution<float> dist(lo[k], hi[k]);
            g[k] = dist(rng);
        }
        population_.emplace_back(g);
    }
}

float Entraineur::evaluer_agent(const Agent& agent, std::mt19937& rng, EtatJeu& dernier_jeu) const {
    float total = 0.0f;
    Jeu jeu(rng);
    for (int i = 0; i < parties_par_agent_; ++i) {
        jeu.reinitialiser();
        while (jeu.peut_jouer()) {
            auto coup = agent.choisir_coup(jeu.etat());
            if (coup.index_piece < 0) break;
            jeu.jouer_coup(coup.index_piece, coup.ligne, coup.colonne);
        }
        total += static_cast<float>(jeu.etat().score);
    }
    dernier_jeu = jeu.etat();
    return total / static_cast<float>(parties_par_agent_);
}

Agent Entraineur::selection_tournoi(const std::vector<std::pair<float, Agent>>& resultats,
                                     int taille_tournoi, std::mt19937& rng) const {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(resultats.size()) - 1);
    int meilleur = dist(rng);
    for (int i = 1; i < taille_tournoi; ++i) {
        int c = dist(rng);
        if (resultats[c].first > resultats[meilleur].first) meilleur = c;
    }
    return resultats[meilleur].second;
}

Agent Entraineur::croiser_blx(const Agent& p1, const Agent& p2, std::mt19937& rng) const {
    // BLX-0.3 : explore légèrement au-delà des bornes des parents → meilleure exploration
    constexpr float ALPHA = 0.3f;
    Genome enfant;
    for (int i = 0; i < NB_CRITERES; ++i) {
        float a = p1.genome()[i], b = p2.genome()[i];
        float lo = std::min(a, b), hi = std::max(a, b);
        float ext = ALPHA * (hi - lo);
        std::uniform_real_distribution<float> dist(lo - ext, hi + ext);
        enfant[i] = std::max(0.0f, dist(rng));
    }
    return Agent(enfant);
}

void Entraineur::muter(Agent& agent, std::mt19937& rng) const {
    // Mutation proportionnelle : σ = 15% de la valeur → adapte l'intensité à l'échelle du poids
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    Genome g = agent.genome();
    for (auto& v : g) {
        if (prob(rng) < 0.25f) {
            float sigma = std::max(0.05f, std::abs(v) * 0.15f);
            std::normal_distribution<float> bruit(0.0f, sigma);
            v = std::max(0.0f, v + bruit(rng));
        }
    }
    agent.genome() = g;
}

void Entraineur::evoluer(std::vector<std::pair<float, Agent>>& resultats, std::mt19937& rng) {
    std::sort(resultats.begin(), resultats.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    population_.clear();

    // Élitisme : top 20% passent intacts
    int elite = std::max(1, taille_population_ / 5);
    for (int i = 0; i < elite; ++i)
        population_.push_back(resultats[i].second);

    // Reste : sélection tournoi + croisement BLX + mutation
    while (static_cast<int>(population_.size()) < taille_population_) {
        Agent p1 = selection_tournoi(resultats, 3, rng);
        Agent p2 = selection_tournoi(resultats, 3, rng);
        Agent enfant = croiser_blx(p1, p2, rng);
        muter(enfant, rng);
        population_.push_back(std::move(enfant));
    }
}

// Format de sauvegarde complet : lisible, robuste, extensible
void Entraineur::sauvegarder_etat(const std::string& chemin, int gen) const {
    std::ofstream f(chemin);
    if (!f) return;
    f << "ncriteres " << NB_CRITERES << "\n";
    f << "gen " << gen << "\n";
    f << "record_score " << meilleur_score_absolu_ << "\n";
    f << "record_genome";
    for (float v : meilleur_genome_absolu_) f << " " << v;
    f << "\n";
    f << "pop_size " << population_.size() << "\n";
    for (const Agent& a : population_) {
        for (float v : a.genome()) f << v << " ";
        f << "\n";
    }
}

void Entraineur::sauvegarder(const std::string& chemin) const {
    // Sauvegarde publique (depuis le thread principal) : lit stat_courante_ sous mutex.
    StatistiquesGeneration stat;
    {
        std::lock_guard lock(mutex_);
        stat = stat_courante_;
    }
    std::ofstream f(chemin);
    if (!f) return;
    // Format simple : juste les 5 poids du genome record
    for (float v : stat.meilleur_genome_absolu) f << v << "\n";
}

bool Entraineur::charger(const std::string& chemin) {
    std::ifstream f(chemin);
    if (!f) return false;

    std::string tag;
    int gen = 0;
    float score = 0.0f;
    Genome best_g{};
    int pop_size = 0;

    // Vérification de compatibilité : rejeter les fichiers avec un NB_CRITERES différent
    int nc = 0;
    if (!(f >> tag >> nc)) return false;       // "ncriteres N"
    if (tag != "ncriteres" || nc != NB_CRITERES) return false;

    if (!(f >> tag >> gen))    return false;   // "gen N"
    if (!(f >> tag >> score))  return false;   // "record_score S"
    if (!(f >> tag))           return false;   // "record_genome"
    for (float& v : best_g) if (!(f >> v)) return false;
    if (!(f >> tag >> pop_size)) return false; // "pop_size N"

    std::vector<Agent> pop;
    pop.reserve(pop_size);
    for (int i = 0; i < pop_size; ++i) {
        Genome g{};
        bool ok = true;
        for (float& v : g) if (!(f >> v)) { ok = false; break; }
        if (!ok) break;
        pop.emplace_back(g);
    }

    if (pop.empty()) return false;

    meilleur_score_absolu_  = score;
    meilleur_genome_absolu_ = best_g;
    population_             = std::move(pop);
    generation_offset_      = gen;
    return true;
}

void Entraineur::boucle(std::stop_token stop,
                         std::function<void(const StatistiquesGeneration&)> cb) {
    std::mt19937 rng(std::random_device{}());

    // Chargement automatique de la progression précédente.
    bool reprise = false;
    if (population_.empty()) {
        reprise = charger(CHEMIN_AUTO);
        if (!reprise)
            initialiser_population(rng);
    }

    int generation = generation_offset_;

    while (!stop.stop_requested()) {
        ++generation;

        {
            std::lock_guard lock(mutex_);
            stat_courante_.generation = generation;
        }

        // Évaluation parallèle : un thread par agent
        std::vector<std::future<std::pair<float, EtatJeu>>> futurs;
        futurs.reserve(population_.size());

        for (size_t i = 0; i < population_.size(); ++i) {
            const Agent& agent = population_[i];
            futurs.push_back(std::async(std::launch::async,
                [this, &agent, i]() -> std::pair<float, EtatJeu> {
                    std::mt19937 rng_local(
                        std::random_device{}() ^ static_cast<unsigned>(i) * 2654435761u);
                    EtatJeu dj;
                    float s = evaluer_agent(agent, rng_local, dj);
                    return {s, dj};
                }));
        }

        std::vector<std::pair<float, Agent>> resultats;
        resultats.reserve(population_.size());

        StatistiquesGeneration stat;
        stat.generation          = generation;
        stat.premiere_generation = (generation == generation_offset_ + 1);

        for (size_t i = 0; i < futurs.size(); ++i) {
            auto [score, dj] = futurs[i].get();
            resultats.push_back({score, population_[i]});
            if (score > stat.meilleur_score) {
                stat.meilleur_score  = score;
                stat.meilleur_genome = population_[i].genome();
                stat.dernier_jeu     = dj;
            }
        }

        float somme = 0.0f;
        for (const auto& [s, _] : resultats) somme += s;
        stat.score_moyen = somme / static_cast<float>(resultats.size());

        // Record absolu : ne diminue jamais
        if (stat.meilleur_score > meilleur_score_absolu_) {
            meilleur_score_absolu_  = stat.meilleur_score;
            meilleur_genome_absolu_ = stat.meilleur_genome;
        }
        stat.meilleur_score_absolu   = meilleur_score_absolu_;
        stat.meilleur_genome_absolu  = meilleur_genome_absolu_;

        {
            std::lock_guard lock(mutex_);
            stat_courante_ = stat;
        }

        // Sauvegarde automatique après chaque génération.
        sauvegarder_etat(CHEMIN_AUTO, generation);

        if (cb) cb(stat);

        evoluer(resultats, rng);
    }

    en_cours_ = false;
}
