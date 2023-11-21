uint16_t xc, yc; 
uint8_t isTouch;
int16_t xc_old, yc_old;

// PS3 angles
void getAnglesPS3(float * angles) {

      float accXval, accYval, accZval;

      // Data for the Kionix KXPC4 used in the DualShock 3
      const float zeroG = 511.5f; // 1.65/3.3*1023 (1,65V)
      accXval = -((float)((joystick.getAxis(50 - 9) << 8) | joystick.getAxis(51 - 9)) - zeroG);
      accYval = -((float)((joystick.getAxis(52 - 9) << 8) | joystick.getAxis(53 - 9)) - zeroG);
      accZval = -((float)((joystick.getAxis(54 - 9) << 8) | joystick.getAxis(55 - 9)) - zeroG);
      //printf("\tax: %f, ay: %f, az: %f\n", accXval, accYval, accZval);
      angles[0] =  ((atan2f(accYval, accZval) + PI) * RAD_TO_DEG);
      angles[1]  = ((atan2f(accXval, accZval) + PI) * RAD_TO_DEG);

}

// https://stackoverflow.com/questions/111928/is-there-a-printf-converter-to-print-in-binary-format
// Assumes little endian
void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}
