//------------------------------------------------------------------------------
//  QSPI driver
//
//    File: MyQSPI.h
//------------------------------------------------------------------------------
//STUFF:
// http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-44065-Execute-in-Place-XIP-with-Quad-SPI-Interface-SAM-V7-SAM-E7-SAM-S7_Application-Note.pdf
#ifndef MYQSPI_H_
#define MYQSPI_H_

#include "MyCommon.h"


//Qspi access modes
enum
{
    //Read access
    MYQSPI_READ_ACCESS=QSPI_INSTRFRAME_TFRTYPE_READ_Val,
    //Read memory access
    MYQSPI_READMEM_ACCESS=QSPI_INSTRFRAME_TFRTYPE_READMEMORY_Val,
    //Write access
    MYQSPI_WRITE_ACCESS=QSPI_INSTRFRAME_TFRTYPE_WRITE_Val,
    //Write memory access
    MYQSPI_WRITEMEM_ACCESS=QSPI_INSTRFRAME_TFRTYPE_WRITEMEMORY_Val
};

//QSPI command instruction/address/data width
enum
{
    //Instruction: Single-bit, Address: Single-bit, Data: Single-bit
    MYQSPI_INST1_ADDR1_DATA1=QSPI_INSTRFRAME_WIDTH_SINGLE_BIT_SPI_Val,
    //Instruction: Single-bit, Address: Single-bit, Data: Dual-bit
    MYQSPI_INST1_ADDR1_DATA2=QSPI_INSTRFRAME_WIDTH_DUAL_OUTPUT_Val,
    //Instruction: Single-bit, Address: Single-bit, Data: Quad-bit
    MYQSPI_INST1_ADDR1_DATA4=QSPI_INSTRFRAME_WIDTH_QUAD_OUTPUT_Val,
    //Instruction: Single-bit, Address: Dual-bit, Data: Dual-bit
    MYQSPI_INST1_ADDR2_DATA2=QSPI_INSTRFRAME_WIDTH_DUAL_IO_Val,
    //Instruction: Single-bit, Address: Quad-bit, Data: Quad-bit
    MYQSPI_INST1_ADDR4_DATA4=QSPI_INSTRFRAME_WIDTH_QUAD_IO_Val,
    //Instruction: Dual-bit, Address: Dual-bit, Data: Dual-bit
    MYQSPI_INST2_ADDR2_DATA2=QSPI_INSTRFRAME_WIDTH_DUAL_CMD_Val,
    //Instruction: Quad-bit, Address: Quad-bit, Data: Quad-bit
    MYQSPI_INST4_ADDR4_DATA4=QSPI_INSTRFRAME_WIDTH_QUAD_CMD_Val
};


//QSPI command option code length in bits
enum
{
    //The option code is 1 bit long
    MYQSPI_OPT_1BIT=QSPI_INSTRFRAME_OPTCODELEN_1BIT_Val,
    //The option code is 2 bits long
    MYQSPI_OPT_2BIT=QSPI_INSTRFRAME_OPTCODELEN_2BITS_Val,
    //The option code is 4 bits long
    MYQSPI_OPT_4BIT=QSPI_INSTRFRAME_OPTCODELEN_4BITS_Val,
    //The option code is 8 bits long
    MYQSPI_OPT_8BIT=QSPI_INSTRFRAME_OPTCODELEN_8BITS_Val
};
//------------------------------------------------------------------------------
//QSPI-n torteno parancsot leiro struktura
typedef struct
{
    union
    {
       #pragma pack(1)
        struct
        {
            /* Width of QSPI Addr , inst data */
            uint32_t width : 3;
            /* Reserved */
            uint32_t reserved0 : 1;
            /* Enable Instruction */
            uint32_t inst_en : 1;
            /* Enable Address */
            uint32_t addr_en : 1;
            /* Enable Option */
            uint32_t opt_en : 1;
            /* Enable Data */
            uint32_t data_en : 1;
            /* Option Length */
            uint32_t opt_len : 2;
            /* Address Length */
            uint32_t addr_len : 1;
            /* Option Length */
            uint32_t reserved1 : 1;
            /* Transfer type */
            uint32_t tfr_type : 2;
            /* Continuous read mode */
            uint32_t continues_read : 1;
            /* Enable Double Data Rate */
            uint32_t ddr_enable : 1;
            /* Dummy Cycles Length */
            //Dumy orajelek szama
            uint32_t dummy_cycles : 5;
            /* Reserved */
            uint32_t reserved3 : 11;
        } bits;
        #pragma pack()
        uint32_t word;
    } instFrame;

    uint8_t  instruction;
    uint8_t  option;
    uint32_t address;

    uint32_t    bufLen;
    const void* txBuf;
    uint8_t*    rxBuf;
} MyQSPI_cmd_t;
//------------------------------------------------------------------------------
//QSPI drivert inicializalo struktura
typedef struct
{
    //QSPI periferiara mutato pointer
    Qspi* hw;

    //Clock Polarity
    //false: The inactive state value of SPCK is logic level zero.
    //true:  The inactive state value of SPCK is logic level one.
    bool cpol;

    //Clock Phase
    //false: Data is changed on the leading edge of SPCK and captured on the
    //       following edge of SPCK.
    //true:  Data is captured on the leading edge of SPCK and changed on the
    //       following edge of SPCK.
    bool cpha;

    //baud regiszterbe irando ertek
    uint32_t baudValue;

    //Delay Between Consecutive Transfers
    uint32_t dlyBCT;
    //Minimum Inactive CS Delay
    uint32_t dlyCS;
    //Delay Before SCK
    uint32_t dlySC;

    //true eseten tiltja a D cache ki/be kapcsolast az adatmozgatas idejere.
    //Ez azert lenyeges, mert a QSPI adat regija (AHB buszon) alapertelmezesben
    //cachelve van. A cache vezerlo pedig tobbszoros busz hozzaferest okoz, ami
    //fals adat transzfert indit a QSPI-n.
    bool disableCacheHandling;
} MyQSPI_config_t;
//------------------------------------------------------------------------------
//MyQSPI valtozoi
typedef struct
{
    //QSPI regisztereire mutato pointer
    Qspi* hw;
    //Data cache letiltasanak tiltasa az adatmozgatasok idejere, a driver
    //altal.
    bool disableCacheHandling;
} MyQSPI_t;
//------------------------------------------------------------------------------
//Driver inicializalasa
void MyQSPI_init(MyQSPI_t* dev, const MyQSPI_config_t* cfg);

//QSPI periferian adat transzfer
status_t MyQSPI_transfer(MyQSPI_t* dev, const MyQSPI_cmd_t* cmd);
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif //MYQSPI_H_
