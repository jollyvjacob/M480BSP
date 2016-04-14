/**************************************************************************//**
 * @file     main.c
 * @version  V3.00
 * @brief
 *           This is a USCI_UART PDMA demo and need to connect UACI_UART TX and RX.
 *
 * @copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 *
 ******************************************************************************/
#include "stdio.h"
#include "M480.h"

#define PDMA_TEST_LENGTH   128

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
uint8_t g_u8Tx_Buffer[PDMA_TEST_LENGTH] = {0};
uint8_t g_u8Rx_Buffer[PDMA_TEST_LENGTH] = {0};

volatile uint32_t u32IsTestOver = 0;

/*---------------------------------------------------------------------------------------------------------*/
/* Define functions prototype                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void);
void USCI_UART_PDMATest(void);

void PDMA_IRQHandler(void)
{
    uint32_t status = PDMA_GET_INT_STATUS();

    if (status & 0x1) { /* abort */
        printf("target abort interrupt !!\n");
        if (PDMA_GET_ABORT_STS() & 0x4)
            u32IsTestOver = 2;
        PDMA_CLR_ABORT_FLAG(PDMA_GET_ABORT_STS());
    } else if (status & 0x2) { /* done */
        if ( (PDMA_GET_TD_STS() & (1 << 0)) && (PDMA_GET_TD_STS() & (1 << 1)) ) {
            u32IsTestOver = 1;
            PDMA_CLR_TD_FLAG(PDMA_GET_TD_STS());
        }
    } else if (status & 0x300) { /* channel 2 timeout */
        printf("timeout interrupt !!\n");
        u32IsTestOver = 3;
        PDMA_CLR_TMOUT_FLAG(0);
        PDMA_CLR_TMOUT_FLAG(1);
    } else
        printf("unknown interrupt !!\n");
}

void SYS_Init(void)
{

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable HXT and LXT */
    CLK->PWRCTL |= CLK_PWRCTL_HXTEN_Msk; // XTAL12M (HXT) Enabled
    CLK->PWRCTL |= CLK_PWRCTL_LXTEN_Msk; // 32K (LXT) Enabled

    /* Waiting clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(192000000);
    CLK->PCLKDIV = (CLK_PCLKDIV_PCLK0DIV2 | CLK_PCLKDIV_PCLK1DIV2);

    /* Select IP clock source */
    CLK->CLKSEL1 &= ~CLK_CLKSEL1_UART0SEL_Msk;
    CLK->CLKSEL1 |= (0x3 << CLK_CLKSEL1_UART0SEL_Pos);// Clock source from HIRC

    /* Enable IP clock */
    CLK->APBCLK0 |= CLK_APBCLK0_UART0CKEN_Msk; // UART0 Clock Enable
    CLK->APBCLK1 |= CLK_APBCLK1_USCI0CKEN_Msk; // UUART0 Clock Enable
    CLK->AHBCLK  |= CLK_AHBCLK_PDMACKEN_Msk;   // PDMA Clock Enable

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Set PD multi-function pins for UART0 RXD and TXD */
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD2MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD2MFP_UART0_RXD | SYS_GPD_MFPL_PD3MFP_UART0_TXD);

    /* Set PC multi-function pins for UUART0 RXD, TXD, CTS, RTS */
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE3MFP_Msk | SYS_GPE_MFPL_PE4MFP_Msk | SYS_GPE_MFPL_PE5MFP_Msk | SYS_GPE_MFPL_PE6MFP_Msk);
    SYS->GPE_MFPL |= SYS_GPE_MFPL_PE3MFP_USCI0_DAT0 | SYS_GPE_MFPL_PE4MFP_USCI0_DAT1 |
                     SYS_GPE_MFPL_PE5MFP_USCI0_CTL1 | SYS_GPE_MFPL_PE6MFP_USCI0_CTL0;
}

void UART0_Init()
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    UART_Open(UART0, 115200);
}

void USCI0_Init()
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init USCI                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Configure USCI0 as UART mode */
    UUART_Open(UUART0, 115200);
}

void PDMA_Init(void)
{
    /* Open PDMA Channel */
    PDMA_Open(1 << 0); // Channel 0 for UART1 TX
    PDMA_Open(1 << 1); // Channel 1 for UART1 RX
    // Select basic mode
    PDMA_SetTransferMode(0, PDMA_USCI0_TX, 0, 0);
    PDMA_SetTransferMode(1, PDMA_USCI0_RX, 0, 0);
    // Set data width and transfer count
    PDMA_SetTransferCnt(0, PDMA_WIDTH_8, PDMA_TEST_LENGTH);
    PDMA_SetTransferCnt(1, PDMA_WIDTH_8, PDMA_TEST_LENGTH);
    //Set PDMA Transfer Address
    PDMA_SetTransferAddr(0, ((uint32_t) (&g_u8Tx_Buffer[0])), PDMA_SAR_INC, (uint32_t)(&(UUART0->TXDAT)), PDMA_DAR_FIX);
    PDMA_SetTransferAddr(1, (uint32_t)(&(UUART0->RXDAT)), PDMA_SAR_FIX, ((uint32_t) (&g_u8Rx_Buffer[0])), PDMA_DAR_INC);
    //Select Single Request
    PDMA_SetBurstType(0, PDMA_REQ_SINGLE, 0);
    PDMA_SetBurstType(1, PDMA_REQ_SINGLE, 0);
    //Set timeout
    //PDMA_SetTimeOut(0, 0, 0x5555);
    //PDMA_SetTimeOut(1, 0, 0x5555);

#ifdef ENABLE_PDMA_INTERRUPT
    PDMA_EnableInt(0, 0);
    PDMA_EnableInt(1, 0);
    NVIC_EnableIRQ(PDMA_IRQn);
    u32IsTestOver = 0;
#endif
}


/*---------------------------------------------------------------------------------------------------------*/
/* USCI UART Test Sample                                                                                   */
/* Test Item                                                                                               */
/* It sends the received data to HyperTerminal.                                                            */
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();

    /* Lock protected registers */
    SYS_LockReg();

    /* Init USCI0 for printf */
    UART0_Init();

    /* Init USCI0 for test */
    USCI0_Init();

    /*---------------------------------------------------------------------------------------------------------*/
    /* SAMPLE CODE                                                                                             */
    /*---------------------------------------------------------------------------------------------------------*/

    printf("\n\nCPU @ %d Hz\n", SystemCoreClock);

    printf("\nUSCI UART Sample Program\n");

    /* USCI UART sample function */
    USCI_UART_PDMATest();

    while(1);

}

/*---------------------------------------------------------------------------------------------------------*/
/*  USCI UART Function Test                                                                                */
/*---------------------------------------------------------------------------------------------------------*/
void USCI_UART_PDMATest()
{
    uint32_t i;

    printf("+-----------------------------------------------------------+\n");
    printf("|  USCI UART PDMA Test                                  |\n");
    printf("+-----------------------------------------------------------+\n");
    printf("|  Description :                                            |\n");
    printf("|    The sample code will demo USCI_UART PDMA function      |\n");
    printf("|    Please connect USCI0_UART_TX and USCI0_UART_RX pin.    |\n");
    printf("+-----------------------------------------------------------+\n");
    printf("Please press any key to start test. \n\n");

    for (i = 0; i < PDMA_TEST_LENGTH; i++) {
        g_u8Tx_Buffer[i] = i;
        g_u8Rx_Buffer[i] = 0xff;
    }

    /* PDMA reset */
    UUART0->PDMACTL |= UUART_PDMACTL_PDMARST_Msk;
    while( UUART0->PDMACTL & UUART_PDMACTL_PDMARST_Msk );

    PDMA_Init();

    /* Enable UART DMA Tx and Rx */
    UUART0->PDMACTL = UUART_PDMACTL_TXPDMAEN_Msk | UUART_PDMACTL_RXPDMAEN_Msk | UUART_PDMACTL_PDMAEN_Msk;

#ifdef ENABLE_PDMA_INTERRUPT
    while(u32IsTestOver == 0);

    if (u32IsTestOver == 1)
        printf("test done...\n");
    else if (u32IsTestOver == 2)
        printf("target abort...\n");
    else if (u32IsTestOver == 3)
        printf("timeout...\n");
#else
    while( (!(PDMA_GET_TD_STS() & (1 << 0))) || (!(PDMA_GET_TD_STS() & (1 << 1))) );

    PDMA_CLR_TD_FLAG(PDMA_TDSTS_TDIF0_Msk|PDMA_TDSTS_TDIF1_Msk);
#endif

    for (i=0; i<PDMA_TEST_LENGTH; i++) {
        if(g_u8Rx_Buffer[i] != i) {
            printf("\n Receive Data Compare Error !!");
            while(1);
        }

    }

    printf("\nUART PDMA test Pass.\n");

}
