#ifndef LOGIC_GAME
#define LOGIC_GAME


#define BLOCO_TAM 10
#define LARGURA_TELA 300
#define ALTURA_TELA 200
#define hex5 0x10
#define hex4 0x20
#define hex3 0x30
// Tamanho para a matriz
#define COLUNA_TABULEIRO 10
#define LINHA_TABULEIRO 20

typedef struct logic_game
{
    /* data */
};

typedef struct
{
    bool ocupado[LINHA_TABULEIRO][COLUNA_TABULEIRO]; // Matriz para armazenar o estado das posições
    int cor[LINHA_TABULEIRO][COLUNA_TABULEIRO];
} Tabuleiro;

typedef struct
{
    int pos_x, pos_y; // Posição relativa do quadrado dentro da peça
    int cor;          // Cor do quadrado
    bool ativo;       // Indica se o quadrado faz parte da peça (ativo ou não)
} Quadrado;

typedef struct
{
    int tam_x, tam_y; // Dimensões da peça (número de quadrados)
    int pos_x, pos_y; // Posição da peça no tabuleiro
    int prev_pos_x, prev_pos_y;
    Quadrado quadrados[4][4]; // Matriz de quadrados, 4x4 é suficiente para qualquer peça de Tetris
} Peca;
    int display[10] = {
        0b1000000, 
        0b1111001, 
        0b0100100, 
        0b0110000, 
        0b0011001, 
        0b0010010, 
        0b0000010, 
        0b1111000, 
        0b0000000, 
        0b0010000};



int corAleatoria();
void converterCorParaRGB(int cor, int *R, int *G, int *B);
void moverPeca(Peca *peca, int dy);
void iniTabuleiro(Tabuleiro *tabuleiro);
void moverDirOuEsq(Tabuleiro *tab, Peca *peca, int dx);
bool verificarColisao(Tabuleiro *tabuleiro, Peca peca);
void verificaLinhaCompleta(Tabuleiro *tab);
void atualizaDisplay(int score);
bool reiniciarGame(Tabuleiro *tabuleiro, Peca peca);
Peca criarPecasAleatorias();

#endif