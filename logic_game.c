#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "logic_game.h"
#include "Biblioteca_GPU.h"
#include "map.h"

int scoreTotal = 0;
int corAleatoria()
{
    srand(time(NULL));             // Semente para o gerador de números aleatórios
    int numAleatorio = rand() % 7; // Gerar um número aleatório entre 0 e 9

    // return cores[numAleatorio];
    return numAleatorio;
}

void converterCorParaRGB(int cor, int *R, int *G, int *B)
{
    switch (cor)
    {
    case (1):
        *R = 0b111;
        *G = 0b111;
        *B = 0; // Amarelo
        break;
    case (2):
        *R = 0b111;
        *G = 0;
        *B = 0; // Vermelho
        break;
    case (3):
        *R = 0;
        *G = 0b111;
        *B = 0; // Verde
        break;
    case (4):
        *R = 0;
        *G = 0;
        *B = 0b111; // Azul
        break;
    case (5):
        *R = 0;
        *G = 0b111;
        *B = 0b111; // Ciano
        break;
    case (6):
        *R = 0b111;
        *G = 0;
        *B = 0b111; // Cinza
        break;
    case (7):
        *R = 0b111;
        *G = 0;
        *B = 0; // Laranja
        break;
    default:
        *R = 0b111;
        *G = 0b111;
        *B = 0b111; // Cor padrão: Branco
        break;
    }
}

void moverPeca(Peca *peca, int dy)
{
    // printf("\npos: %d\n", peca->pos_y);
    peca->pos_y += dy;
}

void iniTabuleiro(Tabuleiro *tabuleiro)
{
    for (int i = 0; i < LINHA_TABULEIRO; i++)
    {
        for (int j = 0; j < COLUNA_TABULEIRO; j++)
        {
            tabuleiro->ocupado[i][j] = false;
        }
    }
}

void moverDirOuEsq(Tabuleiro *tab, Peca *peca, int dx)
{
    bool podeMover = true;

    for (int i = 0; i < 4; i++) // não precisa percorrer o tabuleiro todo, pois está passando a posição da peça na matriz
    {
        for (int j = 0; j < 4; j++)
        {
            if (peca->quadrados[i][j].ativo)
            {
                int x = (peca->pos_y / BLOCO_TAM) + i;
                int y = (peca->pos_x / BLOCO_TAM) + j;

                // para saber se a posição da peça será para a esquerda ou direita
                if (dx > 0)
                {
                    // verifica se tem uma peça a direita
                    if (y + 1 >= COLUNA_TABULEIRO || tab->ocupado[x][y + 1] == true)
                    {
                        podeMover = false;
                    }
                }
                else
                {
                    // verifica se tem uma peça a esquerda
                    if (y - 1 < 0 || tab->ocupado[x][y - 1] == true)
                    {
                        podeMover = false;
                    }
                }
            }
        }
    }

    if (podeMover)
    {
        peca->pos_x += dx;
    }
}

bool verificarColisao(Tabuleiro *tabuleiro, Peca peca)
{
    for (int i = 0; i < 4; i++) // não precisa percorrer o tabuleiro todo, pois está passando a posição da peça na matriz
    {
        for (int j = 0; j < 4; j++)
        {
            if (peca.quadrados[i][j].ativo)
            {
                int x = (peca.pos_y / BLOCO_TAM) + i;
                int y = (peca.pos_x / BLOCO_TAM) + j;

                // verifica se a peça está dentro do tabuleiro
                if (x < 0 || x + 1 >= LINHA_TABULEIRO || y < 0 || y >= COLUNA_TABULEIRO)
                {
                    return true;
                }

                // verifica se tem uma peça abaixo
                if (x + 1 < LINHA_TABULEIRO && tabuleiro->ocupado[x + 1][y] == true)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void verificaLinhaCompleta(Tabuleiro *tab)
{
    // começa de baixo para cima
    for (int i = LINHA_TABULEIRO - 1; i >= 0; i--)
    {
        bool linhaCompleta = true;

        for (int j = 0; j < COLUNA_TABULEIRO; j++)
        {
            if (!tab->ocupado[i][j])
            {
                linhaCompleta = false;
                break;
            }
        }

        if (linhaCompleta)
        {
            scoreTotal += 1;
            // velocidade -= 3500;
            atualizaDisplay(scoreTotal);

            // desocupa a linha completa
            for (int j = 0; j < COLUNA_TABULEIRO; j++)
            {
                tab->ocupado[i][j] = false;
            }

            // mover tatualizaDisplay(score);odas as linhas acima para baixo
            for (int p = i; p > 0; p--)
            {
                for (int k = 0; k < COLUNA_TABULEIRO; k++)
                {
                    tab->ocupado[p][k] = tab->ocupado[p - 1][k];
                    tab->cor[p][k] = tab->cor[p - 1][k];
                }
            }

            for (int k = 0; k < COLUNA_TABULEIRO; k++)
            {
                tab->ocupado[0][k] = false;
            }

            i++;
        }
    }
}


void atualizaDisplay(int score)
{
    int fd = -1;
    void * LW_virtual;
    if (fd == -1)
        if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1) {
            printf ("ERROR: could not open \"/dev/mem\"...\n");
            return (-1);
        }
     // Get a mapping from physical addresses to virtual addresses
    LW_virtual = mmap (NULL, LW_BRIDGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (LW_virtual == MAP_FAILED) {
        printf ("ERROR: mmap() failed...\n");
        close (fd);
        return (NULL);
    }

    int centenas = score / 100;
    int dezenas = (score / 100) & 10;
    int unidades = score % 10;
    int numeros[10] = {
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

    volatile int *DISPLAY_ptr5 = (unsigned int *) (LW_virtual + 0x10); // SEGMENTO A, B, C, D
    volatile int *DISPLAY_ptr4 = (unsigned int *) (LW_virtual + 0x20); // SEGMENTO E F G
    volatile int *DISPLAY_ptr3 = (unsigned int *) (LW_virtual + 0x30); // SEGMENTO E F G
    volatile int *DISPLAY_ptr2 = (unsigned int *) (LW_virtual + 0x40); // SEGMENTO E F G
    volatile int *DISPLAY_ptr1 = (unsigned int *) (LW_virtual + 0x50); // SEGMENTO E F G
    volatile int *DISPLAY_ptr0 = (unsigned int *) (LW_virtual + 0x60); // SEGMENTO E F G

    *DISPLAY_ptr5 = numeros[centenas];    // Exibe o dígito das centenas
    *DISPLAY_ptr4 = numeros[dezenas];     // Exibe o dígito das dezenas
    *DISPLAY_ptr3 = numeros[unidades];    // Exibe o dígito das unidades
    *DISPLAY_ptr2 = 0b1111111;    // Exibe o dígito das unidades
    *DISPLAY_ptr1 = 0b1111111;    // Exibe o dígito das unidades
    *DISPLAY_ptr0 = 0b1111111;    // Exibe o dígito das unidades

}


bool reiniciarGame(Tabuleiro *tabuleiro, Peca peca)
{
    for (int i = 0; i < 4; i++) // não precisa percorrer o tabuleiro todo, pois está passando a posição da peça na matriz
    {
        for (int j = 0; j < 4; j++)
        {
            if (peca.quadrados[i][j].ativo)
            {
                int x = (peca.pos_y / BLOCO_TAM) + i;
                int y = (peca.pos_x / BLOCO_TAM) + j;

                // verifica se tem uma peça abaixo logo ao exibir a nova peça
                if (x + 1 < LINHA_TABULEIRO && tabuleiro->ocupado[x + 1][y] == true)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

Peca criarPecasAleatorias()
{
    // Define as cores disponíveis para as peças
    int cor = corAleatoria();

    // Gera um número aleatório para escolher a peça
    int tipoPeca = rand() % 9;

    Peca peca;
    peca.pos_x = (60) - (BLOCO_TAM * 1); // Centraliza a peça
    peca.pos_y = 0;                      // Começa no topo da tela
    peca.tam_x = 4;                      // Dimensão máxima 4x4 para qualquer peça
    peca.tam_y = 4;

    // Inicializa a matriz de quadrados como inativa
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            peca.quadrados[i][j].ativo = false;
        }
    }

    // tipoPeca = 9;

    // Define a forma da peça com base no número aleatório
     switch (tipoPeca)
    {
    case 0: // Peça "I"
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[3][1].ativo = true;
        break;
    case 1: // Peça "O"
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[1][2].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[2][2].ativo = true;
        break;
    case 2: // Peça "T"
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[1][0].ativo = true;
        break;
    case 3: // Peça "L"
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[2][2].ativo = true;
        break;
    case 4: // Peça "J"
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[2][0].ativo = true;
        break;
    case 5: // Peça "Z"
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[1][2].ativo = true;
        peca.quadrados[2][2].ativo = true;
        break;
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[2][1].ativo = true;
        peca.quadrados[3][1].ativo = true;
        break;
    case 6: // Peça "S"
        peca.quadrados[0][2].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[1][2].ativo = true;
        peca.quadrados[2][1].ativo = true;
        break;
    case 7: // Peça "---"
        peca.quadrados[0][0].ativo = true;
        peca.quadrados[0][1].ativo = true;
        peca.quadrados[0][2].ativo = true;
        peca.quadrados[0][3].ativo = true;
        break;
    case 8: // Peça "-|-"
        peca.quadrados[1][0].ativo = true;
        peca.quadrados[1][1].ativo = true;
        peca.quadrados[1][2].ativo = true;
        peca.quadrados[0][1].ativo = true;
        break;
    }
    // Define a cor para os quadrados ativos
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (peca.quadrados[i][j].ativo)
            {
                peca.quadrados[i][j].cor = cor;
            }
        }
    }

    return peca;
}

Peca copiaPeca(Peca peca){
    Peca anterior;
    anterior.pos_x = peca.pos_x;
    anterior.pos_y = peca.pos_y;
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 4; j++){
            anterior.quadrados[i][j] = peca.quadrados[i][j];
        }
    }
    anterior.tam_x = peca.tam_x;
    anterior.tam_y = peca.tam_y;
    return anterior;
}