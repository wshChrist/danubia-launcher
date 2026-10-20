#pragma once
// Plugin BakkesMod « Danubia » : exécute les ordres du launcher pour la ranked Rocket League (jeu lancé SANS EAC,
// hors ligne / LAN uniquement : c'est le mode prévu par Psyonix pour BakkesMod).
//
// Échange par fichiers dans <données BakkesMod>/danubia/ (pas de port, pas de console) :
//   ordre.txt     écrit par le launcher : n=<numéro>, action=heberger|rejoindre|quitter, carte, format, port, motdepasse,
//                 attendu=<id en jeu>⇥<équipe 0/1>⇥<nom> (tabulations : un identifiant « Epic|…|0 » contient des |) (une ligne par joueur, hôte compris)
//   etat.txt      écrit ici 4 fois par seconde : identité du joueur local, ordre traité, état de la partie, joueurs présents
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <map>
#include <string>
#include <vector>

struct Attendu {
    std::string id;
    int equipe = 0;
    std::string nom;
};

class DanubiaRL : public BakkesMod::Plugin::BakkesModPlugin {
public:
    void onLoad() override;
    void onUnload() override;

private:
    void boucle();
    void lireOrdre();
    void executer(const std::map<std::string, std::string>& o, const std::vector<Attendu>& attendus);
    void surveillerHote();
    void ecrireEtat();

    std::filesystem::path dossier;
    long long dernierOrdre = -1;
    std::string action;          // dernier ordre exécuté
    std::vector<Attendu> attendus; // hôte : joueurs à placer
    bool heberge = false;
    bool complet = false;        // hôte : tout le monde est là, équipes placées, coup d'envoi donné
    std::string erreur;
    bool actif = true;
};
