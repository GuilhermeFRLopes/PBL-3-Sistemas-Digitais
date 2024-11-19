#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "graphic_functions.h"
#include "logic_game.h"
#include "Biblioteca_GPU.h"

void setQuadrado(int coluna, int linha, int R, int G, int B)
{
    // Ajuste para desenhar um quadrado 4x4
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            // Aumenta as coordenadas para o bloco
            int block_col = (coluna * 2) + j;
            int block_line = (linha * 2) + i;
            // usleep(1500);
            //  Chama a função para definir o bloco de fundo
            set_background_block(block_col, block_line, R, G, B);
        }
    }
}

void limpa()
{
    for (int i = 0; i < 41; i++)
    {
        for (int j = 0; j < 21; j++)
        {
            //while (1)
            //{
                int block_col = j;
                int block_line = i;
                // usleep(10);
                //  Chama a função para definir o bloco de fundo
                //setQuadrado(block_col, block_line, 0, 0, 0);
                set_background_block(block_col, block_line, 0, 0, 0);
                if (j == 20 && i <= 40)
                {
                    
                    set_background_block(block_col, block_line, 0b010, 0b011, 0b110); // encontro da coluna e linha
                }
                if (i == 40 && j <= 20)
                {
                    
                    set_background_block(block_col, block_line, 0b010, 0b011, 0b110); // encontro da coluna e linha
                }
                //break;
            //}
        }
    }
}

void limpaDevagar()
{
    for (int i = 0; i < 60; i++)
    {
        for (int j = 0; j < 80; j++)
        {
            while (1)
            {
                int block_col = j;
                int block_line = i;
                usleep(1000);
                // Chama a função para definir o bloco de fundo
                set_background_block(block_col, block_line, 0, 0, 0);
                break;
            }
        }
    }
}

void limpaTudo()
{
    for (int i = 0; i < 60; i++)
    {
        for (int j = 0; j < 80; j++)
        {
            //while (1)
            //{
                int block_col = j;
                int block_line = i;
                // usleep(1000);
                //  Chama a função para definir o bloco de fundo
                set_background_block(block_col, block_line, 0, 0, 0);
              //  break;
            //}
        }
    }
}

void desenhaPeca(Peca peca)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (peca.quadrados[i][j].ativo)
            {
                int x = peca.pos_x / BLOCO_TAM + j;
                int y = peca.pos_y / BLOCO_TAM + i;
                int R, G, B;
                converterCorParaRGB(peca.quadrados[i][j].cor, &R, &G, &B);
                setQuadrado(x, y, R, G, B); // x = coluna; y = linha
            }
        }
    }
}

void desenha(int matriz[12][25], int x, int y)
{
    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 25; j++)
        {
            // Aumenta as coordenadas para o bloco
            if (matriz[i][j] == 1)
            {
                int block_col = j;
                int block_line = i;
                int R, G, B;
                int cor = corAleatoria();
                converterCorParaRGB(cor, &R, &G, &B);
                setQuadrado(block_col + x, block_line + y, R, G, B);
                // set_background_block(block_col, block_line, R, G, B);
            }
            // usleep(1500);
        }
    }
}

void desenhaS(int matriz[12][25], int x, int y)
{
    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 25; j++)
        {
            // Aumenta as coordenadas para o bloco
            if (matriz[i][j] == 1)
            {
                int block_col = j;
                int block_line = i;
                int R, G, B;
                int cor = corAleatoria();
                converterCorParaRGB(cor, &R, &G, &B);
                // setQuadrado(block_col + x, block_line + y, R, G, B);
                set_background_block(block_col + x, block_line + y, R, G, B);
            }
            // usleep(1500);
        }
    }
}

void desenhaNumero(int matriz[9][8], int x, int y)
{
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            // Aumenta as coordenadas para o bloco
            if (matriz[i][j] == 1)
            {
                int block_col = j;
                int block_line = i;
                int R, G, B;
                int cor = corAleatoria();
                converterCorParaRGB(cor, &R, &G, &B);
                // setQuadrado(block_col + x, block_line + y, R, G, B);
                set_background_block(block_col + x, block_line + y, R, G, B);
            }
            // usleep(1500);
        }
    }
}

void mostrarAllPecas(Tabuleiro *tab)
{
    for (int i = 0; i < LINHA_TABULEIRO; i++)
    {
        for (int j = 0; j < COLUNA_TABULEIRO; j++)
        {
            // faz com que mostre na tela as posições que já estão ativas no tabuleiro
            if (tab->ocupado[i][j] == true)
            {
                int x = j;
                int y = i;

                int R, G, B;
                converterCorParaRGB(tab->cor[i][j], &R, &G, &B);
                setQuadrado(x, y, R, G, B);
            }
        }
    }
}

void addPecaNoTabuleiro(Tabuleiro *tabuleiro, Peca peca)
{
    for (int i = 0; i < LINHA_TABULEIRO; i++)
    {
        for (int j = 0; j < COLUNA_TABULEIRO; j++)
        {
            // faz com que marque como true no tabuleiro
            if (i < 4 && j < 4 && peca.quadrados[i][j].ativo)
            {
                int x = (peca.pos_y / BLOCO_TAM) + i; // x = linha; y = coluna
                int y = (peca.pos_x / BLOCO_TAM) + j;

                tabuleiro->ocupado[x][y] = true;
                tabuleiro->cor[x][y] = peca.quadrados[i][j].cor;

                // mostra a peça na tela
                // x = peca.pos_x + (j * BLOCO_TAM);
                // y = peca.pos_y + (i * BLOCO_TAM);

                int R, G, B;
                converterCorParaRGB(peca.quadrados[i][j].cor, &R, &G, &B);
                setQuadrado(y, x, R, G, B); // x = linha; y = coluna
            }
        }
    }
}