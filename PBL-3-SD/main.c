#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include "graphic_functions.h"
#include "logic_game.h"
#include "logic_game.c"
#include "graphic_functions.c"
#include "Biblioteca_GPU.h"

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
//int scoreTotal = 0; // Pontuação do jogador

int main()
{

    int numeros[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};
    int numeros1[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};
    int numeros2[10] = {num0, num1, num2, num3, num4, num5, num6, num7, num8, num9};



    int jogadas = 0;
    int fd1;
    void *i2c_base;

    // Abrir /dev/mem para acessar a memória do sistema
    fd1 = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd1 == -1)
    {
        perror("Erro ao abrir /dev/mem");
        return -1;
    }

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

    Tabuleiro tab;
    iniTabuleiro(&tab);

    Peca peca = criarPecasAleatorias();
    Peca pecaNova = criarPecasAleatorias();
    volatile uint32_t *KEY_ptr;
    KEY_ptr = open_button();

    set_background_color(0b111, 0, 0);

    int valor = 1;
    int centenas = 0;
    int dezenas = 0;
    int unidades = 0;
    limpaTudo();
    //desenha(tetris, 8, 8);
    //sleep(3);
    atualizaDisplay(0);
    while (1)
    {
        desenha(tetris, 8, 8);
        if (*KEY_ptr == 0b1110)
        {
            //valor = -1;
            limpaDevagar();
            break;
        }
    }

    while (1)
    {
        while (!verificarColisao(&tab, peca))
        {
            usleep(190000);            
            KEY_ptr = open_button();
            //while(1){
                if (*KEY_ptr == 0b1110){ // 4 BOTAO
                    valor *= -1;
                    limpaTudo();
                   // break;
                }
            //}
            
                if (valor == 1)
                {
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
                    while (*((uint32_t *)(i2c_base + IC_RXFLR_REG)) < 6);

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

                    if (*KEY_ptr == 0b1101){
                        //0b1101
                        pecaNova = criarPecasAleatorias();
                        peca = copiaPeca(pecaNova);
                    }
                    desenhaPeca(peca);
                    
                    moverPeca(&peca, 10); // move para baixo
                    if (accel_data[2] < -10)
                    {
                        moverDirOuEsq(&tab, &peca, -10);
                    } // move para a esquerda
                    if (accel_data[2] > 10)
                    {
                        moverDirOuEsq(&tab, &peca, 10);
                    } // move para a direita

                    limpa(peca.pos_y);
                    desenhaS(score, 50, 3);
                    desenhaNumero(numeros[scoreTotal % 10], 60, 15);
                    desenhaNumero(numeros1[(scoreTotal / 100) & 10], 55, 15);
                    desenhaNumero(numeros2[scoreTotal / 100], 50, 15);


                    if (*KEY_ptr == 0b1101){
                        //0b1101
                        pecaNova = criarPecasAleatorias();
                        peca = copiaPeca(pecaNova);
                    }
                    desenhaPeca(peca);
                    mostrarAllPecas(&tab);
                }
                else
                {
                 //   limpaTudo();
                    desenha(pauseMatriz, 8, 8);
                }
            }

            addPecaNoTabuleiro(&tab, peca);
            verificaLinhaCompleta(&tab);
            peca = criarPecasAleatorias();

            if (reiniciarGame(&tab, peca))
            {

                while (1)
                {
                    // desenhaFimDoJogo();
                    // video_erase();

                     limpaTudo();
                    // desenha(gameOver, 8, 8);
                    //sleep(3);
                    while (1)
                    {

                        desenha(gameOver, 8, 8);
                        if (*KEY_ptr == 0b1110){
                            //0b1101
                            limpaDevagar();
                            break;
                        }
                    }
                    
                  
                    desenha(tetris, 8, 8);
                    //sleep(3);
                    scoreTotal = 0;

                    break;
                }
                    limpaDevagar();
                    iniTabuleiro(&tab); // reinicia o tabuleiro ao apertar o botão
                    valor = 1;
            }
            limpaTudo();
            mostrarAllPecas(&tab);
        }
        // Desabilitar o I2C0 após a operação
        *((uint32_t *)(i2c_base + IC_ENABLE_REG)) = 0x0;
        printf("I2C desabilitado\n");

        //closeMappingMemory();

        // Desmapear a memória e fechar o arquivo de memória
        munmap(i2c_base, MAP_SIZE);
        close(fd1);

        return 0;
    
}
