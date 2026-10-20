#include "DanubiaRL.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

BAKKESMOD_PLUGIN(DanubiaRL, "Danubia", "0.1.0", PLUGINTYPE_FREEPLAY)

namespace {
constexpr float PERIODE = 0.25f;
// Partie LAN de Psyonix telle que l'ouvre Rocket Plugin (MIT) : soccar, sans bots, hébergée et en écoute.
constexpr const char* MODE = "TAGame.GameInfo_Soccar_TA";

std::string idDe(PriWrapper pri)
{
    return pri.GetUniqueIdWrapper().GetIdString();
}
}

void DanubiaRL::onLoad()
{
    dossier = gameWrapper->GetDataFolder() / "danubia";
    std::error_code ec;
    std::filesystem::create_directories(dossier, ec);
    cvarManager->log("Danubia : prêt (" + dossier.string() + ")");
    boucle();
}

void DanubiaRL::onUnload()
{
    actif = false;
}

void DanubiaRL::boucle()
{
    if (!actif) return;
    try {
        lireOrdre();
        surveillerHote();
        ecrireEtat();
    } catch (const std::exception& e) {
        erreur = e.what();
    }
    gameWrapper->SetTimeout([this](GameWrapper*) { boucle(); }, PERIODE);
}

void DanubiaRL::lireOrdre()
{
    std::ifstream f(dossier / "ordre.txt");
    if (!f) return;
    std::map<std::string, std::string> o;
    std::vector<Attendu> l;
    std::string ligne;
    while (std::getline(f, ligne)) {
        if (!ligne.empty() && ligne.back() == '\r') ligne.pop_back();
        const auto eq = ligne.find('=');
        if (eq == std::string::npos) continue;
        const std::string cle = ligne.substr(0, eq), val = ligne.substr(eq + 1);
        if (cle == "attendu") {
            std::stringstream s(val);
            Attendu a;
            std::string equipe;
            std::getline(s, a.id, '\t');
            std::getline(s, equipe, '\t');
            std::getline(s, a.nom);
            a.equipe = equipe == "1" ? 1 : 0;
            l.push_back(a);
        } else {
            o[cle] = val;
        }
    }
    const auto n = o.find("n");
    if (n == o.end()) return;
    const long long numero = std::atoll(n->second.c_str());
    // la liste des attendus de l'hôte peut grandir sans nouvel ordre (identités reçues au fil de l'eau)
    if (numero == dernierOrdre) {
        if (action == "heberger") attendus = l;
        return;
    }
    dernierOrdre = numero;
    executer(o, l);
}

void DanubiaRL::executer(const std::map<std::string, std::string>& o, const std::vector<Attendu>& l)
{
    auto val = [&](const char* k, const char* d = "") {
        const auto it = o.find(k);
        return it == o.end() ? std::string(d) : it->second;
    };
    action = val("action");
    erreur.clear();
    if (action == "heberger") {
        attendus = l;
        heberge = true;
        complet = false;
        const int format = std::clamp(std::atoi(val("format", "1").c_str()), 1, 4);
        const std::string tags = "BotsNone,PlayerCount" + std::to_string(2 * format);
        const std::string cmd = "open " + val("carte", "Stadium_P") + "?Playtest?game=" + MODE + "?GameTags=" + tags +
            "?NumPublicConnections=10?NumOpenPublicConnections=10?Lan?Listen";
        cvarManager->log("Danubia : " + cmd);
        gameWrapper->ExecuteUnrealCommand(cmd);
    } else if (action == "rejoindre") {
        heberge = false;
        const std::string cmd = "start 127.0.0.1:" + val("port", "7779") + "/?Lan?Password=" + val("motdepasse");
        cvarManager->log("Danubia : rejoindre 127.0.0.1:" + val("port", "7779"));
        gameWrapper->ExecuteUnrealCommand(cmd);
    } else if (action == "quitter") {
        heberge = complet = false;
        attendus.clear();
        gameWrapper->ExecuteUnrealCommand("disconnect");
    }
}

// Hôte : place chaque joueur attendu dans son équipe ; quand tout le monde est là, remet la partie à zéro et la lance.
void DanubiaRL::surveillerHote()
{
    if (!heberge || attendus.empty()) return;
    ServerWrapper partie = gameWrapper->GetGameEventAsServer();
    if (partie.IsNull()) return;
    auto pris = partie.GetPRIs();
    int places = 0;
    for (int i = 0; i < pris.Count(); i++) {
        PriWrapper pri = pris.Get(i);
        if (pri.IsNull() || pri.GetbBot()) continue;
        const std::string id = idDe(pri);
        for (const auto& a : attendus) {
            if (a.id != id) continue;
            if (pri.GetTeamNum() != a.equipe) pri.ServerChangeTeam(a.equipe);
            else places++;
        }
    }
    if (!complet && places == static_cast<int>(attendus.size())) {
        complet = true;
        // coup d'envoi propre : le jeu a pu commencer pendant que les invités arrivaient
        auto equipes = partie.GetTeams();
        for (int i = 0; i < equipes.Count(); i++)
            if (!equipes.Get(i).IsNull()) equipes.Get(i).ResetScore();
        for (int i = 0; i < pris.Count(); i++)
            if (!pris.Get(i).IsNull()) pris.Get(i).ResetScore();
        partie.SetSecondsRemaining(300);
        partie.ResetBalls();
        partie.ResetPlayers();
        partie.StartCountdownTimer();
        cvarManager->log("Danubia : tout le monde est là, coup d'envoi.");
    }
}

void DanubiaRL::ecrireEtat()
{
    std::ostringstream s;
    const auto maintenant = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    s << "t=" << maintenant << "\n";
    s << "version=0.1.0\n";
    s << "id=" << gameWrapper->GetUniqueID().GetIdString() << "\n";
    s << "nom=" << gameWrapper->GetPlayerName().ToString() << "\n";
    s << "ordre=" << dernierOrdre << "\n";
    s << "action=" << action << "\n";
    s << "enjeu=" << (gameWrapper->IsInGame() ? 1 : 0) << "\n";
    s << "heberge=" << (heberge ? 1 : 0) << "\n";
    s << "complet=" << (complet ? 1 : 0) << "\n";
    ServerWrapper partie = gameWrapper->GetCurrentGameState();
    if (!partie.IsNull()) {
        s << "partie=1\n";
        s << "fin=" << (partie.GetbMatchEnded() ? 1 : 0) << "\n";
        auto pris = partie.GetPRIs();
        for (int i = 0; i < pris.Count(); i++) {
            PriWrapper pri = pris.Get(i);
            if (pri.IsNull() || pri.GetbBot()) continue;
            s << "joueur=" << idDe(pri) << "\t" << static_cast<int>(pri.GetTeamNum()) << "\t" << pri.GetPlayerName().ToString() << "\n";
        }
    } else {
        s << "partie=0\n";
    }
    if (!erreur.empty()) s << "erreur=" << erreur << "\n";
    // écriture atomique : le launcher ne lit jamais un fichier à moitié écrit
    const auto tmp = dossier / "etat.txt.tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        f << s.str();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, dossier / "etat.txt", ec);
}
