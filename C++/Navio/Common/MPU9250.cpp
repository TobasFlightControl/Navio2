#include <cmath>
#include <cassert>

#include "MPU9250.h"

#define DEVICE "/dev/spidev0.1"
#define G_SI 9.80665
#define DEG2RAD (M_PI / 180.)
#define DATA_LENGTH 255

//-----------------------------------------------------------------------------------------------

MPU9250::MPU9250() : spi_dev_(DEVICE, kSpiSpeedHz)
{
}

/*-----------------------------------------------------------------------------------------------
                                    REGISTER READ & WRITE
usage: use these methods to read and write MPU9250 registers over SPI
-----------------------------------------------------------------------------------------------*/

uint8_t MPU9250::WriteReg(uint8_t WriteAddr, uint8_t WriteData)
{
  uint8_t tx[2] = { WriteAddr, WriteData };
  uint8_t rx[2] = { 0 };

  spi_dev_.transfer(tx, rx, 2);

  return rx[1];
}

//-----------------------------------------------------------------------------------------------

uint8_t MPU9250::ReadReg(uint8_t ReadAddr)
{
  return WriteReg(ReadAddr | READ_FLAG, 0x00);
}

//-----------------------------------------------------------------------------------------------

void MPU9250::ReadRegs(uint8_t ReadAddr, uint8_t* ReadBuf, uint32_t Bytes)
{
  assert(Bytes + 1 < DATA_LENGTH);

  uint8_t tx[DATA_LENGTH] = { 0 };
  uint8_t rx[DATA_LENGTH] = { 0 };

  tx[0] = ReadAddr | READ_FLAG;

  spi_dev_.transfer(tx, rx, Bytes + 1);
  // usleep(50);

  for (uint32_t i = 0; i < Bytes; ++i)
    ReadBuf[i] = rx[i + 1];
}

/*-----------------------------------------------------------------------------------------------
                                TEST CONNECTION
usage: call this function to know if SPI and MPU9250 are working correctly.
returns true if mpu9250 answers
-----------------------------------------------------------------------------------------------*/

bool MPU9250::probe()
{
  uint8_t responseXG, responseM;

  responseXG = ReadReg(MPUREG_WHOAMI | READ_FLAG);

  WriteReg(MPUREG_USER_CTRL, 0x20);     // I2C Master mode
  WriteReg(MPUREG_I2C_MST_CTRL, 0x0D);  // I2C configuration multi-master  IIC 400KHz
  WriteReg(MPUREG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | READ_FLAG);  // Set the I2C slave addres of
                                                                // AK8963 and set for read.
  WriteReg(MPUREG_I2C_SLV0_REG, AK8963_WIA);  // I2C slave 0 register address from where to begin
                                              // data transfer
  WriteReg(MPUREG_I2C_SLV0_CTRL, 0x81);       // Read 1 byte from the magnetometer
  usleep(10000);
  responseM = ReadReg(MPUREG_EXT_SENS_DATA_00);

  if (responseXG == 0x71 && responseM == 0x48)
    return true;
  else
    return false;
}

/*-----------------------------------------------------------------------------------------------
                                    INITIALIZATION
usage: call this function at startup for initialize settings of sensor
low pass filter suitable values are:
BITS_DLPF_CFG_256HZ_NOLPF2
BITS_DLPF_CFG_188HZ
BITS_DLPF_CFG_98HZ
BITS_DLPF_CFG_42HZ
BITS_DLPF_CFG_20HZ
BITS_DLPF_CFG_10HZ
BITS_DLPF_CFG_5HZ
BITS_DLPF_CFG_2100HZ_NOLPF
returns 1 if an error occurred
-----------------------------------------------------------------------------------------------*/

#define MPU_InitRegNum 16

void MPU9250::initialize()
{
  uint8_t i = 0;
  uint8_t MPU_Init_Data[MPU_InitRegNum][2] = {
    //{0x80, MPUREG_PWR_MGMT_1},     // Reset Device - Disabled because it seems to corrupt
    // initialisation of AK8963
    { 0x01, MPUREG_PWR_MGMT_1 },  // Clock Source
    { 0x00, MPUREG_PWR_MGMT_2 },  // Enable Acc & Gyro
    { 0x00, MPUREG_CONFIG },  // Use DLPF set Gyroscope bandwidth 184Hz, temperature bandwidth 188Hz
    { 0x18, MPUREG_GYRO_CONFIG },     // +-2000dps
    { 3 << 3, MPUREG_ACCEL_CONFIG },  // +-16G
    { 0x08, MPUREG_ACCEL_CONFIG_2 },  // Set Acc Data Rates, Enable Acc LPF , Bandwidth 184Hz
    { 0x30, MPUREG_INT_PIN_CFG },     //
    //{0x40, MPUREG_I2C_MST_CTRL},   // I2C Speed 348 kHz
    //{0x20, MPUREG_USER_CTRL},      // Enable AUX
    { 0x20, MPUREG_USER_CTRL },     // I2C Master mode
    { 0x0D, MPUREG_I2C_MST_CTRL },  //  I2C configuration multi-master  IIC 400KHz

    { AK8963_I2C_ADDR, MPUREG_I2C_SLV0_ADDR },  // Set the I2C slave addres of AK8963 and set for
                                                // write.
    //{0x09, MPUREG_I2C_SLV4_CTRL},
    //{0x81, MPUREG_I2C_MST_DELAY_CTRL}, //Enable I2C delay

    { AK8963_CNTL2, MPUREG_I2C_SLV0_REG },  // I2C slave 0 register address from where to begin data
                                            // transfer
    { 0x01, MPUREG_I2C_SLV0_DO },           // Reset AK8963
    { 0x81, MPUREG_I2C_SLV0_CTRL },         // Enable I2C and set 1 byte

    { AK8963_CNTL1, MPUREG_I2C_SLV0_REG },  // I2C slave 0 register address from where to begin data
                                            // transfer
    { 0x12, MPUREG_I2C_SLV0_DO },           // Register value to continuous measurement in 16bit
    { 0x81, MPUREG_I2C_SLV0_CTRL }          // Enable I2C and set 1 byte

  };

  // set_acc_scale(BITS_FS_16G);
  // set_gyro_scale(BITS_FS_2000DPS);
  set_acc_scale(BITS_FS_4G);
  set_gyro_scale(BITS_FS_500DPS);

  for (i = 0; i < MPU_InitRegNum; ++i)
  {
    WriteReg(MPU_Init_Data[i][1], MPU_Init_Data[i][0]);
    usleep(100000);  // I2C must slow down the write speed, otherwise it won't work
  }

  calib_mag();
}

/*-----------------------------------------------------------------------------------------------
                                ACCELEROMETER SCALE
usage: call this function at startup, after initialization, to set the right range for the
accelerometers. Suitable ranges are:
BITS_FS_2G
BITS_FS_4G
BITS_FS_8G
BITS_FS_16G
returns the range set (2,4,8 or 16)
-----------------------------------------------------------------------------------------------*/

void MPU9250::set_acc_scale(uint8_t scale)
{
  WriteReg(MPUREG_ACCEL_CONFIG, scale);

  switch (scale)
  {
    case BITS_FS_2G:
      acc_divider = 16384;
      break;
    case BITS_FS_4G:
      acc_divider = 8192;
      break;
    case BITS_FS_8G:
      acc_divider = 4096;
      break;
    case BITS_FS_16G:
      acc_divider = 2048;
      break;
  }
}

/*-----------------------------------------------------------------------------------------------
                                GYROSCOPE SCALE
usage: call this function at startup, after initialization, to set the right range for the
gyroscopes. Suitable ranges are:
BITS_FS_250DPS
BITS_FS_500DPS
BITS_FS_1000DPS
BITS_FS_2000DPS
returns the range set (250,500,1000 or 2000)
-----------------------------------------------------------------------------------------------*/

void MPU9250::set_gyro_scale(uint8_t scale)
{
  WriteReg(MPUREG_GYRO_CONFIG, scale);
  switch (scale)
  {
    case BITS_FS_250DPS:
      gyro_divider = 131;
      break;
    case BITS_FS_500DPS:
      gyro_divider = 65.5;
      break;
    case BITS_FS_1000DPS:
      gyro_divider = 32.8;
      break;
    case BITS_FS_2000DPS:
      gyro_divider = 16.4;
      break;
  }
}

/*-----------------------------------------------------------------------------------------------
                                READ ACCELEROMETER CALIBRATION
usage: call this function to read accelerometer data. Axis represents selected axis:
0 -> X axis
1 -> Y axis
2 -> Z axis
returns Factory Trim value
-----------------------------------------------------------------------------------------------*/

void MPU9250::calib_acc()
{
  uint8_t response[4];
  // read current acc scale
  auto temp_scale = WriteReg(MPUREG_ACCEL_CONFIG | READ_FLAG, 0x00);
  set_acc_scale(BITS_FS_8G);
  // ENABLE SELF TEST need modify
  // temp_scale=WriteReg(MPUREG_ACCEL_CONFIG, 0x80>>axis);

  ReadRegs(MPUREG_SELF_TEST_X, response, 4);
  calib_data[0] = ((response[0] & 11100000) >> 3) | ((response[3] & 00110000) >> 4);
  calib_data[1] = ((response[1] & 11100000) >> 3) | ((response[3] & 00001100) >> 2);
  calib_data[2] = ((response[2] & 11100000) >> 3) | ((response[3] & 00000011));

  set_acc_scale(temp_scale);
}

//-----------------------------------------------------------------------------------------------

void MPU9250::calib_mag()
{
  uint8_t response[3];
  float data;
  int i;

  WriteReg(MPUREG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | READ_FLAG);  // Set the I2C slave addres of
                                                                // AK8963 and set for read.
  WriteReg(MPUREG_I2C_SLV0_REG, AK8963_ASAX);  // I2C slave 0 register address from where to begin
                                               // data transfer
  WriteReg(MPUREG_I2C_SLV0_CTRL, 0x83);        // Read 3 bytes from the magnetometer

  // WriteReg(MPUREG_I2C_SLV0_CTRL, 0x81);    //Enable I2C and set bytes
  usleep(10000);
  // response[0]=WriteReg(MPUREG_EXT_SENS_DATA_01 | READ_FLAG, 0x00);    //Read I2C
  ReadRegs(MPUREG_EXT_SENS_DATA_00, response, 3);

  // response=WriteReg(MPUREG_I2C_SLV0_DO, 0x00);    //Read I2C
  for (i = 0; i < 3; ++i)
  {
    data = response[i];
    magnetometer_ASA[i] = ((data - 128) / 256 + 1) * Magnetometer_Sensitivity_Scale_Factor;
  }
}

//-----------------------------------------------------------------------------------------------

void MPU9250::update()
{
  uint8_t response[21];
  int16_t bit_data[3];
  int i;

  // Send I2C command at first
  WriteReg(MPUREG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | READ_FLAG);  // Set the I2C slave addres of
                                                                // AK8963 and set for read.
  WriteReg(MPUREG_I2C_SLV0_REG, AK8963_HXL);  // I2C slave 0 register address from where to begin
                                              // data transfer
  WriteReg(MPUREG_I2C_SLV0_CTRL, 0x87);       // Read 7 bytes from the magnetometer
  // must start your read from AK8963A register 0x03 and read seven bytes so that upon read of ST2
  // register 0x09 the AK8963A will unlatch the data registers for the next measurement.

  ReadRegs(MPUREG_ACCEL_XOUT_H, response, 21);

  // Get accelerometer value
  for (i = 0; i < 3; ++i)
  {
    bit_data[i] = ((int16_t)response[i * 2] << 8) | response[i * 2 + 1];
  }
  ax_ = G_SI * bit_data[0] / acc_divider;
  ay_ = G_SI * bit_data[1] / acc_divider;
  az_ = G_SI * bit_data[2] / acc_divider;

  // Get temperature
  bit_data[0] = ((int16_t)response[i * 2] << 8) | response[i * 2 + 1];
  temperature = ((bit_data[0] - 21) / 333.87) + 21;

  // Get gyroscope value
  for (i = 4; i < 7; ++i)
  {
    bit_data[i - 4] = ((int16_t)response[i * 2] << 8) | response[i * 2 + 1];
  }
  gx_ = DEG2RAD * bit_data[0] / gyro_divider;
  gy_ = DEG2RAD * bit_data[1] / gyro_divider;
  gz_ = DEG2RAD * bit_data[2] / gyro_divider;

  // Get Magnetometer value
  for (i = 7; i < 10; ++i)
  {
    bit_data[i - 7] = ((int16_t)response[i * 2 + 1] << 8) | response[i * 2];
  }
  mx_ = bit_data[0] * magnetometer_ASA[0];
  my_ = bit_data[1] * magnetometer_ASA[1];
  mz_ = bit_data[2] * magnetometer_ASA[2];
}
