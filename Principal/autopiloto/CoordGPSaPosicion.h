//
// File: CoordGPSaPosicion.h
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
#ifndef RTW_HEADER_CoordGPSaPosicion_h_
#define RTW_HEADER_CoordGPSaPosicion_h_
#include <cfloat>
#include <cmath>
#include <cstring>
#include <math.h>
#include "autopiloto/rtwtypes.h"
#include "autopiloto/CoordGPSaPosicion_types.h"

// Macros for accessing real-time model data structure

// Class declaration for model CoordGPSaPosicion
class CoordGPSaPosicionModelClass {
  // public data and function members
 public:
  // External inputs (root inport signals with default storage)
  typedef struct {
    real_T Latitud_Longitud[2];        // '<Root>/Latitud_Longitud'
    real_T Altura;                     // '<Root>/Altura'
  } ExtU_CoordGPSaPosicion_T;

  // External outputs (root outports fed by signals with default storage)
  typedef struct {
    real_T Posicion[3];                // '<Root>/Posicion'
  } ExtY_CoordGPSaPosicion_T;

  // model initialize function
  void initialize();

  // model step function
  void step();

  // model terminate function
  void terminate();

  // Constructor
  CoordGPSaPosicionModelClass();

  // Destructor
  ~CoordGPSaPosicionModelClass();

  // Root-level structure-based inputs set method

  // Root inports set method
  void setExternalInputs(const ExtU_CoordGPSaPosicion_T
    * pExtU_CoordGPSaPosicion_T)
  {
    CoordGPSaPosicion_U = *pExtU_CoordGPSaPosicion_T;
  }

  // Root-level structure-based outputs get method

  // Root outports get method
  const CoordGPSaPosicionModelClass::ExtY_CoordGPSaPosicion_T
    & getExternalOutputs() const
  {
    return CoordGPSaPosicion_Y;
  }

  // private data and function members
// private:
  // External inputs
  ExtU_CoordGPSaPosicion_T CoordGPSaPosicion_U;

  // External outputs
  ExtY_CoordGPSaPosicion_T CoordGPSaPosicion_Y;
};

//-
//  These blocks were eliminated from the model due to optimizations:
//
//  Block '<S16>/Reshape (9) to [3x3] column-major' : Reshape block reduction
//  Block '<S27>/Reshape (9) to [3x3] column-major' : Reshape block reduction


//-
//  The generated code includes comments that allow you to trace directly
//  back to the appropriate location in the model.  The basic format
//  is <system>/block_name, where system is the system number (uniquely
//  assigned by Simulink) and block_name is the name of the block.
//
//  Use the MATLAB hilite_system command to trace the generated code back
//  to the model.  For example,
//
//  hilite_system('<S3>')    - opens system 3
//  hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
//
//  Here is the system hierarchy for this model
//
//  '<Root>' : 'CoordGPSaPosicion'
//  '<S1>'   : 'CoordGPSaPosicion/LLA to ECEF Position'
//  '<S2>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED'
//  '<S3>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1'
//  '<S4>'   : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap'
//  '<S5>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Radius at Geocentric Latitude'
//  '<S6>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A11'
//  '<S7>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A12'
//  '<S8>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A13'
//  '<S9>'   : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A21'
//  '<S10>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A22'
//  '<S11>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A23'
//  '<S12>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A31'
//  '<S13>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A32'
//  '<S14>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/A33'
//  '<S15>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/Angle Conversion'
//  '<S16>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED/Create Transformation Matrix'
//  '<S17>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A11'
//  '<S18>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A12'
//  '<S19>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A13'
//  '<S20>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A21'
//  '<S21>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A22'
//  '<S22>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A23'
//  '<S23>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A31'
//  '<S24>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A32'
//  '<S25>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/A33'
//  '<S26>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/Angle Conversion'
//  '<S27>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Direction Cosine Matrix ECI to NED1/Create Transformation Matrix'
//  '<S28>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Latitude Wrap 90'
//  '<S29>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Wrap Longitude'
//  '<S30>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Latitude Wrap 90/Compare To Constant'
//  '<S31>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Latitude Wrap 90/Wrap Angle 180'
//  '<S32>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Latitude Wrap 90/Wrap Angle 180/Compare To Constant'
//  '<S33>'  : 'CoordGPSaPosicion/LLA to ECEF Position/LatLong wrap/Wrap Longitude/Compare To Constant'
//  '<S34>'  : 'CoordGPSaPosicion/LLA to ECEF Position/Radius at Geocentric Latitude/Conversion'

#endif                                 // RTW_HEADER_CoordGPSaPosicion_h_

//
// File trailer for generated code.
//
// [EOF]
//
