//------------------------------------------------------------------------------
//  Altalanos I2C buszon talalhato memoriak (EEPROM/FRAM/SARM) kezelese
//
//    File: MyI2CMem.c
//------------------------------------------------------------------------------
#include "MyI2CMem.h"
#include <string.h>
#include "MyHelpers.h"
#include "MyI2CM.h"

//------------------------------------------------------------------------------
//I2C-s memoria letrehozasa
void MyI2CMem_create(MyI2CMem_t* mem, const MyI2CMem_Config_t* cfg)
{
    ASSERT(cfg->i2c);
    ASSERT(cfg->addressFieldSize);
    ASSERT(cfg->addressFieldSize<5);

    MyI2CM_createDevice(&mem->i2cDevice, cfg->i2c, cfg->slaveAddress, NULL);
    mem->addressFieldSize=cfg->addressFieldSize;
    mem->writeBlockSize=cfg->writeBlockSize;
}
//------------------------------------------------------------------------------
//I2C-s memoria olvasasa
//mem: Az eszkoz leiroja
//address: Olvasasi kezdocim
//data: Az olvasott adatokat ide helyezi
//length: Olvasott byteok szama
status_t MyI2CMem_read(MyI2CMem_t* mem,
                       uint32_t address,
                       uint8_t* data, uint32_t length)
{
    status_t status;

    ASSERT(data);

    //Az I2C-s memoriak BIG endianban igenylik a cimet
    address = __builtin_bswap32(address);


    //Adatatviteli blokk leirok listajnak osszeallitasa.
    //(Ez a stcaken marad, amig le nem megy a transzfer!)
    MyI2CM_xfer_t xferBlocks[]=
    {
        //Cim kiirasa. Ez a konfiguracio szerinti hosszusagu lehet
        (MyI2CM_xfer_t){MYI2CM_DIR_TX,
                       ((uint8_t*)&address+4)-mem->addressFieldSize,
                       mem->addressFieldSize},
        //Adattartalom
        (MyI2CM_xfer_t){MYI2CM_DIR_RX, data, length }
    };

    //I2C mukodes kezdemenyezese.
    //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
    status=MYI2CM_transfer(&mem->i2cDevice,
                           xferBlocks,
                           ARRAY_SIZE(xferBlocks));

    return status;
}
//------------------------------------------------------------------------------
//I2C-s memoria irasa.
//A fuggveny kezeli a memorian beluli irolapok kezeleset is.
//mem: Az eszkoz leiroja
//address: Irasi kezdocim
//data: A kiirando adatokra mutat
//length: Kiirando byteok szama
status_t MyI2CMem_write(MyI2CMem_t* mem,
                        uint32_t address,
                        uint8_t* data,
                        uint32_t length)
{
    status_t status=kStatus_Success;

    //Bizonyos I2C memoriak iro lapokkal rendelkeznek, mely laphatarokat kezelni
    //kell...

    //Ciklus addig, amig mindent sikerul kiirni. Azt is vizsgaljuk, hogy nem
    //tortent-e hiba a kiiras kozben...
    while(length && status==kStatus_Success)
    {
        //Egy lepesben kiirhato adatbyte mennyiseg
        uint32_t writeLen=length;

        //A blokkon belul a blokkon beluli kezdocimhez kepest ennyi byte van meg
        //hatra
        if (mem->writeBlockSize)
        {   //Van meghatarozva iroblokk meret
            uint32_t remainInBlock;
            remainInBlock=
                    mem->writeBlockSize - (address % mem->writeBlockSize);
            //Ha kevesebb van hatra, mint amennyi a blokk vegeig lehetne irni,
            //akkor csak annyit irunk, hogy a blokk vegeig vegezzunk.
            if (writeLen>remainInBlock) writeLen=remainInBlock;
        }

        //Az I2C-s memoriak BIG endianban igenylik a cimet
        uint32_t addr = __builtin_bswap32(address);

        //Adatatviteli blokk leirok listajnak osszeallitasa.
        //(Ez a stcaken marad, amig le nem megy a transzfer!)
        MyI2CM_xfer_t xferBlocks[]=
        {
            //Cim kiirasa. Ez a konfiguracio szerinti hosszusagu lehet
            (MyI2CM_xfer_t){MYI2CM_DIR_TX,
                           ((uint8_t*)&addr + 4) - mem->addressFieldSize,
                           mem->addressFieldSize},
            //Adattartalom
            (MyI2CM_xfer_t){MYI2CM_DIR_TX, data, writeLen }
        };

        //I2C muvelet kezdemenyezese.
        //(A rutin megvarja, amig befejezodik az eloirt folyamat!)
        status=MYI2CM_transfer(&mem->i2cDevice,
                               xferBlocks,
                               ARRAY_SIZE(xferBlocks));

        //Ha valami hiba vagy timeot volt, akkor kilepes. Nincs tovabbi blokk
        //irogatas
        if (status) break;

        //Odabblepunk a forrasban
        data+=writeLen;
        //Cel cim leptetese
        address+=writeLen;
        //Hatralevo byteszam csokkentese
        length-=writeLen;

        //Varakozas arra, hogy az eszkoz mar ne legyen foglalt. Amig egy eszkoz
        //foglalt, addig NACK-t ad valasznak.
        while(1)
        {
            status=MyI2CM_ackTest(&mem->i2cDevice);

            if (status==kMyI2CMStatus_NACK)
            {   //Az eszkoz foglalt. Picit varunk, aztan ujra probalkozunk
                vTaskDelay(1);
                continue;
                //TODO: timeoutot belefejleszteni
            }
            break;
        } //while


    } //while

    return status;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
