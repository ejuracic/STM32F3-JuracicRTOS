#ifndef SPI_H
#define SPI_H

#include <stdint.h>

// Name: spi1_init
// Description: Configures SPI1 as an 8-bit master using mode 3.
// Parameters: None
// Returns: None
void spi1_init(void);

// Name: spi1_transfer
// Description: Sends and receives one byte through SPI1.
// Parameters: tx - Byte to transmit.
//             rx - Pointer to the received byte.
// Returns: 1 on success, 0 on timeout.
uint8_t spi1_transfer(uint8_t tx, uint8_t *rx);

#endif
