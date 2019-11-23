# RF24 Library for Irri Project
Optimized fork of nRF24L01 for Raspberry Pi/Linux patched to use SPI1 instead of SPI0. The main changes were done in the SPI layer to use bcm2835_AUX_SPI functions.
## Usage

Configure:
`sudo ./configure --driver=RPi`

Make:
`sudo make install -B`

Links and Cache shared libraries:
`sudo ldconfig`
