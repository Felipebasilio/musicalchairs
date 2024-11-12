#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
int numero_cadeira = 1; // Contador global para as cadeiras ocupadas

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : cadeiras(num_jogadores - 1) {}

    void iniciar_rodada(int jogadores_ativos) {
        cadeiras--;
        numero_cadeira = 1; // Reinicia o contador para cada rodada
        while (cadeira_sem.try_acquire())
            ;
        cadeira_sem.release(cadeiras);
        musica_parada.store(false);

        if (jogadores_ativos > 1) {
            std::cout << "\nPróxima rodada com " << jogadores_ativos << " jogadores e " << cadeiras << " cadeiras.\n";
            std::cout << "A música está tocando... 🎵.\n\n";
        }
    }

    void parar_musica() {
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada.store(true);
        music_cv.notify_all();
        std::cout << "> A música parou! Os jogadores estão tentando se sentar...\n\n";
        std::cout << "-----------------------------------------------\n";
    }

    void exibir_estado() {
        std::cout << "Rodada atual com " << cadeiras << " cadeiras disponíveis.\n";
    }

    bool jogo_em_progresso(int jogadores_ativos) const {
        return jogadores_ativos > 1;
    }

private:
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id)
        : id(id), ativo(true), tentou_rodada(false) {}

    bool esta_ativo() const {
        return ativo;
    }

    int get_id() const {
        return id;
    }

    void tentar_ocupar_cadeira() {
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, []
                      { return musica_parada.load(); });

        if (ativo && !tentou_rodada) {
            tentou_rodada = true; // Marca que o jogador já tentou nesta rodada
            if (cadeira_sem.try_acquire())
            {
                std::cout << "[Cadeira " << numero_cadeira++ << "]: Ocupada por P" << id << "\n";
            }
            else
            {
                ativo = false; // Marca o jogador como eliminado
                std::cout << "\nJogador P" << id << " não conseguiu uma cadeira e foi eliminado!\n";
            }
        }
    }

    void reseta_rodada() {
        tentou_rodada = false;
    }

    void joga() {
        while (ativo) {
            tentar_ocupar_cadeira();
            if (!ativo)
            {
                std::this_thread::yield();
            }
        }
    }

private:
    int id;
    bool ativo;
    bool tentou_rodada;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras &jogo, std::vector<Jogador> &jogadores)
        : jogo(jogo), jogadores(jogadores) {}

    void iniciar_jogo() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1000, 3000);

        while (jogo.jogo_em_progresso(jogadores_ativos())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
            jogo.parar_musica();

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            liberar_threads_eliminadas();
            jogo.iniciar_rodada(jogadores_ativos());
            reseta_rodada_jogadores();
        }

        std::cout << "\n🏆 Vencedor: Jogador P" << encontrar_vencedor() << "! Parabéns! 🏆\n";
    }

private:
    JogoDasCadeiras &jogo;
    std::vector<Jogador> &jogadores;

    int jogadores_ativos() const {
        int ativos = 0;
        for (const auto &jogador : jogadores) {
            if (jogador.esta_ativo())
                ativos++;
        }
        return ativos;
    }

    int encontrar_vencedor() const {
        for (const auto &jogador : jogadores) {
            if (jogador.esta_ativo())
            {
                return jogador.get_id();
            }
        }
        return -1;
    }

    void liberar_threads_eliminadas() {
        cadeira_sem.release(NUM_JOGADORES - 1);
    }

    void reseta_rodada_jogadores() {
        for (auto &jogador : jogadores) {
            jogador.reseta_rodada();
        }
    }
};

int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    std::vector<Jogador> jogadores;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores.emplace_back(i);
    }

    Coordenador coordenador(jogo, jogadores);
    std::vector<std::thread> threads_jogadores;

    for (auto &jogador : jogadores) {
        threads_jogadores.emplace_back(&Jogador::joga, &jogador);
    }

    std::thread thread_coordenador(&Coordenador::iniciar_jogo, &coordenador);

    for (auto &t : threads_jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (thread_coordenador.joinable()) {
        thread_coordenador.join();
    }

    std::cout << "Jogo das Cadeiras finalizado.\n";
    return 0;
}
