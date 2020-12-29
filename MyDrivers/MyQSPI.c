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
    dev->disableCacheHandling=cfg->disableCacheHandling;

    Qspi* hw=cfg->hw;

    //periferia orajel engedelyezese
    MY_ENTER_CRITICAL();
    MCLK->AHBMASK.bit.QSPI_=1;
    MCLK->AHBMASK.bit.QSPI_2X_=1;
    MCLK->APBCMASK.bit.QSPI_=1;
    MY_LEAVE_CRITICAL();

    //Periferia szoftveres resetelese
    hw->CTRLA.reg=QSPI_CTRLA_SWRST;
    hw->CTRLA.reg=0;    

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
//Memoria masolas.
static inline void MyQSPI_memcpy(uint8_t *dst, const uint8_t *src, uint32_t count)
{
    //A QSPI periferia olyan hosszusagu buszciklusokat indit, mint amekkoraval
    //az AHB buszt cimezzuk.
    //Itt dedikaltan 8 bites adatokkal operalunk
    while (count--)
    {
        *dst++ = *src++;
    }
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

    //Instrukcio frame regiszter beallitasa. A cmd-ben talalhato bitmezo fele-
    //pitese megegyezik a regiszter bitjeinek a felepitesevel, igy abba az
    //azonnal beirhato.
    hw->INSTRFRAME.reg=cmd->instFrame.word;

    if (cmd->instFrame.bits.data_en)
    {   //Van adattartalom mozgatas is.

        //Cim kiszamitasa...
        uint8_t* qspiMemPtr = (uint8_t*)QSPI_AHB;
        if (cmd->instFrame.bits.addr_en)
        {
            qspiMemPtr += cmd->address;
        }

        //Szinkronizacio a rendszerbuszhoz, az INSTRFRAME regiszter olvasasaval
        uint32_t dumy=hw->INSTRFRAME.reg;
        (void) dumy;

        //Ha van adatmozgatas, akkor valamelyik buffernek definialt kell lennie!
        ASSERT(cmd->txBuf || cmd->rxBuf);

        bool cacheEnabled=false;
        if (dev->disableCacheHandling==false)
        {
            //Annak lekerdezese, hogy a cache kezeles be van-e kapcsolva....
            MY_ENTER_CRITICAL();
            cacheEnabled=((CMCC->SR.bit.CSTS) && (CMCC->CFG.bit.DCDIS==0));
            if (cacheEnabled)
            {   //Az adatmozgatas idejere a D cache-t ki kell kapcsolni
                //cache tiltasa az atprogramozas idejere
                CMCC->CTRL.bit.CEN = 0;
                while (CMCC->SR.bit.CSTS) {}
                //D cache tiltasa
                CMCC->CFG.bit.DCDIS=1;
                //Mindent invalidalunk
                CMCC->MAINT0.bit.INVALL = 1;
                //cache vezerlo visszakapcsolasa
                CMCC->CTRL.bit.CEN = 1;
                while (CMCC->SR.bit.CSTS==0) {}
            }
            MY_LEAVE_CRITICAL();
        }


        //irany szerinti adatmozgatas...
        if (cmd->txBuf)
        {   //Van kimeneti buffer. Kifele irunk...
            MyQSPI_memcpy((uint8_t*)qspiMemPtr,
                          (const uint8_t *)cmd->txBuf,
                          cmd->bufLen);
        } else
        {   //Olvasunk...
            MyQSPI_memcpy((uint8_t *)cmd->rxBuf,
                          (uint8_t*)qspiMemPtr,
                          cmd->bufLen);
        }

        __DSB();
        __ISB();        

        if (dev->disableCacheHandling==false)
        {
            MY_ENTER_CRITICAL();
            if (cacheEnabled)
            {   //Az adatmozgatas elott a D cache be volt kapcsolva. Visszakapcs
                //cache vezerlo tiltasa
                CMCC->CTRL.bit.CEN = 0;
                while (CMCC->SR.bit.CSTS) {}
                //D cache engedelyezese
                CMCC->CFG.bit.DCDIS=0;
                //cache vezerlo ujra engedelyezese
                CMCC->CTRL.bit.CEN = 1;
                while (CMCC->SR.bit.CSTS==0) {}
            }
            MY_LEAVE_CRITICAL();
        }

    } //if (cmd->inst_frame.bits.data_en)

    //transzfer lezarasa. (A driver ugy van beallitva, hogy a tranzakcio leza-
    //rasat a LASTXFER bit-el jelezzuk.)
    hw->CTRLA.reg=QSPI_CTRLA_LASTXFER | QSPI_CTRLA_ENABLE;

    //Varakozas az utasitas befejezesere
    while(hw->INTFLAG.bit.INSTREND==0);
    //IT flag torlese...
    hw->INTFLAG.reg=QSPI_INTFLAG_INSTREND;

    return kStatus_Success;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
