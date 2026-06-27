#pragma once
#include "module_entrainement/entraineur.hpp"
#include "module_adb/gestionnaire_adb.hpp"
#include "module_vision/analyseur_ecran.hpp"
#include "module_ia/agent.hpp"
#include <opencv2/opencv.hpp>
#include <deque>
#include <mutex>
#include <atomic>
#include <string>
#include <thread>

enum class Mode { SIMULATION, REEL };

class Interface {
public:
    Interface();
    ~Interface();
    void lancer();

private:
    static constexpr int LARGEUR        = 1200;
    static constexpr int HAUTEUR        = 800;
    static constexpr int LARGEUR_G      = 260;
    static constexpr int LARGEUR_C      = 460;
    static constexpr int TAILLE_CASE    = 50;
    static constexpr int MARGE_GRILLE   = 20;
    static constexpr int MAX_LIGNES_LOG = 20;

    // Les mutexes doivent être déclarés AVANT les threads qui les utilisent.
    // En C++ les membres sont détruits dans l'ordre INVERSE de déclaration,
    // donc les mutexes (déclarés en premier) seront détruits EN DERNIER,
    // après que tous les threads les utilisant aient rejoint.
    mutable std::mutex mutex_log_;
    mutable std::mutex mutex_apercu_;

    // Données simples
    Mode mode_{Mode::SIMULATION};
    std::atomic<bool> actif_{false};
    cv::Mat canvas_;
    std::deque<std::string> journal_;
    cv::Mat apercu_ecran_;

    // Objets métier (pas de threads internes, sauf Entraineur)
    GestionnaireAdb adb_;
    AnalyseurEcran analyseur_;
    Agent agent_courant_;

    // Threads — déclarés EN DERNIER → détruits EN PREMIER (avant les mutexes).
    std::jthread fil_reel_;
    Entraineur entraineur_;

    cv::Rect btn_sim_     {10,  50, 110, 35};
    cv::Rect btn_reel_    {130, 50, 110, 35};
    cv::Rect btn_lancer_  {10, 100, 110, 40};
    cv::Rect btn_arreter_ {130,100, 110, 40};
    cv::Rect btn_sauv_    {10, 155, 110, 35};
    cv::Rect btn_conn_    {130,155, 110, 35};
    cv::Rect btn_capture_ {10, 205, 240, 55};

    void dessiner();
    void dessiner_panneau_gauche();
    void dessiner_panneau_central(const StatistiquesGeneration& stat);
    void dessiner_panneau_droit();
    void dessiner_grille(const Grille& grille, cv::Point origine);
    void dessiner_piece(const Piece& piece, cv::Point origine, int taille_case);
    void dessiner_bouton(const std::string& texte, cv::Rect zone,
                         cv::Scalar couleur, bool actif = true);

    void on_clic(int x, int y);
    static void cb_souris(int event, int x, int y, int flags, void* data);

    void ajouter_log(const std::string& msg);
    void demarrer_simulation();
    void arreter_simulation();
    void demarrer_reel();
    void arreter_reel();
    void boucle_reel(std::stop_token stop);
    void calibrer();
    void capturer_et_afficher();

    bool dans(const cv::Rect& zone, int x, int y) const;
};
