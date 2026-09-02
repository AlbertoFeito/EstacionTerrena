//
// File: CoordGPSaPosicion.cpp
//
// Code generated for Simulink model 'CoordGPSaPosicion'.
//
// Model version                  : 1.5
// Simulink Coder version         : 9.3 (R2020a) 18-Nov-2019
// C/C++ source code generated on : Wed Apr 24 20:06:41 2024
//
// Target selection: ert.tlc
// Embedded hardware selection: Intel->x86-64 (Windows64)
// Code generation objective: Execution efficiency
// Validation result: Not run
//
#include "autopiloto/CoordGPSaPosicion.h"
#include "autopiloto/CoordGPSaPosicion_private.h"

real_T rt_modd(real_T u0, real_T u1)
{
  real_T y;
  boolean_T yEq;
  real_T q;
  y = u0;
  if (u1 == 0.0) {
    if (u0 == 0.0) {
      y = u1;
    }
  } else if (u0 == 0.0) {
    y = 0.0 / u1;
  } else {
    y = std::fmod(u0, u1);
    yEq = (y == 0.0);
    if ((!yEq) && (u1 > std::floor(u1))) {
      q = std::abs(u0 / u1);
      yEq = (std::abs(q - std::floor(q + 0.5)) <= DBL_EPSILON * q);
    }

    if (yEq) {
      y = 0.0;
    } else {
      if ((u0 < 0.0) != (u1 < 0.0)) {
        y += u1;
      }
    }
  }

  return y;
}

// Model step function
void CoordGPSaPosicionModelClass::step()
{
  int32_T slat;
  real_T rtb_MathFunction1[9];
  real_T rtb_MathFunction[9];
  real_T rtb_Sum;
  real_T rtb_Switch;
  real_T rtb_Abs1;
  boolean_T rtb_Compare_o;
  real_T rtb_MathFunction1_0[9];
  real_T rtb_sincos_o2_idx_0;
  real_T rtb_sincos_o2_idx_1;
  real_T rtb_sincos_o1_idx_0;
  real_T rtb_sincos_o1_idx_1;

  // Switch: '<S31>/Switch' incorporates:
  //   Abs: '<S31>/Abs'
  //   Bias: '<S31>/Bias'
  //   Bias: '<S31>/Bias1'
  //   Constant: '<S31>/Constant2'
  //   Constant: '<S32>/Constant'
  //   Inport: '<Root>/Latitud_Longitud'
  //   Math: '<S31>/Math Function1'
  //   RelationalOperator: '<S32>/Compare'

  if (std::abs(CoordGPSaPosicion_U.Latitud_Longitud[0]) > 180.0) {
    rtb_Switch = rt_modd(CoordGPSaPosicion_U.Latitud_Longitud[0] + 180.0, 360.0)
      + -180.0;
  } else {
    rtb_Switch = CoordGPSaPosicion_U.Latitud_Longitud[0];
  }

  // End of Switch: '<S31>/Switch'

  // Abs: '<S28>/Abs1'
  rtb_Abs1 = std::abs(rtb_Switch);

  // RelationalOperator: '<S30>/Compare' incorporates:
  //   Constant: '<S30>/Constant'

  rtb_Compare_o = (rtb_Abs1 > 90.0);

  // Switch: '<S28>/Switch' incorporates:
  //   Bias: '<S28>/Bias'
  //   Bias: '<S28>/Bias1'
  //   Gain: '<S28>/Gain'
  //   Product: '<S28>/Divide1'

  if (rtb_Compare_o) {
    // Signum: '<S28>/Sign1'
    if (rtb_Switch < 0.0) {
      rtb_Switch = -1.0;
    } else {
      if (rtb_Switch > 0.0) {
        rtb_Switch = 1.0;
      }
    }

    // End of Signum: '<S28>/Sign1'
    rtb_Switch *= -(rtb_Abs1 + -90.0) + 90.0;
  }

  // End of Switch: '<S28>/Switch'

  // Geod2Geoc: '<S1>/Geodetic to  Geocentric Latitude'
  rtb_Sum = std::abs(rtb_Switch * 0.017453292519943295);
  slat = 1;
  rtb_Abs1 = rtb_Switch * 0.017453292519943295;
  if (rtb_Sum > 3.1415926535897931) {
    if (rtb_Switch * 0.017453292519943295 < -3.1415926535897931) {
      slat = -1;
    }

    rtb_Abs1 = (rt_modd(rtb_Sum + 3.1415926535897931, 6.2831853071795862) -
                3.1415926535897931) * static_cast<real_T>(slat);
    rtb_Sum = std::abs(rtb_Abs1);
  }

  if (rtb_Sum > 1.5707963267948966) {
    if (rtb_Abs1 > 1.5707963267948966) {
      rtb_Abs1 = 1.5707963267948966 - (rtb_Sum - 1.5707963267948966);
    }

    if (rtb_Abs1 < -1.5707963267948966) {
      rtb_Abs1 = -(1.5707963267948966 - (rtb_Sum - 1.5707963267948966));
    }
  }

  rtb_sincos_o2_idx_0 = std::sin(rtb_Abs1);
  rtb_Abs1 = atan2(6.378137E+6 / std::sqrt(1.0 - 0.0066943799901413165 *
    rtb_sincos_o2_idx_0 * rtb_sincos_o2_idx_0) * 0.99330562000985867 *
                   rtb_sincos_o2_idx_0, 6.378137E+6 / std::sqrt(1.0 -
    0.0066943799901413165 * rtb_sincos_o2_idx_0 * rtb_sincos_o2_idx_0) * std::
                   cos(rtb_Abs1)) * 57.295779513082323;

  // End of Geod2Geoc: '<S1>/Geodetic to  Geocentric Latitude'

  // Switch: '<S4>/Switch1' incorporates:
  //   Constant: '<S4>/Constant'
  //   Constant: '<S4>/Constant1'

  if (rtb_Compare_o) {
    slat = 180;
  } else {
    slat = 0;
  }

  // End of Switch: '<S4>/Switch1'

  // Sum: '<S4>/Sum' incorporates:
  //   Inport: '<Root>/Latitud_Longitud'

  rtb_Sum = static_cast<real_T>(slat) + CoordGPSaPosicion_U.Latitud_Longitud[1];

  // Switch: '<S29>/Switch' incorporates:
  //   Abs: '<S29>/Abs'
  //   Bias: '<S29>/Bias'
  //   Bias: '<S29>/Bias1'
  //   Constant: '<S29>/Constant2'
  //   Constant: '<S33>/Constant'
  //   Math: '<S29>/Math Function1'
  //   RelationalOperator: '<S33>/Compare'

  if (std::abs(rtb_Sum) > 180.0) {
    rtb_Sum = rt_modd(rtb_Sum + 180.0, 360.0) + -180.0;
  }

  // End of Switch: '<S29>/Switch'

  // UnitConversion: '<S26>/Unit Conversion'
  // Unit Conversion - from: deg to: rad
  // Expression: output = (0.0174533*input) + (0)
  rtb_sincos_o2_idx_0 = 0.017453292519943295 * rtb_Abs1;
  rtb_sincos_o2_idx_1 = 0.017453292519943295 * rtb_Sum;

  // Trigonometry: '<S3>/sincos'
  rtb_sincos_o1_idx_0 = std::cos(rtb_sincos_o2_idx_0);
  rtb_sincos_o2_idx_0 = std::sin(rtb_sincos_o2_idx_0);
  rtb_sincos_o1_idx_1 = std::cos(rtb_sincos_o2_idx_1);
  rtb_sincos_o2_idx_1 = std::sin(rtb_sincos_o2_idx_1);

  // UnaryMinus: '<S17>/Unary Minus' incorporates:
  //   Product: '<S17>/u(1)*u(4)'

  rtb_MathFunction1[0] = -(rtb_sincos_o2_idx_0 * rtb_sincos_o1_idx_1);

  // UnaryMinus: '<S20>/Unary Minus'
  rtb_MathFunction1[1] = -rtb_sincos_o2_idx_1;

  // UnaryMinus: '<S23>/Unary Minus' incorporates:
  //   Product: '<S23>/u(3)*u(4)'

  rtb_MathFunction1[2] = -(rtb_sincos_o1_idx_0 * rtb_sincos_o1_idx_1);

  // UnaryMinus: '<S18>/Unary Minus' incorporates:
  //   Product: '<S18>/u(1)*u(2)'

  rtb_MathFunction1[3] = -(rtb_sincos_o2_idx_0 * rtb_sincos_o2_idx_1);

  // SignalConversion generated from: '<S27>/Vector Concatenate'
  rtb_MathFunction1[4] = rtb_sincos_o1_idx_1;

  // UnaryMinus: '<S24>/Unary Minus' incorporates:
  //   Product: '<S24>/u(2)*u(3)'

  rtb_MathFunction1[5] = -(rtb_sincos_o2_idx_1 * rtb_sincos_o1_idx_0);

  // SignalConversion generated from: '<S27>/Vector Concatenate'
  rtb_MathFunction1[6] = rtb_sincos_o1_idx_0;

  // SignalConversion generated from: '<S27>/Vector Concatenate' incorporates:
  //   Constant: '<S22>/Constant'

  rtb_MathFunction1[7] = 0.0;

  // UnaryMinus: '<S25>/Unary Minus'
  rtb_MathFunction1[8] = -rtb_sincos_o2_idx_0;

  // Math: '<S1>/Math Function1'
  for (slat = 0; slat < 3; slat++) {
    rtb_MathFunction1_0[3 * slat] = rtb_MathFunction1[slat];
    rtb_MathFunction1_0[3 * slat + 1] = rtb_MathFunction1[slat + 3];
    rtb_MathFunction1_0[3 * slat + 2] = rtb_MathFunction1[slat + 6];
  }

  std::memcpy(&rtb_MathFunction1[0], &rtb_MathFunction1_0[0], 9U * sizeof(real_T));

  // End of Math: '<S1>/Math Function1'

  // UnitConversion: '<S15>/Unit Conversion'
  // Unit Conversion - from: deg to: rad
  // Expression: output = (0.0174533*input) + (0)
  rtb_sincos_o1_idx_0 = 0.017453292519943295 * rtb_Switch;
  rtb_sincos_o1_idx_1 = 0.017453292519943295 * rtb_Sum;

  // Trigonometry: '<S2>/sincos'
  rtb_sincos_o2_idx_0 = std::cos(rtb_sincos_o1_idx_0);
  rtb_sincos_o1_idx_0 = std::sin(rtb_sincos_o1_idx_0);
  rtb_sincos_o2_idx_1 = std::cos(rtb_sincos_o1_idx_1);
  rtb_Switch = std::sin(rtb_sincos_o1_idx_1);

  // UnaryMinus: '<S6>/Unary Minus' incorporates:
  //   Product: '<S6>/u(1)*u(4)'

  rtb_MathFunction[0] = -(rtb_sincos_o1_idx_0 * rtb_sincos_o2_idx_1);

  // UnaryMinus: '<S9>/Unary Minus'
  rtb_MathFunction[1] = -rtb_Switch;

  // UnaryMinus: '<S12>/Unary Minus' incorporates:
  //   Product: '<S12>/u(3)*u(4)'

  rtb_MathFunction[2] = -(rtb_sincos_o2_idx_0 * rtb_sincos_o2_idx_1);

  // UnaryMinus: '<S7>/Unary Minus' incorporates:
  //   Product: '<S7>/u(1)*u(2)'

  rtb_MathFunction[3] = -(rtb_sincos_o1_idx_0 * rtb_Switch);

  // SignalConversion generated from: '<S16>/Vector Concatenate'
  rtb_MathFunction[4] = rtb_sincos_o2_idx_1;

  // UnaryMinus: '<S13>/Unary Minus' incorporates:
  //   Product: '<S13>/u(2)*u(3)'

  rtb_MathFunction[5] = -(rtb_Switch * rtb_sincos_o2_idx_0);

  // SignalConversion generated from: '<S16>/Vector Concatenate'
  rtb_MathFunction[6] = rtb_sincos_o2_idx_0;

  // SignalConversion generated from: '<S16>/Vector Concatenate' incorporates:
  //   Constant: '<S11>/Constant'

  rtb_MathFunction[7] = 0.0;

  // UnaryMinus: '<S14>/Unary Minus'
  rtb_MathFunction[8] = -rtb_sincos_o1_idx_0;

  // Math: '<S1>/Math Function'
  for (slat = 0; slat < 3; slat++) {
    rtb_MathFunction1_0[3 * slat] = rtb_MathFunction[slat];
    rtb_MathFunction1_0[3 * slat + 1] = rtb_MathFunction[slat + 3];
    rtb_MathFunction1_0[3 * slat + 2] = rtb_MathFunction[slat + 6];
  }

  std::memcpy(&rtb_MathFunction[0], &rtb_MathFunction1_0[0], 9U * sizeof(real_T));

  // End of Math: '<S1>/Math Function'

  // UnitConversion: '<S34>/Unit Conversion'
  // Unit Conversion - from: deg to: rad
  // Expression: output = (0.0174533*input) + (0)
  rtb_Abs1 *= 0.017453292519943295;

  // Trigonometry: '<S5>/Trigonometric Function'
  rtb_Abs1 = std::sin(rtb_Abs1);

  // Sqrt: '<S5>/sqrt' incorporates:
  //   Constant: '<S5>/Constant'
  //   Constant: '<S5>/Re'
  //   Product: '<S5>/Product2'
  //   Product: '<S5>/Product3'
  //   Sum: '<S5>/Sum2'

  rtb_Abs1 = std::sqrt(4.0680631590769E+13 / (rtb_Abs1 * rtb_Abs1 *
    0.006739496742276474 + 1.0));
  for (slat = 0; slat < 3; slat++) {
    // Outport: '<Root>/Posicion' incorporates:
    //   Inport: '<Root>/Altura'
    //   Product: '<S1>/Product'
    //   Product: '<S1>/Product1'
    //   Sqrt: '<S5>/sqrt'
    //   Sum: '<S1>/Sum'
    //   UnaryMinus: '<S1>/Unary Minus'
    //   UnaryMinus: '<S1>/Unary Minus1'

    CoordGPSaPosicion_Y.Posicion[slat] = rtb_MathFunction1[slat + 6] * -rtb_Abs1
      + rtb_MathFunction[slat + 6] * -CoordGPSaPosicion_U.Altura;
  }
}

// Model initialize function
void CoordGPSaPosicionModelClass::initialize()
{
  // (no initialization code required)
}

// Model terminate function
void CoordGPSaPosicionModelClass::terminate()
{
  // (no terminate code required)
}

// Constructor
CoordGPSaPosicionModelClass::CoordGPSaPosicionModelClass():
  CoordGPSaPosicion_U()
  ,CoordGPSaPosicion_Y()
{
  // Currently there is no constructor body generated.
}

// Destructor
CoordGPSaPosicionModelClass::~CoordGPSaPosicionModelClass()
{
  // Currently there is no destructor body generated.
}

//
// File trailer for generated code.
//
// [EOF]
//
