#pragma once

#include "../Common/RCOutput.h"
#include "./PCA9685.h"

class RCOutput_Navio : public RCOutput
{
public:
  explicit RCOutput_Navio();
  bool initialize(const uint32_t&) override;
  bool enable(const uint32_t&) override;
  bool setFrequency(const uint32_t&, const float& frequency) override;
  bool setDutyCycle(const uint32_t& channel, const float& period) override;

private:
  PCA9685 pwm;
};
