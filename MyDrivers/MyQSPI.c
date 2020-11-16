//------------------------------------------------------------------------------
//  QSPI driver
//
//    File: MyQSPI.c
//------------------------------------------------------------------------------
#include "MyQSPI.h"
#include <string.h>
#include "MyGCLK.h"
//------------------------------------------------------------------------------
void MyQSPI_init(MyQSPI_t* dev, const MyQSPI_config_t* cfg)
{
    //Driver valtozoinak nullazasa
    memset(dev, 0, sizeof(MyQSPI_t));

    dev->hw=cfg->hw;
    Qspi* hw=cfg->hw;

    //periferia orajel engedelyezese
    MY_ENTER_CRITICAL();
    MCLK->AHBMASK.bit.QSPI_=1;
    MCLK->AHBMASK.bit.QSPI_2X_=1;
    MCLK->APBCMASK.bit.QSPI_=1;
    MY_LEAVE_CRITICAL();

    //Periferia szoftveres resetelese
    hw->CTRLA.reg=QSPI_CTRLA_SWRST;
    __DSB();
    hw->CTRLA.reg=0;
    __DSB();

    hw->CTRLB.reg=
            //Uzemmod kivalasztasa. A Driver memoria eleresi modot tamogat!
            QSPI_CTRLB_MODE_MEMORY |
            //Az utolso atvitelt a LATXFER bittel jelzi a driver
            QSPI_CTRLB_CSMODE_LASTXFER |
            //8 bites adatokkal operalunk
            QSPI_CTRLB_DATALEN(QSPI_CTRLB_DATALEN_8BITS_Val) |
            //Delay Between Consecutive Transfers
            QSPI_CTRLB_DLYBCT(cfg->dlyBCT) |
            //Minimum Inactive CS Delay
            QSPI_CTRLB_DLYCS(cfg->dlyCS);


    hw->BAUD.reg=
            ((cfg->cpol) ? QSPI_BAUD_CPOL : 0) |
            ((cfg->cpha) ? QSPI_BAUD_CPHA : 0) |
            QSPI_BAUD_BAUD(cfg->baudValue) |
            //Delay Before SCK
            QSPI_BAUD_DLYBS(cfg->dlySC);


    //QSPI periferia engedelyezese
    hw->CTRLA.reg = QSPI_CTRLA_ENABLE;

}
//------------------------------------------------------------------------------
//QSPI periferian adat transzfer
status_t MyQSPI_transfer(MyQSPI_t* dev, const MyQSPI_cmd_t* cmd)
{
    Qspi* hw=dev->hw;

    if (cmd->instFrame.bits.addr_en)
    {   //Elo van irva, hogy cim mezo is tartozik az uzenethez. Beallitjuk.
        hw->INSTRADDR.reg=cmd->address;
    }

    uint32_t tmp = hw->INSTRCTRL.reg;
    if (cmd->instFrame.bits.inst_en)
    {   //Elo van irva, hogy parancs mezo is tartozik az uzenethez. Beallitjuk.
        tmp &= ~QSPI_INSTRCTRL_INSTR_Msk;
        tmp |= QSPI_INSTRCTRL_INSTR((uint32_t)cmd->instruction);
    }

    if (cmd->instFrame.bits.opt_en)
    {   //Elo van irva, option parancs mezo is tartozik az uzenethez. Beallitjuk.
        tmp &= ~QSPI_INSTRCTRL_OPTCODE_Msk;
        tmp |= QSPI_INSTRCTRL_OPTCODE((uint32_t)  cmd->option);
    }
    hw->INSTRCTRL.reg=tmp;

    //Instrukcio frame regiszter beallitasa. A cmd.ben talalhato bitmezo fele-
    //pitese megegyezik a regiszter bitjeinek a felepitesevel, igy abba az
    //azonnal beirhato.
    hw->INSTRFRAME.reg=cmd->instFrame.word;

    if (cmd->instFrame.bits.data_en)
    {   //Van adattartalom mozgatas is.

        //Cim kiszamitasa...
        uint8_t *qspiMemPtr = (uint8_t *)QSPI_AHB;
        if (cmd->instFrame.bits.addr_en)
        {
            qspiMemPtr += cmd->address;
        }

        //Szinkronizacio a rendszerbuszhoz, az INSTRFRAME regiszter olvasasaval
        volatile uint32_t dumy=hw->INSTRFRAME.reg;
        (void) dumy;

        ASSERT(cmd->txBuf || cmd->rxBuf);

        uint32_t leftByte=cmd->bufLen;
        //irany szerinti adatmozgatas...
        if (cmd->txBuf)
        {   //Van kimeneti buffer. Kifele irunk...
            const uint8_t* src=(const uint8_t*)cmd->txBuf;
            for(;leftByte; leftByte--)
            {
                *qspiMemPtr++ = *src++;
            }
        } else
        {   //Olvasunk...
            uint8_t* dst=(uint8_t*)cmd->rxBuf;
            for(;leftByte; leftByte--)
            {
                *dst++ = *qspiMemPtr++;
            }
        }
        __DSB();
        __ISB();

    } //if (cmd->inst_frame.bits.data_en)

    //transzfer lezarasa. (A driver ugy van beallitva, hogy a tranzakcio leza-
    //rasat a LASTXFER bit-el jelezzuk
    hw->CTRLA.bit.LASTXFER=1;

    //Varakozas az utasitas befejezesere
    while(hw->INTFLAG.bit.INSTREND==0);
    //IT flag torlese...
    hw->INTFLAG.reg=QSPI_INTFLAG_INSTREND;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
