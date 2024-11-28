#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include "Biblioteca_GPU.h"
#include <linux/input.h>
#include "map.h"
#include "matrizes.h"

#define I2C0_BASE_ADDR 0xFFC04000                   // Endereço base do I2C0
#define IC_CON_OFFSET 0x0                           // Deslocamento do registrador ic_con
#define MAP_SIZE 0x1000                             // Tamanho do mapeamento de memória
#define IC_TAR_REG 0x04                             // Offset do registrador IC_TAR
#define IC_CON_REG (I2C0_BASE_ADDR + IC_CON_OFFSET) // Correto
#define IC_DATA_CMD_REG 0x10                        // Offset do registrador IC_DATA_CMD
#define IC_ENABLE_REG 0x6C                          // Offset do registrador IC_ENABLE
#define IC_RXFLR_REG 0x78                           // Offset do registrador IC_RXFLR
// Endereço I2C do ADXL345
#define ADXL345_ADDR 0x53

// Registradores internos do ADXL345
#define ADXL345_REG_DATA_X0 0x32 // Registrador inicial dos dados X, Y, Z (6 bytes)
#define DATA_FORMAT 0x31
#define BW_RATE 0x2C
#define POWER_CTL 0x2D

#define SYSMGR_GENERALIO7 ((volatile unsigned int *)0xFFD0849C)
#define SYSMGR_GENERALIO8 ((volatile unsigned int *)0xFFD084A0)
#define SYSMGR_I2C0USEFPGA ((volatile unsigned int *)0xFFD08704)

#define SCORE_MAX 85

typedef struct
{
    int coord_x;       // current x-coordinate of the sprite on screen.
    int coord_y;       // current y-coordinate of the sprite on screen.
    int direction;     // variable that defines the sprite's movement angle.
    int offset;        // Variable that defines the sprite's memory offset. Used to choose which animation to use.
    int data_register; // Variable that defines in which register the data relating to the sprite will be stored.
    int step_x;        // Number of steps the sprite moves on the X axis.
    int step_y;        // Number of steps the sprite moves on the Y axis.
    int ativo;
    int collision; // 0 - without collision , 1 - collision detected
} Sprite;

/*-------Defining data relating to fixed sprites -------------------------------------------------------------------------*/
typedef struct
{
    int coord_x;       // current x-coordinate of the sprite on screen.
    int coord_y;       // current y-coordinate of the sprite on screen.
    int offset;        // Variable that defines the sprite's memory offset. Used to choose which animation to use.
    int data_register; // Variable that defines in which register the data relating to the sprite will be stored.
    int ativo;
} Sprite_Fixed;

void *i2c_base;
Sprite sprt_1;
Sprite sprt_2;
int16_t accel_data[3] = {0,0,0};
int y_fantasma;
int x_fantasma;
int sentido_fantasma;
int direcao_fantasma;
int sentido;
int direcao;
volatile uint32_t *KEY_ptr;
int fd_mouse;
pthread_mutex_t lock;
int xsoma = 0, ysoma = 0;
unsigned int click_reset = 0;
int fd1;
int x_mouse, y_mouse;

struct input_event ev;

bool verificarColisao(int campo[60][39], Sprite sprite, int direcao, int sentido)
{
    // Calcular as coordenadas dos 4 cantos da sprite
    int cantoSuperiorEsquerdoX = sprite.coord_x;
    int cantoSuperiorEsquerdoY = sprite.coord_y;
    int cantoInferiorDireitoX = sprite.coord_x + 16; // 16 pixels de largura, por isso 15
    int cantoInferiorDireitoY = sprite.coord_y + 16; // 16 pixels de altura, por isso 15

    // Converte as coordenadas dos 4 cantos para índices da matriz de blocos
    int blocoX1 = cantoSuperiorEsquerdoX / 8;
    int blocoY1 = cantoSuperiorEsquerdoY / 8;
    int blocoX2 = cantoInferiorDireitoX / 8;
    int blocoY2 = cantoInferiorDireitoY / 8;

    // Verifica os limites antes de acessar a matriz
    // printf("sprite1 cordX: %d\n", sprite.coord_x);
    // printf("sprite1 cordY: %d\n", sprite.coord_y);
    // printf("blocoX1 1: %d, blocoY1: %d\n", blocoX1, blocoY1);
    // printf("blocoX2 1: %d, blocoY2: %d\n\n\n\n", blocoX2, blocoY2);

    if (direcao == 1)
    { // Movimentação horizontal
        if (sentido == 1)
        { // Direita
            // Verifica se a sprite vai colidir com o próximo bloco na direção direita
            if (campo[blocoY1][blocoX1 + 1] == 0b111000000 ||
                campo[blocoY2][blocoX2 + 1] == 0b111000000)
            {
                return false;
            }
        }
        else
        { // Esquerda
            // Verifica se a sprite vai colidir com o bloco na direção esquerda
            if (campo[blocoY1][blocoX1 - 1] == 0b111000000 ||
                campo[blocoY2][blocoX2 - 1] == 0b111000000)
            {
                return false;
            }
        }
    }
    else
    { // Movimentação vertical
        if (sentido == 1)
        { // Para baixo
            // Verifica se a sprite vai colidir com o próximo bloco na direção para baixo
            if (campo[blocoY1 + 1][blocoX1] == 0b111000000 ||
                campo[blocoY2 + 1][blocoX2] == 0b111000000)
            {
                return false;
            }
        }
        else
        { // Para cima
            // Verifica se a sprite vai colidir com o bloco na direção para cima
            if (campo[blocoY1 - 1][blocoX1] == 0b111000000 ||
                campo[blocoY2 - 1][blocoX2] == 0b111000000)
            {
                return false;
            }
        }
    }

    return true; // Sem colisão
}
void *read_mouse(void *arg)
{

    ssize_t n;
    int x, y;
    int ultimo_x, ultimo_y;

    while (1)
    {
        n = read(fd_mouse, &ev, sizeof(ev));
        if (n == (ssize_t)-1)
        {
            perror("Error reading");
            continue;
        }
        else if (n != sizeof(ev))
        {
            fprintf(stderr, "Error: read %ld bytes, expecting %ld\n", n, sizeof(ev));
            continue;
        }

        pthread_mutex_lock(&lock);

        if (ev.type == EV_REL && ev.code == REL_X)
        {
            direcao_fantasma = 1;
            if (ev.value < 0)
            {
                x_fantasma = -1;
            }
            else if (ev.value > 0)
            {
                x_fantasma = 1;
            }
            else if (ev.value > ultimo_x)
            {
                x_fantasma = 1;
            }
            else if (ev.value < ultimo_x)
            {
                x_fantasma = -1;
            }
            ultimo_x = ev.value;
            if (verificarColisao(campoAtivo, sprt_2, direcao_fantasma, x_fantasma))
            {
                sprt_2.coord_x += x_fantasma;
            }
        }
        if (ev.type == EV_REL && ev.code == REL_Y)
        {
            direcao_fantasma = -1;
            if (ev.value < 0)
            {
                y_fantasma = -1;
            }
            else if (ev.value > 0)
            {
                y_fantasma = 1;
            }
            else if (ev.value > ultimo_y)
            {
                y_fantasma = 1;
            }
            else if (ev.value < ultimo_y)
            {
                y_fantasma = -1;
            }
            ultimo_y = ev.value;
            if (verificarColisao(campoAtivo, sprt_2, direcao_fantasma, y_fantasma))
            {
                sprt_2.coord_y += y_fantasma;
            }
        }
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

void *read_accel(void *arg)
{

    // Mapear a memória do controlador I2C0
    i2c_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, I2C0_BASE_ADDR);
    if (i2c_base == MAP_FAILED)
    {
        perror("Erro ao mapear a memória do I2C");
        close(fd1);
        return -1;
    }

    // pthread_mutex_lock(&lock);
    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = DATA_FORMAT + 0x400;
    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x0B;

    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = BW_RATE + 0x400;
    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x0B;

    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = POWER_CTL + 0x400;
    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x00;

    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = POWER_CTL + 0x400;
    *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x08;

    // Escrever no IC_DATA_CMD para solicitar a leitura dos dados de X, Y, Z
    // Enviar o registrador de início de leitura (0x32 - registrador de dados do ADXL345)
    *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = ADXL345_REG_DATA_X0;

    // Solicitar leitura de 6 bytes (dados de X, Y, Z)
    for (int i = 0; i < 6; i++)
    {
        *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x100; // Cmd para leitura
    }

    // Verificar o IC_RXFLR para garantir que os dados estejam prontos para leitura
    while (*((uint32_t *)(i2c_base + IC_RXFLR_REG)) < 6)
        ;

    // Ler os dados do IC_DATA_CMD (6 bytes: 2 para X, 2 para Y, 2 para Z)
    // int16_t accel_data[3] = {0}; // Array para armazenar os valores de X, Y, Z

    for (int i = 0; i < 3; i++)
    {
        // Lê dois bytes (low byte primeiro, depois o high byte)
        uint8_t low_byte = *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) & 0xFF;
        uint8_t high_byte = *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) & 0xFF;
        accel_data[i] = (int16_t)((high_byte << 8) | low_byte); // Combinar os dois bytes
    }

    printf("\n------------------------------------\n");
    printf("Aceleração em X: %d\n", accel_data[0]);
    printf("Aceleração em Z: %d\n", accel_data[2]);
    printf("\n------------------------------------\n");

    // pthread_mutex_unlock(&lock);
    //  descobre o sentido do movimento
}

void limpaTudo()
{
    for (int i = 0; i < 60; i++)
    {
        for (int j = 0; j < 80; j++)
        {
            // while (1)
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
                R = 0b000;
                G = 0b000;
                B = 0b111;
                // setQuadrado(block_col + x, block_line + y, R, G, B);
                set_background_block(block_col, block_line, R, G, B);
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
                R = 0b000;
                G = 0b000;
                B = 0b111;
                // converterCorParaRGB(cor, &R, &G, &B);
                //  setQuadrado(block_col + x, block_line + y, R, G, B);
                set_background_block(block_col + x, block_line + y, R, G, B);
            }
            // usleep(1500);
        }
    }
}

void desenhaNumero(int matriz[9][8], int x, int y)
{
    // limpa();
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
                R = 0b000;
                G = 0b000;
                B = 0b111;
                // setQuadrado(block_col + x, block_line + y, R, G, B);
                set_background_block(block_col + x, block_line + y, R, G, B);
            }
            // usleep(1500);
        }
    }
}

void limpa()
{
    for (int i = 0; i < 25; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            // while (1)
            //{
            int block_col = j + 50;
            int block_line = i + 15;
            // usleep(5000);
            //  Chama a função para definir o bloco de fundo
            // setQuadrado(block_col, block_line, 0, 0, 0);
            set_background_block(block_col, block_line, 0, 0, 0);
        }
    }
}

void desenhaCampo(int campo[60][39])
{
    for (int i = 0; i < 60; i++)
    {
        for (int j = 0; j < 39; j++)
        {
            int cor = campo[i][j];
            // Extrair os 3 primeiros bits
            int R = (cor >> 6) & 0b111;
            int G = (cor >> 3) & 0b111;
            int B = cor & 0b111;
            int block_col = j;
            int block_line = i;
            // setQuadrado(block_col, block_line, R, G, B);
            //  for (int c = 0; c < 2; c ++){
            //      for (int d = 0; d < 2; d++){
            //          set_background_block((block_col * 2) + c, (block_line * 2) + d, R, G, B); // encontro da coluna e linha
            //      }

            // }
            set_background_block(block_col, block_line, R, G, B); // encontro da coluna e linha
        }
    }
}

void verificaPonto(int campo[60][39], Sprite sprite, int *scorePlayer)
{
    int blocoX = (sprite.coord_x + 8) / 8; // Converter para índice da matriz
    int blocoY = (sprite.coord_y + 8) / 8;

    if (campo[blocoY][blocoX] == 0b000111000)
    {
        (*scorePlayer)++;                    // Incrementa o score
        campo[blocoY][blocoX] = 0b000000000; // Apaga o ponto
        limpa();
    }
    // printf("Pontos: %d\n", *scorePlayer);
}

bool verificaColisaoSprite(Sprite sprite1, Sprite sprite2)
{
    // Definindo os limites da caixa de colisão para o sprite1
    int x1_min = sprite1.coord_x;
    int x1_max = sprite1.coord_x + 16; // largura 16px
    int y1_min = sprite1.coord_y;
    int y1_max = sprite1.coord_y + 16; // altura 16px

    // Definindo os limites da caixa de colisão para o sprite2
    int x2_min = sprite2.coord_x;
    int x2_max = sprite2.coord_x + 16; // largura 16px
    int y2_min = sprite2.coord_y;
    int y2_max = sprite2.coord_y + 16; // altura 16px

    // Verificando se as caixas delimitadoras dos dois sprites se sobrepõem
    if (x1_max > x2_min && x1_min < x2_max && y1_max > y2_min && y1_min < y2_max) {
        return true; // Colisão detectada
    }

    return false; // Sem colisão
}

void elementoPassivo(){
    //
}

void desenharSprite()
{
    for (int i = 0; i < 20; i++)
    {

        for (int j = 0; j < 20; j++)
        {
            set_pixelSprite(0 + i + (j * 20), pacman[j][i]);   // setando o fantasma no primeiro sprite
            set_pixelSprite(400 + i + j * 20, fantasma[j][i]); // setando o pacman no segundo sprite
        }
    }
}

int main()
{

    void *i2c_base;
    int scorePlayer = 0;
    int hp = 1;
    pthread_t threadMouseMove, threadAccel;
    int centenas = 0;
    int dezenas = 0;
    int unidades = 0;

    // Abrir /dev/mem para acessar a memória do sistema
    fd1 = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd1 == -1)
    {
        perror("Erro ao abrir /dev/mem");
        return -1;
    }

    // Abrindo o dispositivo de entrada do mouse
    fd_mouse = open("/dev/input/event0", O_RDONLY); // Substitua X pelo número correto do seu dispositivo
    if (fd_mouse == -1)
    {
        perror("Erro ao abrir /dev/input/event0");
        return -1;
    }

    createMappingMemory();
    desenharSprite();

    KEY_ptr = open_button();

    int numeros[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};
    int numeros1[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};
    int numeros2[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};

    limpaTudo();
    desenhaCampo(campoAtivo);

    desenhaS(score, 50, 3);
    //Unidades
    desenhaNumero(numeros[scorePlayer % 10], 60, 15);
    //Dezenas
    desenhaNumero(numeros1[(scorePlayer / 100) % 10], 55, 15);
    //Centenas
    desenhaNumero(numeros2[scorePlayer / 100], 50, 15);

    sprt_1.ativo = 1;
    sprt_1.data_register = 1;
    // Inicializa a posição do sprite na posição [4][4] da matriz campoAtivo
    sprt_1.coord_x = 1 * 8; // Coluna 4 da matriz, convertida para pixels
    sprt_1.coord_y = 20
     * 8; // Linha 4 da matriz, convertida para pixels

    sprt_1.offset = 0;

    sprt_2.ativo = 1;
    sprt_2.data_register = 2;
    // Inicializa a posição do sprite na posição [4][4] da matriz campoAtivo
    sprt_2.coord_x = 35 * 8; // Coluna 4 da matriz, convertida para pixels
    sprt_2.coord_y = 1 * 8; // Linha 4 da matriz, convertida para pixels

    sprt_2.offset = 1;
    // 18
    // 16
    // set_background_color(0b111,0b000,0b000);
    set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo);
    set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo);

    // printf("Sprite inicializado em: (%d, %d)\n", sprt_1.coord_x, sprt_1.coord_y);
    // set_background_block(18, 16,0b111,0b000,0b111);
    int valor = 1;

    // direcao 1 = direita/esquerda
    // direcao -1 = cima/baixo
    // sentido 1 = é positivo
    // sentido -1 = é negativo
    int direcao = 1;
    int sentido = 0;
    int direcao_fantasma = 1;
    int sentido_fantasma = 1; // Inicializa o mutex
    pthread_mutex_init(&lock, NULL);

    // Cria as threads de leitura do mouse e do acelerômetro

    while (hp == 1)
    {
        // desenha o campo
        desenhaCampo(campoAtivo);
        desenhaS(score, 50, 3);
        desenhaNumero(numeros[scorePlayer % 10], 60, 15);
        desenhaNumero(numeros1[(scorePlayer / 100) % 10], 55, 15);
        desenhaNumero(numeros2[scorePlayer / 100], 50, 15);
        // desenhaCampo((campoTeste.cor));
        // desenha o sprite
        set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo); // pacman
        set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo); // fantasma
        // set_background_block(sprt_1.coord_x, sprt_1.coord_y ,0b000,0b000,0b111);

        // pthread_create(&threadAccel, NULL, read_accel, NULL);
        pthread_create(&threadMouseMove, NULL, read_mouse, NULL);

        // Mapear a memória do controlador I2C0
        i2c_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, I2C0_BASE_ADDR);
        if (i2c_base == MAP_FAILED)
        {
            perror("Erro ao mapear a memória do I2C");
            close(fd1);
            return -1;
        }

        // //pthread_mutex_lock(&lock);
        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = DATA_FORMAT + 0x400;
        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x0B;

        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = BW_RATE + 0x400;
        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x0B;

        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = POWER_CTL + 0x400;
        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x00;

        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = POWER_CTL + 0x400;
        *((volatile uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x08;

        // Escrever no IC_DATA_CMD para solicitar a leitura dos dados de X, Y, Z
        // Enviar o registrador de início de leitura (0x32 - registrador de dados do ADXL345)
        *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = ADXL345_REG_DATA_X0;

        // Solicitar leitura de 6 bytes (dados de X, Y, Z)
        for (int i = 0; i < 6; i++)
        {
            *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) = 0x100; // Cmd para leitura
        }

        // Verificar o IC_RXFLR para garantir que os dados estejam prontos para leitura
        while (*((uint32_t *)(i2c_base + IC_RXFLR_REG)) < 6)
            ;

        // Ler os dados do IC_DATA_CMD (6 bytes: 2 para X, 2 para Y, 2 para Z)
        // int16_t accel_data[3] = {0}; // Array para armazenar os valores de X, Y, Z

        for (int i = 0; i < 3; i++)
        {
            // Lê dois bytes (low byte primeiro, depois o high byte)
            uint8_t low_byte = *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) & 0xFF;
            uint8_t high_byte = *((uint32_t *)(i2c_base + IC_DATA_CMD_REG)) & 0xFF;
            accel_data[i] = (int16_t)((high_byte << 8) | low_byte); // Combinar os dois bytes
        }

        // printf("\n------------------------------------\n");
        // printf("Aceleração em X: %d\n", accel_data[0]);
        // printf("Aceleração em Z: %d\n", accel_data[2]);
        // printf("\n------------------------------------\n");
        pthread_mutex_lock(&lock);

        if (*KEY_ptr == 0b1110)
        {
            printf("Clicou");
            direcao = -1;
        }
        else
        {
            direcao = 1;
        }

        if (x_fantasma < 0)
        {
            sentido_fantasma = -1;
        }

        if (x_fantasma > 0)
        {
            sentido_fantasma = 1;
        }
        if (y_fantasma < 0)
        {
            sentido_fantasma = -1;
        }
        if (y_fantasma > 0)
        {
            sentido_fantasma = 1;
        }

        if (accel_data[2] < -10)
        {
            sentido = -1;
        }
        if (accel_data[2] > 10)
        {
            sentido = 1;
        }
        pthread_mutex_unlock(&lock);

        // Ajustar a movimentação da sprite e verificar colisões
        if (verificarColisao(campoAtivo, sprt_1, direcao, sentido))
        {
            if (direcao == 1)
            {                                  // Horizontal
                sprt_1.coord_x += sentido * 4; // Movimenta 8 pixels por vez
            }
            else
            {                                  // Vertical
                sprt_1.coord_y += sentido * 4; // Movimenta 8 pixels por vez
            }
        }

        if (verificaColisaoSprite(sprt_1, sprt_2))
        {
            
            hp = 0;
            limpaTudo();
            sprt_1.ativo = 0;
            sprt_2.ativo = 0;
            set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo); // pacman
            set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo); 
            desenha(gameOver, 8, 8);
            sleep(3);
            while (1)
            {
                if (*KEY_ptr == 0b1101)
                {
                    scorePlayer = 0;
                    hp = 1;
                    sprt_1.ativo = 1;
                    sprt_2.ativo = 1;
                    sprt_2.coord_x = 35*8;
                    sprt_2.coord_y = 8;
                    set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo); // pacman
                    set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo);
                    break;
                }
            }

            // valor = -1;
            limpaTudo();
        }
        verificaPonto(campoAtivo, sprt_1, &scorePlayer);
        if (scorePlayer == 266)
        {

            hp = 0;
            limpaTudo();
            sprt_1.ativo = 0;
            sprt_2.ativo = 0;

            // TELA DIFERENTE
            desenha(gameOver, 8, 8);
            sleep(3);
            while (1)
            {
                if (*KEY_ptr == 0b1101)
                {
                    scorePlayer = 0;
                    hp = 1;
                    sprt_1.ativo = 1;
                    sprt_2.ativo = 2;
                    break;
                }
            }
            limpaTudo();
        }
    }
    // Fechar recursos e limpar
    pthread_join(threadAccel, NULL);

    // Desabilitar o I2C0 após a operação
    *((uint32_t *)(i2c_base + IC_ENABLE_REG)) = 0x0;
    printf("I2C desabilitado\n");

    // closeMappingMemory();

    // Desmapear a memória e fechar o arquivo de memória
    munmap(i2c_base, MAP_SIZE);
    close(fd1);
    pthread_mutex_destroy(&lock);

    return 0;
}
