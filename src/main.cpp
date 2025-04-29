#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <ctime>

// Constantes globais
constexpr int TOTAL_JOGADORES = 4;
std::counting_semaphore<TOTAL_JOGADORES> semaforo_cadeiras(TOTAL_JOGADORES - 1); // Inicialmente há n-1 cadeiras
std::condition_variable cond_musica;
std::mutex mtx_musica;
std::atomic<bool> musica_pausada{false};
std::atomic<bool> partida_ativa{true};
int proxima_cadeira = 1;
std::mt19937 rng(std::time(nullptr));

// Classe principal do jogo
class Jogo {
public:
    Jogo(int jogadores)
        : cadeiras_disponiveis(jogadores - 1) {}

    void nova_rodada(int jogadores_restantes) {
        cadeiras_disponiveis--;
        proxima_cadeira = 1;

        while (semaforo_cadeiras.try_acquire());
        semaforo_cadeiras.release(cadeiras_disponiveis);

        musica_pausada.store(false);

        if (jogadores_restantes > 1) {
            std::cout << "\nRodada com " << jogadores_restantes << " jogadores e " << cadeiras_disponiveis << " cadeiras.\n";
            std::cout << "Música tocando... 🎶\n\n";
        }
    }

    void pausar_musica() {
        std::unique_lock<std::mutex> lock(mtx_musica);
        musica_pausada.store(true);
        cond_musica.notify_all();
        std::cout << "> Música pausada! Jogadores tentando se sentar...\n\n";
        std::cout << "----------------------------------------------------------\n";
    }

    bool ainda_rodando(int jogadores_vivos) const {
        return jogadores_vivos > 1;
    }

private:
    int cadeiras_disponiveis;
};

class Participante {
public:
    Participante(int identificador)
        : id(identificador), em_jogo(true), ja_tentou(false) {}

    bool ativo() const {
        return em_jogo;
    }

    int obter_id() const {
        return id;
    }

    void reiniciar_tentativa() {
        ja_tentou = false;
    }

    void executar() {
        while (em_jogo && partida_ativa.load()) {
            std::unique_lock<std::mutex> lock(mtx_musica);
            cond_musica.wait(lock, [] { return musica_pausada.load() || !partida_ativa.load(); });

            if (!partida_ativa.load()) break;

            if (em_jogo && !ja_tentou) {
                ja_tentou = true;
                if (semaforo_cadeiras.try_acquire()) {
                    std::cout << "[Cadeira " << proxima_cadeira++ << "] Ocupada por Jogador P" << id << "\n";
                } else {
                    em_jogo = false;
                    std::cout << "\nJogador P" << id << " ficou de pé e foi eliminado!\n";
                    std::cout << "----------------------------------------------------------\n";
                }
            }
        }
    }

private:
    int id;
    bool em_jogo;
    bool ja_tentou;
};

class Arbitro {
public:
    Arbitro(Jogo& jogo_ref, std::vector<Participante>& lista_jogadores)
        : jogo(jogo_ref), jogadores(lista_jogadores) {}

    void rodar_partida() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay(1000, 3000);

        while (jogo.ainda_rodando(jogadores_ativos())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
            jogo.pausar_musica();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            liberar_threads_presas();
            jogo.nova_rodada(jogadores_ativos());
            resetar_participantes();
        }

        std::cout << "\n🏆 Campeão: Jogador P" << obter_vencedor() << "! 🎉\n\n";
        std::cout << "----------------------------------------------------------\n";

        partida_ativa.store(false);
        cond_musica.notify_all();
    }

    void liberar_threads_presas() {
        semaforo_cadeiras.release(TOTAL_JOGADORES - 1);
    }

    int jogadores_ativos() const {
        int cont = 0;
        for (const auto& p : jogadores) {
            if (p.ativo()) ++cont;
        }
        return cont;
    }

    int obter_vencedor() const {
        for (const auto& p : jogadores) {
            if (p.ativo()) return p.obter_id();
        }
        return -1;
    }

    void resetar_participantes() {
        for (auto& p : jogadores) {
            p.reiniciar_tentativa();
        }
    }

private:
    Jogo& jogo;
    std::vector<Participante>& jogadores;
};

int main() {
    std::cout << "----------------------------------------------------------\n";
    std::cout << "🎲 Iniciando o Desafio das Cadeiras! 🎲\n";
    std::cout << "----------------------------------------------------------\n";

    std::cout << "\nPrimeira rodada com " << TOTAL_JOGADORES << " jogadores e " << TOTAL_JOGADORES - 1 << " cadeiras.\n";
    std::cout << "Música tocando... 🎶\n\n";

    Jogo jogo(TOTAL_JOGADORES);
    std::vector<Participante> jogadores;

    for (int i = 1; i <= TOTAL_JOGADORES; ++i) {
        jogadores.emplace_back(i);
    }

    Arbitro arbitro(jogo, jogadores);
    std::vector<std::thread> threads_participantes;

    for (auto& jogador : jogadores) {
        threads_participantes.emplace_back(&Participante::executar, &jogador);
    }

    std::thread thread_arbitro(&Arbitro::rodar_partida, &arbitro);

    for (auto& t : threads_participantes) {
        if (t.joinable()) t.join();
    }

    if (thread_arbitro.joinable()) thread_arbitro.join();

    std::cout << "Fim do jogo. Obrigado por participar!\n\n";
    return 0;
}