#include "interface.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace {
    // Racine du projet déterminée à la compilation via __FILE__
    // __FILE__ = .../src/interface/interface.cpp → remonter 3 niveaux
    const std::string RACINE_PROJET =
        std::filesystem::path(__FILE__).parent_path()  // interface/
                                       .parent_path()  // src/
                                       .parent_path()  // project root
                                       .string();
    const std::string CHEMIN_LOG = RACINE_PROJET + "/ia_bb.log";
}

namespace {
    cv::Scalar FOND         {25,  25,  25};
    cv::Scalar FOND_PANNEAU {40,  40,  40};
    cv::Scalar VERT         {50, 200,  50};
    cv::Scalar VERT_SOMBRE  {30, 100,  30};
    cv::Scalar ROUGE        {50,  50, 200};
    cv::Scalar BLEU         {200, 100,  50};
    cv::Scalar GRIS         {120, 120, 120};
    cv::Scalar BLANC        {220, 220, 220};
    cv::Scalar OR           {50, 200, 255};
    cv::Scalar CASE_VIDE    {60,  60,  60};
    cv::Scalar CASE_REMPLIE {100, 180, 255};
    cv::Scalar BORDURE      {80,  80,  80};
}

Interface::Interface() : agent_courant_({10.0f, 0.6f, 0.6f, 0.3f}) {
    canvas_ = cv::Mat(HAUTEUR, LARGEUR, CV_8UC3);
}

Interface::~Interface() {
    // Arrêt explicite des threads avant la destruction des membres.
    entraineur_.arreter();
    actif_ = false;
    fil_reel_.request_stop();
}

bool Interface::dans(const cv::Rect& zone, int x, int y) const {
    return zone.contains(cv::Point(x, y));
}

void Interface::ajouter_log(const std::string& msg) {
    std::lock_guard lock(mutex_log_);
    journal_.push_front(msg);
    while (static_cast<int>(journal_.size()) > MAX_LIGNES_LOG)
        journal_.pop_back();
    std::ofstream f(CHEMIN_LOG, std::ios::app);
    if (f) f << msg << "\n";
}

void Interface::dessiner_bouton(const std::string& texte, cv::Rect zone,
                                 cv::Scalar couleur, bool actif) {
    cv::Scalar c = actif ? couleur : GRIS;
    cv::rectangle(canvas_, zone, c, -1);
    cv::rectangle(canvas_, zone, BLANC, 1);
    int baseline = 0;
    auto taille = cv::getTextSize(texte, cv::FONT_HERSHEY_SIMPLEX, 0.45, 1, &baseline);
    cv::Point pos(zone.x + (zone.width - taille.width) / 2,
                  zone.y + (zone.height + taille.height) / 2);
    cv::putText(canvas_, texte, pos, cv::FONT_HERSHEY_SIMPLEX, 0.45, BLANC, 1);
}

void Interface::dessiner_grille(const Grille& grille, cv::Point origine) {
    for (int r = 0; r < TAILLE_GRILLE; ++r) {
        for (int c = 0; c < TAILLE_GRILLE; ++c) {
            cv::Point tl(origine.x + c * TAILLE_CASE, origine.y + r * TAILLE_CASE);
            cv::Point br(tl.x + TAILLE_CASE - 1, tl.y + TAILLE_CASE - 1);
            cv::Scalar couleur = grille.est_remplie(r, c) ? CASE_REMPLIE : CASE_VIDE;
            cv::rectangle(canvas_, tl, br, couleur, -1);
            cv::rectangle(canvas_, tl, br, BORDURE, 1);
        }
    }
}

void Interface::dessiner_piece(const Piece& piece, cv::Point origine, int taille_case) {
    for (const auto& c : piece.cellules) {
        cv::Point tl(origine.x + c.colonne * taille_case,
                     origine.y + c.ligne * taille_case);
        cv::Point br(tl.x + taille_case - 2, tl.y + taille_case - 2);
        cv::rectangle(canvas_, tl, br, CASE_REMPLIE, -1);
        cv::rectangle(canvas_, tl, br, BORDURE, 1);
    }
}

void Interface::dessiner_panneau_gauche() {
    cv::rectangle(canvas_, {0, 0, LARGEUR_G, HAUTEUR}, FOND_PANNEAU, -1);

    cv::putText(canvas_, "IA BLOCK BLAST", {10, 35},
                cv::FONT_HERSHEY_SIMPLEX, 0.7, OR, 2);

    dessiner_bouton("SIMULATION", btn_sim_,
                    mode_ == Mode::SIMULATION ? BLEU : GRIS);
    dessiner_bouton("REEL",       btn_reel_,
                    mode_ == Mode::REEL ? BLEU : GRIS);

    bool en_marche = entraineur_.en_cours() || actif_;
    dessiner_bouton("Lancer",   btn_lancer_,  VERT,  !en_marche);
    dessiner_bouton("Arreter",  btn_arreter_, ROUGE, en_marche);
    // En mode réel : "Sauver" devient "Calibrer"
    if (mode_ == Mode::REEL)
        dessiner_bouton("Calibrer", btn_sauv_, GRIS);
    else
        dessiner_bouton("Sauver",   btn_sauv_, GRIS);
    dessiner_bouton("Connecter", btn_conn_,
                    adb_.est_connecte() ? VERT_SOMBRE : GRIS);

    // Gros bouton rouge de capture
    cv::Scalar rouge_vif{30, 30, 220};
    cv::rectangle(canvas_, btn_capture_, rouge_vif, -1);
    cv::rectangle(canvas_, btn_capture_, BLANC, 2);
    {
        const std::string label = "CAPTURER ECRAN";
        int bl = 0;
        auto tsz = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.55, 2, &bl);
        cv::Point pos(btn_capture_.x + (btn_capture_.width - tsz.width) / 2,
                      btn_capture_.y + (btn_capture_.height + tsz.height) / 2);
        cv::putText(canvas_, label, pos, cv::FONT_HERSHEY_SIMPLEX, 0.55, BLANC, 2);
    }

    int y = 210;
    auto ligne = [&](const std::string& s, cv::Scalar couleur = BLANC) {
        cv::putText(canvas_, s, {10, y}, cv::FONT_HERSHEY_SIMPLEX, 0.42, couleur, 1);
        y += 22;
    };

    bool connecte = adb_.est_connecte();
    ligne("Connexion: " + (connecte ? adb_.appareil_actuel() : "Aucun"),
          connecte ? VERT : ROUGE);

    if (mode_ == Mode::REEL) {
        bool cfg = analyseur_.config_valide();
        ligne("Config:   " + std::string(cfg ? "OK" : "MANQUANTE"),
              cfg ? VERT : ROUGE);
    }

    StatistiquesGeneration stat = entraineur_.derniere_stat();
    ligne("Generation:  " + std::to_string(stat.generation));
    y += 4;
    ligne("RECORD ABS:  " + std::to_string(static_cast<int>(stat.meilleur_score_absolu)), OR);
    ligne("Gen best:    " + std::to_string(static_cast<int>(stat.meilleur_score)));
    ligne("Moyen:       " + std::to_string(static_cast<int>(stat.score_moyen)));

    if (stat.generation > 0) {
        y += 8;
        ligne("Genome record:", OR);
        const Genome& g = stat.meilleur_genome_absolu;
        ligne(std::format("  lig={:.2f}", g[0]));
        ligne(std::format("  pL={:.3f} pC={:.3f}", g[1], g[2]));
        ligne(std::format("  iso={:.3f}", g[3]));
    }
}

void Interface::dessiner_panneau_central(const StatistiquesGeneration& stat) {
    int x0 = LARGEUR_G;
    cv::rectangle(canvas_, {x0, 0, LARGEUR_C, HAUTEUR}, FOND, -1);

    cv::putText(canvas_, "Etat du jeu", {x0 + 10, 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.6, BLANC, 1);

    cv::Point origine_grille(x0 + MARGE_GRILLE, 50);
    dessiner_grille(stat.dernier_jeu.grille, origine_grille);

    int score_y = 50 + TAILLE_GRILLE * TAILLE_CASE + 25;
    cv::putText(canvas_,
                "Score: " + std::to_string(stat.dernier_jeu.score),
                {x0 + 10, score_y},
                cv::FONT_HERSHEY_SIMPLEX, 0.55, OR, 1);

    cv::putText(canvas_, "Pieces courantes:", {x0 + 10, score_y + 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.45, BLANC, 1);

    int px = x0 + MARGE_GRILLE;
    for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
        if (!stat.dernier_jeu.pieces_utilisees[i]) {
            dessiner_piece(stat.dernier_jeu.pieces_courantes[i],
                           {px, score_y + 50}, 18);
        }
        px += 140;
    }
}

void Interface::dessiner_panneau_droit() {
    int x0 = LARGEUR_G + LARGEUR_C;
    int largeur = LARGEUR - x0;

    cv::rectangle(canvas_, {x0, 0, largeur, HAUTEUR}, FOND_PANNEAU, -1);

    cv::putText(canvas_, "Journal", {x0 + 10, 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.6, BLANC, 1);

    cv::line(canvas_, {x0, 40}, {LARGEUR, 40}, GRIS, 1);

    {
        std::lock_guard lock(mutex_log_);
        int y = 65;
        for (const auto& msg : journal_) {
            cv::putText(canvas_, msg, {x0 + 8, y},
                        cv::FONT_HERSHEY_SIMPLEX, 0.38, BLANC, 1);
            y += 18;
            if (y > HAUTEUR - 250) break;
        }
    }

    int apercu_y = HAUTEUR - 230;
    cv::line(canvas_, {x0, apercu_y}, {LARGEUR, apercu_y}, GRIS, 1);
    cv::putText(canvas_, "Apercu ecran", {x0 + 10, apercu_y + 20},
                cv::FONT_HERSHEY_SIMPLEX, 0.45, BLANC, 1);

    {
        std::lock_guard lock(mutex_apercu_);
        if (!apercu_ecran_.empty()) {
            cv::Mat redim;
            cv::resize(apercu_ecran_, redim, {largeur - 10, 200});
            redim.copyTo(canvas_(cv::Rect(x0 + 5, apercu_y + 30, largeur - 10, 200)));
        } else {
            cv::rectangle(canvas_,
                          {x0 + 5, apercu_y + 30, largeur - 10, 200},
                          CASE_VIDE, -1);
            cv::putText(canvas_, "Aucun apercu",
                        {x0 + largeur / 2 - 60, apercu_y + 135},
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, GRIS, 1);
        }
    }
}

void Interface::dessiner() {
    canvas_.setTo(FOND);
    StatistiquesGeneration stat = entraineur_.derniere_stat();
    dessiner_panneau_gauche();
    dessiner_panneau_central(stat);
    dessiner_panneau_droit();
}

void Interface::demarrer_simulation() {
    if (entraineur_.en_cours()) return;
    ajouter_log("Demarrage de l'entrainement...");
    entraineur_.demarrer([this](const StatistiquesGeneration& stat) {
        // Message de reprise uniquement à la première génération
        if (stat.premiere_generation) {
            if (stat.meilleur_score_absolu > 0.0f)
                ajouter_log(std::format("=== REPRISE gen {}, record {:.0f}",
                                        stat.generation, stat.meilleur_score_absolu));
            else
                ajouter_log("=== Nouvelle session d'entrainement");
        }
        ajouter_log(std::format("Gen {:3d} | Rec:{:5.0f} | Gen:{:5.0f} | Moy:{:4.0f}",
                                stat.generation,
                                stat.meilleur_score_absolu,
                                stat.meilleur_score,
                                stat.score_moyen));
    });
}

void Interface::arreter_simulation() {
    if (!entraineur_.en_cours()) return;
    ajouter_log("Arret en cours...");
    entraineur_.arreter();  // envoie stop + attend la fin du thread
    ajouter_log("Entrainement arrete.");
}

void Interface::demarrer_reel() {
    if (actif_) return;
    if (!adb_.est_connecte()) {
        ajouter_log("Erreur: aucun appareil connecte.");
        return;
    }
    // Vider le fichier log à chaque nouvelle session
    { std::ofstream f(CHEMIN_LOG, std::ios::trunc); }
    actif_ = true;
    ajouter_log("Mode reel demarre.");
    fil_reel_ = std::jthread([this](std::stop_token stop) {
        boucle_reel(stop);
    });
}

void Interface::arreter_reel() {
    actif_ = false;
    fil_reel_.request_stop();
    if (fil_reel_.joinable()) fil_reel_.join();
    ajouter_log("Mode reel arrete.");
}

void Interface::calibrer() {
    const std::string config_ex = std::string(std::getenv("HOME")) + "/ia_bb_config_exemple.txt";
    const std::string screenshot = std::string(std::getenv("HOME")) + "/ia_bb_screenshot.png";

    AnalyseurEcran::creer_config_exemple(config_ex);

    if (adb_.est_connecte()) {
        if (adb_.capturer_ecran(screenshot))
            ajouter_log("Screenshot sauvegarde: ~/ia_bb_screenshot.png");
        else
            ajouter_log("Echec screenshot ADB.");
    } else {
        ajouter_log("Connectez d'abord l'appareil ADB.");
    }

    ajouter_log("Exemple config: ~/ia_bb_config_exemple.txt");
    ajouter_log("--- Etapes ---");
    ajouter_log("1. Ouvrir screenshot dans GIMP");
    ajouter_log("2. Mesurer la grille (x y larg haut)");
    ajouter_log("3. Mesurer le centre de chaque piece");
    ajouter_log("4. Copier exemple → ~/ia_bb_config.txt");
    ajouter_log("5. Editer les coordonnees");
    ajouter_log("6. Cliquer Connecter pour recharger");
}

void Interface::capturer_et_afficher() {
    if (!adb_.est_connecte()) {
        ajouter_log("Capture: aucun appareil connecte.");
        return;
    }

    const std::string tmp = "/tmp/ia_bb_capture_debug.png";
    ajouter_log("Capture en cours...");

    if (!adb_.capturer_ecran(tmp)) {
        ajouter_log("Echec capture ADB.");
        return;
    }

    cv::Mat img = cv::imread(tmp);
    if (img.empty()) {
        ajouter_log("Image vide ou corrompue.");
        return;
    }

    ajouter_log(std::format("Image: {}x{}", img.cols, img.rows));

    // Analyse si config valide
    if (analyseur_.config_valide()) {
        auto res = analyseur_.analyser(img);
        if (res) {
            // Overlay grille
            cv::Mat dbg = analyseur_.visualiser(img, *res);

            // Colorier les cases remplies en rouge semi-transparent
            const cv::Rect& z = res->zone_grille;
            int lc = z.width  / TAILLE_GRILLE;
            int hc = z.height / TAILLE_GRILLE;
            for (int r = 0; r < TAILLE_GRILLE; ++r)
                for (int c = 0; c < TAILLE_GRILLE; ++c)
                    if (res->grille.est_remplie(r, c))
                        cv::rectangle(dbg,
                            {z.x + c*lc + 2, z.y + r*hc + 2, lc-4, hc-4},
                            {0, 0, 255}, -1);

            // Bbox de chaque pièce détectée
            for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
                const cv::Rect& bb = res->bbox_pieces[i];
                cv::rectangle(dbg, bb, {0, 165, 255}, 4);
                cv::putText(dbg,
                    std::format("P{}: {}", i, res->pieces[i].nom),
                    {bb.x, bb.y - 10},
                    cv::FONT_HERSHEY_SIMPLEX, 1.5, {0, 165, 255}, 3);
            }

            // Sauvegarder
            const std::string out = std::string(std::getenv("HOME")) + "/ia_bb_vue.png";
            cv::imwrite(out, dbg);

            int rempli = 0;
            for (int r = 0; r < TAILLE_GRILLE; ++r)
                for (int c = 0; c < TAILLE_GRILLE; ++c)
                    if (res->grille.est_remplie(r, c)) ++rempli;

            ajouter_log(std::format("Grille: {}/64 cases remplies", rempli));
            for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i)
                ajouter_log(std::format("  P{}: {} ({}x{})",
                    i, res->pieces[i].nom,
                    res->pieces[i].largeur, res->pieces[i].hauteur));
            ajouter_log("Vue sauvegardee: ~/ia_bb_vue.png");

            std::lock_guard lock(mutex_apercu_);
            apercu_ecran_ = dbg.clone();
        } else {
            ajouter_log("Analyse echouee (verifiez config).");
            std::lock_guard lock(mutex_apercu_);
            apercu_ecran_ = img.clone();
        }
    } else {
        ajouter_log("Config absente — affichage brut.");
        std::lock_guard lock(mutex_apercu_);
        apercu_ecran_ = img.clone();
    }
}

void Interface::boucle_reel(std::stop_token stop) {
    // Vérifier la config avant de boucler
    if (!analyseur_.config_valide()) {
        ajouter_log("=== CONFIG MANQUANTE ===");
        ajouter_log("Cliquez 'Calibrer' pour les instructions.");
        actif_ = false;
        return;
    }

    const std::string chemin_capture = "/tmp/ia_bb_capture.png";
    ajouter_log("Mode reel demarre.");

    // Pièces blacklistées cette session : si une pièce échoue 6 fois de suite,
    // on la marque comme "utilisée" pour forcer l'agent à essayer une autre pièce.
    // Reset global quand un coup réussit ou quand toutes les pièces sont blacklistées.
    std::array<bool, NB_PIECES_SIMULTANEES> pieces_bloquees{false, false, false};

    // Correction apprise coup après coup et persistée sur disque.
    // Si pas de fichier de calibration : on part du decalage_doigt_y du config
    // (valeur qui fait atterrir la pièce quelque part, même si pas exactement là).
    int correction_x = 0;
    int correction_y = 0;
    const std::string chemin_corr = std::string(std::getenv("HOME")) + "/ia_bb_correction.txt";
    {
        std::ifstream f(chemin_corr);
        if (f) {
            std::string cle; int val;
            while (f >> cle >> val) {
                if (cle == "correction_x") correction_x = val;
                if (cle == "correction_y") correction_y = val;
            }
            ajouter_log(std::format("Calibration chargee: corrX={} corrY={}", correction_x, correction_y));
        } else {
            correction_y = analyseur_.decalage_doigt_y();
            ajouter_log(std::format("Pas de calibration — decalage initial corrY={}", correction_y));
        }
    }

    while (!stop.stop_requested() && actif_) {
        // Capture
        if (!adb_.capturer_ecran(chemin_capture)) {
            ajouter_log("Echec capture ADB.");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        cv::Mat capture = cv::imread(chemin_capture);
        if (capture.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        {
            std::lock_guard lock(mutex_apercu_);
            apercu_ecran_ = capture.clone();
        }

        // Analyse
        auto resultat = analyseur_.analyser(capture);
        if (!resultat) {
            ajouter_log("Analyse echouee (verifiez config).");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // Construire l'état de jeu depuis l'analyse
        EtatJeu etat;
        etat.grille           = resultat->grille;
        etat.pieces_courantes = resultat->pieces;
        etat.score    = 0;
        etat.termine  = false;
        etat.nb_coups = 0;
        // Marquer les slots vides (pièce déjà jouée) comme utilisés
        for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i)
            etat.pieces_utilisees[i] = !resultat->pieces_presentes[i];
        // Blacklist pièces : si une pièce a complètement échoué (6 rejets),
        // on la marque utilisée pour forcer l'agent à essayer une autre pièce.
        for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i)
            if (pieces_bloquees[i]) etat.pieces_utilisees[i] = true;

        {
            // Log toujours l'état des 3 slots pour diagnostiquer les détections fausses
            std::string slots;
            int nb_present = 0;
            for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
                if (resultat->pieces_presentes[i]) {
                    slots += std::format("P{}={} ", i, etat.pieces_courantes[i].nom);
                    ++nb_present;
                } else {
                    slots += std::format("P{}=VIDE ", i);
                }
            }
            ajouter_log(std::format("Slots ({}/3): {}", nb_present, slots));
        }

        // Choisir le meilleur coup
        auto coup = agent_courant_.choisir_coup(etat);
        if (coup.index_piece < 0) {
            int nb_remplis = 0;
            std::string carte;
            for (int r = 0; r < TAILLE_GRILLE; ++r) {
                for (int c = 0; c < TAILLE_GRILLE; ++c) {
                    bool plein = etat.grille.est_remplie(r, c);
                    carte += plein ? '#' : '.';
                    if (plein) ++nb_remplis;
                }
                carte += '|';
            }
            ajouter_log(std::format("Aucun coup ({}/64 pleines) pieces: {} {} {}",
                nb_remplis,
                etat.pieces_courantes[0].nom,
                etat.pieces_courantes[1].nom,
                etat.pieces_courantes[2].nom));
            ajouter_log("Grille: " + carte);

            // Diagnostic HSV : log les valeurs S*V réelles de chaque cellule
            // pour trouver le bon seuil
            {
                const cv::Rect& zone = resultat->zone_grille;
                int lc2 = zone.width  / TAILLE_GRILLE;
                int hc2 = zone.height / TAILLE_GRILLE;
                cv::Mat region_hsv;
                cv::cvtColor(capture(zone), region_hsv, cv::COLOR_BGR2HSV);
                std::vector<cv::Mat> canaux;
                cv::split(region_hsv, canaux);

                double sv_min = 1.0, sv_max = 0.0;
                std::string sv_carte;
                for (int r = 0; r < TAILLE_GRILLE; ++r) {
                    for (int c = 0; c < TAILLE_GRILLE; ++c) {
                        cv::Rect z(c*lc2+4, r*hc2+4, lc2-8, hc2-8);
                        z &= cv::Rect(0, 0, region_hsv.cols, region_hsv.rows);
                        double mS = cv::mean(canaux[1](z))[0] / 255.0;
                        double mV = cv::mean(canaux[2](z))[0] / 255.0;
                        double sv = mS * mV;
                        sv_min = std::min(sv_min, sv);
                        sv_max = std::max(sv_max, sv);
                        sv_carte += std::format("{:.2f} ", sv);
                    }
                    ajouter_log(std::format("HSV L{}: {}", r, sv_carte));
                    sv_carte.clear();
                }
                ajouter_log(std::format("S*V range: min={:.3f} max={:.3f} seuil={:.2f}",
                    sv_min, sv_max, analyseur_.seuil_remplissage()));

                // Image de debug
                cv::Mat dbg = capture.clone();
                cv::rectangle(dbg, zone, {0,255,0}, 3);
                for (int r = 0; r < TAILLE_GRILLE; ++r)
                    for (int c = 0; c < TAILLE_GRILLE; ++c)
                        if (etat.grille.est_remplie(r, c))
                            cv::rectangle(dbg,
                                {zone.x+c*lc2+2, zone.y+r*hc2+2, lc2-4, hc2-4},
                                {0,0,200}, -1);
                const std::string dp = std::string(std::getenv("HOME")) + "/ia_bb_debug.png";
                cv::imwrite(dp, dbg);
            }

            // Lever les blacklists : peut-être qu'elles cachaient des pièces valides
            pieces_bloquees.fill(false);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        const cv::Rect& zone = resultat->zone_grille;
        int lc = zone.width  / TAILLE_GRILLE;
        int hc = zone.height / TAILLE_GRILLE;

        const Piece& piece   = etat.pieces_courantes[coup.index_piece];
        const cv::Rect& bbox = resultat->bbox_pieces[coup.index_piece];

        // Cellule d'ancrage : la case pleine la plus proche du centroïde.
        // Garantit qu'on saisit une case réelle même pour les pièces avec [0,0] vide.
        float cr = 0.f, cc = 0.f;
        for (const auto& cell : piece.cellules) { cr += cell.ligne; cc += cell.colonne; }
        cr /= piece.cellules.size(); cc /= piece.cellules.size();
        Cellule ancre = piece.cellules.front();
        float best_d = 1e9f;
        for (const auto& cell : piece.cellules) {
            float d = (cell.ligne-cr)*(cell.ligne-cr) + (cell.colonne-cc)*(cell.colonne-cc);
            if (d < best_d) { best_d = d; ancre = cell; }
        }

        // Grab sur la cellule d'ancrage (toujours une case non vide)
        float cw = (piece.largeur  > 0) ? float(bbox.width)  / piece.largeur  : float(bbox.width);
        float ch = (piece.hauteur > 0) ? float(bbox.height) / piece.hauteur : float(bbox.height);
        int grab_x = int(bbox.x + (ancre.colonne + 0.5f) * cw);
        int grab_y = int(bbox.y + (ancre.ligne   + 0.5f) * ch);

        // Destination : la cellule d'ancrage doit atterrir en (coup.ligne+ancre.ligne, coup.colonne+ancre.colonne)
        // + correction apprise par auto-calibration après chaque coup.
        // On clample dans les limites de la grille pour éviter que le doigt sorte du bord.
        int dest_x_base = zone.x + (coup.colonne + ancre.colonne) * lc + lc / 2 + correction_x;
        int dest_y_base = zone.y + (coup.ligne   + ancre.ligne)   * hc + hc / 2 + correction_y;
        dest_x_base = std::clamp(dest_x_base, zone.x + lc/2, zone.x + zone.width  - lc/2);
        dest_y_base = std::clamp(dest_y_base, zone.y + hc/2, zone.y + zone.height - hc/2);
        int dest_x = dest_x_base;
        int dest_y = dest_y_base;

        // Boucle d'essais : si la pièce est rejetée (cases occupées / trop haut),
        // on descend le doigt d'une demi-case à chaque essai (offset temporaire).
        // La correction persistée n'est mise à jour QU'APRÈS un atterrissage réussi.
        const std::string chemin_apres = "/tmp/ia_bb_apres.png";
        const std::string debug_path   = std::string(std::getenv("HOME")) + "/ia_bb_debug.png";
        const int MAX_ESSAIS = 6;
        bool pose_reussie = false;

        for (int essai = 0; essai < MAX_ESSAIS && !pose_reussie; ++essai) {
            int offset_essai = essai * (hc / 2);
            int eff_dest_x   = dest_x;
            int eff_dest_y   = std::min(dest_y + offset_essai,
                                        zone.y + zone.height - hc / 2);

            // Image de debug annotée (mise à jour à chaque essai)
            {
                cv::Mat dbg = capture.clone();
                cv::rectangle(dbg, zone, {0,255,0}, 3);
                for (int r = 0; r <= TAILLE_GRILLE; ++r) {
                    cv::line(dbg, {zone.x, zone.y+r*hc}, {zone.x+zone.width, zone.y+r*hc}, {0,180,0}, 1);
                    cv::line(dbg, {zone.x+r*lc, zone.y}, {zone.x+r*lc, zone.y+zone.height}, {0,180,0}, 1);
                }
                for (int r = 0; r < TAILLE_GRILLE; ++r)
                    for (int c = 0; c < TAILLE_GRILLE; ++c)
                        if (etat.grille.est_remplie(r, c))
                            cv::rectangle(dbg, {zone.x+c*lc+2, zone.y+r*hc+2, lc-4, hc-4}, {0,0,200}, -1);
                for (const auto& cell : piece.cellules) {
                    int tr = coup.ligne + cell.ligne, tc = coup.colonne + cell.colonne;
                    cv::rectangle(dbg, {zone.x+tc*lc+4, zone.y+tr*hc+4, lc-8, hc-8}, {0,255,0}, 3);
                }
                int ar = coup.ligne + ancre.ligne, ac = coup.colonne + ancre.colonne;
                cv::rectangle(dbg, {zone.x+ac*lc+8, zone.y+ar*hc+8, lc-16, hc-16}, {255,0,0}, 3);
                cv::rectangle(dbg, bbox, {255,165,0}, 3);
                cv::circle(dbg, {grab_x, grab_y}, 15, {0,165,255}, -1);
                cv::circle(dbg, {eff_dest_x, eff_dest_y}, 18, {0,255,255}, -1);
                cv::putText(dbg, std::format("doigt({},{}) e{}", eff_dest_x, eff_dest_y, essai+1),
                    {eff_dest_x+5, eff_dest_y-5}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0,255,255}, 2);
                for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i) {
                    cv::rectangle(dbg, resultat->bbox_pieces[i], {200,100,50}, 2);
                    cv::putText(dbg, std::format("P{}", i),
                        {resultat->bbox_pieces[i].x, resultat->bbox_pieces[i].y-5},
                        cv::FONT_HERSHEY_SIMPLEX, 1.2, {200,100,50}, 2);
                }
                cv::imwrite(debug_path, dbg);
            }

            ajouter_log(std::format("Essai {}/{} P{} {} grab({},{}) L{}C{} dest({},{})+{}",
                essai+1, MAX_ESSAIS, coup.index_piece, piece.nom,
                grab_x, grab_y, coup.ligne, coup.colonne,
                eff_dest_x, eff_dest_y, offset_essai));

            adb_.glisser(grab_x, grab_y, eff_dest_x, eff_dest_y, 700);

            // Attendre l'animation (2s au premier essai, 1.5s pour les suivants)
            std::this_thread::sleep_for(std::chrono::milliseconds(essai == 0 ? 2500 : 1500));

            if (!adb_.capturer_ecran(chemin_apres)) continue;
            cv::Mat apres = cv::imread(chemin_apres);
            if (apres.empty()) continue;
            auto res_apres = analyseur_.analyser(apres);
            if (!res_apres) continue;

            std::string nouvelles;
            int nb = 0;
            int pose_min_r = TAILLE_GRILLE, pose_min_c = TAILLE_GRILLE;
            for (int r = 0; r < TAILLE_GRILLE; ++r)
                for (int c = 0; c < TAILLE_GRILLE; ++c)
                    if (!resultat->grille.est_remplie(r, c) &&
                         res_apres->grille.est_remplie(r, c)) {
                        nouvelles += std::format("({},{})", r, c);
                        pose_min_r = std::min(pose_min_r, r);
                        pose_min_c = std::min(pose_min_c, c);
                        ++nb;
                    }

            if (nb > 0) {
                ajouter_log(std::format("Pose OK essai{} {} nb={}", essai+1, nouvelles, nb));
                pose_reussie = true;

                // Calibration : intégrer l'offset utilisé + l'erreur de position mesurée.
                // Objectif : correction_y doit faire atterrir directement à coup.ligne.
                if (nb >= static_cast<int>(piece.cellules.size())) {
                    int err_r = pose_min_r - coup.ligne;
                    int err_c = pose_min_c - coup.colonne;
                    // L'offset_essai a été nécessaire pour atterrir → l'intégrer au correction
                    correction_y += offset_essai - err_r * hc;
                    correction_x -= err_c * lc;
                    ajouter_log(std::format(
                        "Calibration: essai{} offset={} errL{:+d}C{:+d} -> corrY={} corrX={}",
                        essai+1, offset_essai, err_r, err_c, correction_y, correction_x));
                    std::ofstream f(chemin_corr);
                    f << "correction_x " << correction_x << "\n"
                      << "correction_y " << correction_y << "\n";
                }

                const std::string out = std::string(std::getenv("HOME")) + "/ia_bb_apres.png";
                cv::imwrite(out, apres);
            } else {
                ajouter_log(std::format("Essai {} rejete", essai+1));
            }
        } // for (essai)

        if (pose_reussie) {
            // Coup réussi : lever toutes les blacklists
            pieces_bloquees.fill(false);
        } else {
            ajouter_log(std::format("ECHEC total: blacklist P{}", coup.index_piece));
            pieces_bloquees[coup.index_piece] = true;

            // Si toutes les pièces présentes sont bloquées → blocage total
            bool toutes_bloquees = true;
            for (int i = 0; i < NB_PIECES_SIMULTANEES; ++i)
                if (resultat->pieces_presentes[i] && !pieces_bloquees[i])
                    toutes_bloquees = false;

            if (toutes_bloquees) {
                ajouter_log("Toutes les pieces bloquees, attente 10s...");
                pieces_bloquees.fill(false);
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }
    } // while (actif_)

    actif_ = false;
}

void Interface::on_clic(int x, int y) {
    if (dans(btn_sim_, x, y)) {
        if (mode_ != Mode::SIMULATION) {
            arreter_reel();
            mode_ = Mode::SIMULATION;
            ajouter_log("Mode: Simulation");
        }
    } else if (dans(btn_reel_, x, y)) {
        if (mode_ != Mode::REEL) {
            arreter_simulation();
            mode_ = Mode::REEL;
            ajouter_log("Mode: Reel (ADB)");
        }
    } else if (dans(btn_lancer_, x, y)) {
        if (mode_ == Mode::SIMULATION) demarrer_simulation();
        else                           demarrer_reel();
    } else if (dans(btn_arreter_, x, y)) {
        if (mode_ == Mode::SIMULATION) arreter_simulation();
        else                           arreter_reel();
    } else if (dans(btn_sauv_, x, y)) {
        if (mode_ == Mode::REEL) {
            calibrer();
        } else {
            entraineur_.sauvegarder("meilleur_genome.txt");
            ajouter_log("Genome sauvegarde.");
        }
    } else if (dans(btn_capture_, x, y)) {
        capturer_et_afficher();
    } else if (dans(btn_conn_, x, y)) {
        if (adb_.connecter()) {
            ajouter_log("Connecte: " + adb_.appareil_actuel());
            if (analyseur_.charger_config(AnalyseurEcran::chemin_config()))
                ajouter_log("Config: OK");
            else
                ajouter_log("Config absente → cliquez Calibrer");
            StatistiquesGeneration stat = entraineur_.derniere_stat();
            if (stat.meilleur_score_absolu > 0.0f)
                agent_courant_ = Agent(stat.meilleur_genome_absolu);
        } else {
            ajouter_log("Aucun appareil ADB trouve.");
        }
    }
}

void Interface::cb_souris(int event, int x, int y, int /*flags*/, void* data) {
    if (event != cv::EVENT_LBUTTONDOWN) return;
    static_cast<Interface*>(data)->on_clic(x, y);
}

void Interface::lancer() {
    const std::string nom_fenetre = "IA Block Blast";
    cv::namedWindow(nom_fenetre, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(nom_fenetre, cb_souris, this);

    ajouter_log("Pret. Cliquez sur Lancer.");

    while (cv::getWindowProperty(nom_fenetre, cv::WND_PROP_VISIBLE) >= 1) {
        dessiner();
        cv::imshow(nom_fenetre, canvas_);
        int touche = cv::waitKey(33);
        if (touche == 27 || touche == 'q') break;
        if (touche == 's') {
            entraineur_.sauvegarder("meilleur_genome.txt");
            ajouter_log("Genome sauvegarde (touche s).");
        }
    }

    cv::destroyAllWindows();
}
