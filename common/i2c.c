
#include "gpio.h"
#include "i2c.h"
#include "clock.h"

#define GPIO_BASE   ( 0x41004400 )
#define SERCOM_BASE ( 0x42000800 )
#define GCLK_BASE   ( 0x40000C00 )
#define PM_APBC     ( *( ( volatile unsigned int *)0x40000420 ) )

typedef struct
{
    unsigned int CTRLA:32;
    unsigned int CTRLB:32;
    unsigned int RESERVED_0:32;
    unsigned int BAUD:32;
    unsigned int RESERVED_1:32;
    unsigned char INTENCLR:8;
    unsigned char RESERVED_2:8;
    unsigned char INTENSET:8;
    unsigned char RESERVED_3:8;
    unsigned char INTFLAG:8;
    unsigned char RESERVED_4:8;
    unsigned short STATUS:16;
    unsigned int SYNCBUSY:32;
    unsigned int RESERVED_5:32;
    unsigned int ADDR:32;
    unsigned short DATA:16;
    unsigned char RESERVED_6:8;
    unsigned char RESERVED_7:8;
    unsigned char RESERVED_8:8;
    unsigned char RESERVED_9:8;
    unsigned char RESERVED_10:8;
    unsigned char RESERVED_11:8;
    unsigned char DBGCTRL:8;
} i2c_t;

volatile i2c_t  * SERCOM    = ( i2c_t * ) SERCOM_BASE; 
volatile gpio_t * GPIO      = ( gpio_t * ) GPIO_BASE;
volatile gclk_t * GCLK      = ( gclk_t * ) GCLK_BASE;

void I2C_Init( void )
{
    Clock_ConfigureGCLK( 0x7, 0x1, 0x14 );

    PM_APBC |= ( 0x1 << 2U );

    /* GPIO */
    /* PA8 = SDA, MUX C */
    GPIO->DIRR |= ( 0x3 << 8 );
    GPIO->OUTSET |= ( 0x3 << 8 );
    GPIO->PINCFG8 |= ( 0x1 << 0 );
    GPIO->PMUX4 |= 0x2;
    
    /* PA9 = SCL, MUX C */
    GPIO->PINCFG9 |= ( 0x1 << 0 );
    GPIO->PMUX4 |= ( 0x2 << 4 );

    /* SERCOM 0 */
    /* Set I2C Host Mode */
    SERCOM->CTRLA |= ( 0x5 << 2U );

    /* Enable Smart mode */
    SERCOM->CTRLB |= ( 0x1 << 8U );

    /* Enable SCL Timeout */
    SERCOM->CTRLA |= ( 0x1 << 30U ) | ( 0x1 << 22 );

    /* Baud */
    SERCOM->BAUD |= ( 23U << 0U );

    SERCOM->CTRLA |= ( 0x1 << 24 );

    /* Enable */
    SERCOM->CTRLA |= ( 0x1 << 1 );
    WAITCLR( SERCOM->SYNCBUSY, 1U);
    
    /* Force IDLE state */
    SERCOM->STATUS |= ( 0x1 << 4 );
    while( ( SERCOM->SYNCBUSY & ( 1 << 2 ) ) );
    
    /* Send ACK after each data read */ 
    SERCOM->CTRLB &= ~( 0x1 << 18 );
}

extern bool I2C_Write( uint8_t address, uint8_t * buffer, uint8_t len )
{
    bool ret = false;

    /* Send Address */
    SERCOM->ADDR = ( ( address << 1 ) );
    WAITCLR( SERCOM->SYNCBUSY, 2U );
    WAITSET( SERCOM->INTFLAG, 0U );

    if( !(SERCOM->STATUS & ( 0x4 )) )
    {
        /* Write byte and sync */ 
        uint8_t i = 0U;
        for( i = 0U; i < (len - 1U); i++ )
        {
            SERCOM->DATA = buffer[i];
            WAITCLR( SERCOM->SYNCBUSY, 2U );
            WAITSET( SERCOM->INTFLAG, 0U );
        }
    
        SERCOM->DATA = buffer[i];
        WAITCLR( SERCOM->SYNCBUSY, 2U );
        WAITSET( SERCOM->INTFLAG, 0U );
        
        ret = true;
    }

    /* Stop Condition */
    SERCOM->CTRLB |= ( 0x3 << 16 );
    WAITCLR( SERCOM->SYNCBUSY, 2U );
    return ret;
}

extern void I2C_Read( uint8_t address, uint8_t * buffer, uint8_t len )
{
    /* Send Address */
    SERCOM->ADDR = ( ( address << 1 ) | 0x1);
    while( ( SERCOM->SYNCBUSY & ( 1 << 2 ) ) );
    while( !( SERCOM->INTFLAG & ( 1 << 1 ) ) );
   
    /* Read byte and sync */ 
    uint8_t i = 0U;
    for( i = 0U; i < (len - 1U); i++ )
    {
        buffer[i] = SERCOM->DATA;
        WAITCLR( SERCOM->SYNCBUSY, 2U );
        WAITSET( SERCOM->INTFLAG, 1U );
    }

    /* Read Last Byte */
    buffer[i] = SERCOM->DATA;
    WAITCLR( SERCOM->SYNCBUSY, 2U );
    WAITCLR( SERCOM->INTFLAG, 1 );
  
    /* Stop Condition */
    SERCOM->CTRLB |= ( 0x3 << 16 );
    WAITCLR( SERCOM->SYNCBUSY, 2U );
}



