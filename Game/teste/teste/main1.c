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

int scorePlayer = 0;

Sprite sprt_1;
Sprite sprt_2;
int16_t accel_data[3];
int direcao_fantasma;
int sentido_fantasma;
int sentido;
int direcao;
volatile uint32_t *KEY_ptr;

struct input_event ev;

void *function(void *arg)
{
    int valor = *((int *)arg);

    if (valor == 1)
    {
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

        // descobre o sentido do movimento
        if (accel_data[2] < -10)
        {
            sentido = -1;
        }
        if (accel_data[2] > 10)
        {
            sentido = 1;
        }
    }
    else if (valor == 2)
    {
        if (ev.type == EV_KEY)
        {
            if (ev.code == BTN_LEFT)
            {
                if (ev.value == 1)
                {
                    printf("Botão esquerdo pressionado\n");
                    direcao_fantasma = -1;
                }
                else if (ev.value == 0)
                {
                    direcao_fantasma = 1;
                    printf("Botão esquerdo liberado\n");
                }
            }
            else if (ev.code == BTN_RIGHT)
            { // Botão direito
                if (ev.value == 1)
                {
                    direcao_fantasma = -1;
                    printf("Botão direito pressionado\n");
                }
                else if (ev.value == 0)
                {
                    direcao_fant+asma = 1;
                    printf("Botão direito liberado\n");
                }
            }
        }
    }
    else if (valor == 3)
    {

        // Verificando se o evento é de movimento do mouse (REL_X ou REL_Y)
        if (ev.type == EV_REL)
        {
            if (ev.code == REL_X & direcao_fantasma == 1)
            {
                printf("Movimento no eixo X: %d\n", ev.value);
                if (ev.value > 0)
                {
                    sentido_fantasma = 1;
                }
                else
                {
                    sentido_fantasma = -1;
                }
            }
            else if (ev.code == REL_Y & direcao_fantasma == -1)
            {
                printf("Movimento no eixo Y: %d\n", ev.value);
                if (ev.value > 0)
                {
                    sentido_fantasma = 1;
                }
                else
                {
                    sentido_fantasma = -1;
                }
            }
        }
    }
    else if (valor == 4)
    {
        if (*KEY_ptr == 0b1110)
        {
            // valor = -1;
            direcao *= -1;
        }
    }
}
int campoAtivo[27][30] = {
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000}};

int campo1[30][30] = {
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b000111000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b111000000, 0b111000000},
    {0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000, 0b111000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000},
    {0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000, 0b000000000}};

bool verificarColisao(int campo[27][30], Sprite sprite, int direcao, int sentido)
{
    // se ta indo pra direita ou esquerda
    if (direcao == 1)
    {
        // indo pra direita
        if (sentido == 1)
        {
            if (campo[sprite.coord_x + 1][sprite.coord_y] == 0b111000000)
            {
                return false;
            }
            else
            {
                return true;
            }
            // indo pra esquerda
        }
        else
        {
            if (campo[sprite.coord_x - 1][sprite.coord_y] == 0b111000000)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }
    // se ta indo pra cima ou pra baixo
    else
    {
        // ta indo pra pra cima
        if (sentido == 1)
        {
            if (campo[sprite.coord_x][sprite.coord_y + 1] == 0b111000000)
            {
                return false;
            }
            else
            {
                return true;
            }
            // indo pra baixo
        }
        else
        {
            if (campo[sprite.coord_x][sprite.coord_y - 1] == 0b111000000)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }
}

void moverSprite(int campo[30][30], Sprite sprite, int direcao, int sentido)
{
}

void desenhaCampo(int campo[27][30])
{
    for (int i = 0; i < 30; i++)
    {
        for (int j = 0; j < 30; j++)
        {
            int cor = campo[i][j];
            // Extrair os 3 primeiros bits
            int R = (cor >> 6) & 0b111;
            int G = (cor >> 3) & 0b111;
            int B = cor & 0b111;
            int block_col = j;
            int block_line = i;
            // setQuadrado(block_col, block_line, R, G, B);
            set_background_block(block_col, block_line, R, G, B); // encontro da coluna e linha
        }
    }
}

void verificaPonto(int campo[27][30], Sprite sprite)
{
    if (campo[sprite.coord_x][sprite.coord_y] == 0b000111000)
    {
        scorePlayer += 1;
        campo[30][30] = 0b000000000;
    }
}

int main()
{

    int fd2;
    int fd1;
    void *i2c_base;
    pthread threadAccel, threadMouseClick, threadMouseMove, threadBtnClick;
    int t1 = 1;
    int t2 = 2;
    int t3 = 3;
    int t4 = 4;
    // Abrir /dev/mem para acessar a memória do sistema
    fd1 = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd1 == -1)
    {
        perror("Erro ao abrir /dev/mem");
        return -1;
    }

    // Abrindo o dispositivo de entrada do mouse
    fd2 = open("/dev/input/eventX", O_RDONLY); // Substitua X pelo número correto do seu dispositivo

    // Mapear a memória do controlador I2C0
    i2c_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, I2C0_BASE_ADDR);
    if (i2c_base == MAP_FAILED)
    {
        perror("Erro ao mapear a memória do I2C");
        close(fd1);
        return -1;
    }

    // Configurar o registrador IC_CON para:
    // - Mestre
    // - Modo rápido (400 kbit/s)
    // - Endereçamento de 7 bits
    // - Reinício habilitado
    uint32_t ic_con_value = 0x65;
    *((volatile uint32_t *)(i2c_base + IC_CON_OFFSET)) = ic_con_value; // Corrigido

    // Configurar o registrador IC_TAR para:
    // - Endereço de escravo ADXL345 (0x53)
    // - Endereçamento de 7 bits
    uint32_t ic_tar_value = 0x53; // Endereço de escravo (7 bits)
    *((uint32_t *)(i2c_base + IC_TAR_REG)) = ic_tar_value;
    *((uint32_t *)(i2c_base + IC_ENABLE_REG)) = 0x1;

    createMappingMemory();

    volatile uint32_t *KEY_ptr;
    KEY_ptr = open_button();

    desenhaCampo(campoAtivo);

    sprt_1.ativo = 1;
    sprt_1.data_register = 1;
    sprt_1.coord_x = 17;
    sprt_1.coord_y = 19;
    sprt_1.offset = 0;

    sprt_2.ativo = 1;
    sprt_2.data_register = 2;
    sprt_2.coord_x = 27;
    sprt_2.coord_y = 29;
    sprt_2.offset = 1;

    set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo);
    set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo);

    int valor = 1;

    // direcao 1 = direita/esquerda
    // direcao -1 = cima/baixo
    // sentido 1 = é positivo
    // sentido -1 = é negativo
    int direcao = 1;
    int sentido = 1;
    int direcao_fantasma = 1;
    int sentido_fantasma = 1;
    while (1)
    {

        // desenha o campo
        desenhaCampo(campoAtivo);
        // desenha o sprite
        set_sprite(sprt_1.data_register, sprt_1.coord_x, sprt_1.coord_y, sprt_1.offset, sprt_1.ativo);
        set_sprite(sprt_2.data_register, sprt_2.coord_x, sprt_2.coord_y, sprt_2.offset, sprt_2.ativo);

        // Inicialização do accel
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
        int16_t accel_data[3] = {0}; // Array para armazenar os valores de X, Y, Z

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
        // Lendo o evento do dispositivo
        if (read(fd2, &ev, sizeof(struct input_event)) < sizeof(struct input_event))
        {
            perror("Erro ao ler o evento");
            close(fd2);
            return -1;
        }

        // Iniciar threads
        if (pthread_create(&threadAccel, NULL, function, (void *)&t1) != 0)
        {
            perror("Erro ao criar threadAccel");
            exit(EXIT_FAILURE);
        }
        if (pthread_create(&threadMouseClick, NULL, function, (void *)&t2) != 0)
        {
            perror("Erro ao criar threadMouseClick");
            exit(EXIT_FAILURE);
        }
        if (pthread_create(&threadMouseMove, NULL, function, (void *)&t3) != 0)
        {
            perror("Erro ao criar threadMouseMove");
            exit(EXIT_FAILURE);
        }
        if (pthread_create(&threadBtnClick, NULL, function, (void *)&t4) != 0)
        {
            perror("Erro ao criar threadBtnClick");
            exit(EXIT_FAILURE);
        }

        // Esperar threads finalizarem
        pthread_join(threadAccel, NULL);
        pthread_join(threadMouseClick, NULL);
        pthread_join(threadMouseMove, NULL);
        pthread_join(threadBtnClick, NULL);

        // bool verificarColisao(int campo[30][30], Sprite sprite, int direcao, int sentido)
        if (verificarColisao(campoAtivo, sprt_1, direcao, sentido) == true)
        {
            if (direcao == 1)
            {
                sprt_1.coord_x = sprt_1.coord_x + sentido;
            }
            else
            {
                sprt_1.coord_y = sprt_1.coord_y + sentido;
            }
        }

        if (verificarColisao(campoAtivo, sprt_2, direcao, sentido) == true)
        {
            if (direcao == 1)
            {
                sprt_2.coord_x = sprt_2.coord_x + sentido;
            }
            else
            {
                sprt_2.coord_y = sprt_2.coord_y + sentido;
            }
        }

        // verifica se fez ponto e apaga o quadrado do ponto
        verificaPonto(campoAtivo, sprt_1);
    }
    // Desabilitar o I2C0 após a operação
    *((uint32_t *)(i2c_base + IC_ENABLE_REG)) = 0x0;
    printf("I2C desabilitado\n");

    // closeMappingMemory();

    // Desmapear a memória e fechar o arquivo de memória
    munmap(i2c_base, MAP_SIZE);
    close(fd1);

    return 0;
}
