/**
 * Copyright 2014 Alexander Debus, Axel Huebl
 *
 * This file is part of PIConGPU.
 *
 * PIConGPU is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PIConGPU is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PIConGPU.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"
#include "simulation_defines.hpp"
#include "simulation_classTypes.hpp"

#include "math/Vector.hpp"
#include "dimensions/DataSpace.hpp"
#include "mappings/simulation/SubGrid.hpp"
#include "math/Complex.hpp"
#include "fields/background/templates/bgrTWTS.def"

namespace picongpu
{
/** Load external TWTS field
 *
 */
namespace templates
{
namespace pmMath = PMacc::algorithms::math;

/** Auxiliary functions for calculating the TWTS field */
namespace detail
{
    template <typename T_Type, T_AngleType>
    struct RotateField;
    
    template <typename T_Type, T_AngleType>
    struct RotateField<PMacc::math::Vector<T_Type,3>, T_AngleType >
    {
        typedef PMacc::math::Vector<T_Type,3> result;
        typedef T_AngleType AngleType;
        HDINLINE result
        operator()( const result& fieldVector,
                    const AngleType phi ) const
        {
            /*  Since, the laser propagation direction encloses an angle of phi with the
             *  simulation y-axis (i.e. direction of sliding window), the positions vectors are
             *  rotated around the simulation x-axis before calling the TWTS field functions.
             *  Note: The TWTS field functions are in non-rotated frame and only use the angle
             *  phi to determine the required amount of pulse front tilt.
             *  RotationMatrix[PI/2+phi].(y,z) (180Deg-flip at phi=90Deg since coordinate
             *  system in paper is oriented the other way round.) */
            return result(
                fieldPosVector.x(),
               -pmMath::sin(AngleType(phi))*fieldPosVector.y()
                    -pmMath::cos(AngleType(phi))*fieldPosVector.z() ,
               +pmMath::cos(AngleType(phi))*fieldPosVector.y()
                    -pmMath::sin(AngleType(phi))*fieldPosVector.z() );
        }
                        
    };
    
    template <typename T_Type, T_AngleType>
    struct RotateField<PMacc::math::Vector<T_Type,2>, T_AngleType >
    {
        typedef PMacc::math::Vector<T_Type,2> result;
        typedef T_AngleType AngleType;
        HDINLINE result
        operator()( const result& fieldVector,
                    const AngleType phi ) const
        {
            /*  Since, the laser propagation direction encloses an angle of phi with the
             *  simulation y-axis (i.e. direction of sliding window), the positions vectors are
             *  rotated around the simulation x-axis before calling the TWTS field functions.
             *  Note: The TWTS field functions are in non-rotated frame and only use the angle
             *  phi to determine the required amount of pulse front tilt.
             *  RotationMatrix[PI/2+phi].(y,z) (180Deg-flip at phi=90Deg since coordinate
             *  system in paper is oriented the other way round.) */
            
            /*  Rotate 90 degree around y-axis, so that TWTS laser propagates within
             *  the 2D (x,y)-plane. Corresponding position vector for the Ez-components
             *  in 2D simulations.
             *  3D     3D vectors in 2D space (x,y)
             *  x -->  z
             *  y -->  y
             *  z --> -x (Since z=0 for 2D, we use the existing
             *            TWTS-field-function and set -x=0)
             *  Ex --> Ez (--> Same function values can be used in 2D,
             *             but with Yee-Cell-Positions for Ez.)
             *  By --> By
             *  Bz --> -Bx
             *
             * Explicit implementation in 3D coordinates:
             * fieldPosVector = float3_64( -fieldPosVector.z(),       //(Here: ==0)
             *                              fieldPosVector.y(),
             *                              fieldPosVector.x() );
             * fieldPosVector = float3_64( fieldPosVector.x(),
             *       -sin(phi)*fieldPosVector.y()-cos(phi)*fieldPosVector.z(),
             *       +cos(phi)*fieldPosVector.y()-sin(phi)*fieldPosVector.z()  );
             * The 2D implementation here only calculates the last two components.
             * Note: The x-axis of rotation is fine in 2D, because that component now contains
             *       the (non-existing) simulation z-coordinate. */
             return result(
                -pmMath::sin(AngleType(phi))*fieldPosVector.y()
                    -pmMath::cos(AngleType(phi))*fieldPosVector.x() ,
                +pmMath::cos(AngleType(phi))*fieldPosVector.y()
                    -pmMath::sin(AngleType(phi))*fieldPosVector.x() );
        }           
    };
    
    template <typename T_Type,typename T_AngleType>
    HDINLINE typename RotateField<T_Type,T_AngleType>::result
    rotateField( const T_Type& fieldPosVector,
                 const T_AngleType phi ) const
    {
        return RotateField<T_Type,T_AngleType>()(fieldPosVector,phi);
    }
    
    template <unsigned T_dim>
    class get_tdelay_SI
    {
        public:
        /** Obtain the SI time delay that later enters the Ex(r, t), By(r, t) and Bz(r, t)
         *  calculations as t.
         * \tparam T_dim Specializes for the simulation dimension
         *  \param auto_tdelay calculate the time delay such that the TWTS pulse is not
         *                     inside the simulation volume at simulation start
         *                     timestep = 0 [default = true]
         *  \param tdelay_user_SI manual time delay if auto_tdelay is false
         *  \param halfSimSize center of simulation volume in number of cells
         *  \param pulselength_SI sigma of std. gauss for intensity (E^2)
         *  \param focus_y_SI the distance to the laser focus in y-direction [m]
         *  \param phi interaction angle between TWTS laser propagation vector and
         *             the y-axis [rad, default = 90.*(PI/180.)]
         *  \param beta_0 propagation speed of overlap normalized
         *                to the speed of light [c, default = 1.0]
         *  \return time delay in SI units */
        HDINLINE float_64 operator()( const bool auto_tdelay,
                                      const float_64 tdelay_user_SI,
                                      const DataSpace<simDim>& halfSimSize,
                                      const float_64 pulselength_SI,
                                      const float_64 focus_y_SI,
                                      const float_X phi,
                                      const float_X beta_0 ) const;
    };
    
    template<>
    HDINLINE float_64
    get_tdelay_SI<DIM3>::operator()( const bool auto_tdelay,
                                     const float_64 tdelay_user_SI,
                                     const DataSpace<simDim>& halfSimSize,
                                     const float_64 pulselength_SI,
                                     const float_64 focus_y_SI,
                                     const float_X phi,
                                     const float_X beta_0 ) const
    {
        if ( auto_tdelay ) {
            
            /* angle between the laser pulse front and the y-axis. Good approximation for
             * beta0\simeq 1. For exact relation look in TWTS core routines for Ex, By or Bz. */
            const float_64 eta = PI/2 - (phi/2);
            /* halfSimSize[2] --> Half-depth of simulation volume (in z); By geometric
             * projection we calculate the y-distance walkoff of the TWTS-pulse.
             * The abs()-function is for correct offset for -phi<-90Deg and +phi>+90Deg. */
            const float_64 y1 = float_64(halfSimSize[2]
                                *picongpu::SI::CELL_DEPTH_SI)*abs(cos(eta));
            /* Fudge parameter to make sure, that TWTS pulse starts to impact simulation volume
             * at low intensity values. */
            const float_64 m = 3.;
            /* Approximate cross section of laser pulse through y-axis,
             * scaled with "fudge factor" m. */
            const float_64 y2 = m*(pulselength_SI*picongpu::SI::SPEED_OF_LIGHT_SI)/cos(eta);
            /* y-position of laser coordinate system origin within simulation. */
            const float_64 y3 = focus_y_SI;
            /* Programmatically obtained time-delay */
            const float_64 tdelay = (y1+y2+y3)/(picongpu::SI::SPEED_OF_LIGHT_SI*beta_0);
            
            return tdelay;
        }
        else
            return tdelay_user_SI;
    }
    
    template <>
    HDINLINE float_64
    get_tdelay_SI<DIM2>::operator()( const bool auto_tdelay,
                                     const float_64 tdelay_user_SI,
                                     const DataSpace<simDim>& halfSimSize,
                                     const float_64 pulselength_SI,
                                     const float_64 focus_y_SI,
                                     const float_X phi,
                                     const float_X beta_0 ) const
    {
        if ( auto_tdelay ) {
            
            /* angle between the laser pulse front and the y-axis. Good approximation for
             * beta0\simeq 1. For exact relation look in TWTS core routines for Ex, By or Bz. */
            const float_64 eta = PI/2 - (phi/2);
            /* halfSimSize[0] --> Half-depth of simulation volume (in x); By geometric
             * projection we calculate the y-distance walkoff of the TWTS-pulse.
             * The abs()-function is for correct offset for -phi<-90Deg and +phi>+90Deg. */
            const float_64 y1 = float_64(halfSimSize[0]
                                *picongpu::SI::CELL_WIDTH_SI)*abs(cos(eta));
            /* Fudge parameter to make sure, that TWTS pulse starts to impact simulation volume
             * at low intensity values. */
            const float_64 m = 3.;
            /* Approximate cross section of laser pulse through y-axis,
             * scaled with "fudge factor" m. */
            const float_64 y2 = m*(pulselength_SI*picongpu::SI::SPEED_OF_LIGHT_SI)/cos(eta);
            /* y-position of laser coordinate system origin within simulation. */
            const float_64 y3 = focus_y_SI;
            /* Programmatically obtained time-delay */
            const float_64 tdelay = (y1+y2+y3)/(picongpu::SI::SPEED_OF_LIGHT_SI*beta_0);
            
            return tdelay;
        }
        else
            return tdelay_user_SI;
    }
    
    template <unsigned T_dim>
    class getFieldPositions_SI
    {
        public:
        /** Obtains the position in SIat which the TWTS field 
         *  \param auto_tdelay calculate the time delay such that the TWTS pulse is not
         *                     inside the simulation volume at simulation start
         *                     timestep = 0 [default = true]
         *  \param tdelay_user_SI manual time delay if auto_tdelay is false
         *  \param halfSimSize center of simulation volume in number of cells
         *  \param focus_y_SI the distance to the laser focus in y-direction [m]
         *  \param phi interaction angle between TWTS laser propagation vector and
         *             the y-axis [rad, default = 90.*(PI/180.)]
         *  \return field positions in SI units for cellIdx */
        HDINLINE PMacc::math::Vector<floatD_64,numComponents>
        operator()( const DataSpace<simDim>& cellIdx,
                    const DataSpace<simDim>& halfSimSize,
                    const float_64 focus_y_SI,
                    const PMacc::math::Vector<floatD_X, numComponents> fieldOnGridPositions,
                    const float_X phi ) const;
    };
    
    HDINLINE PMacc::math::Vector<floatD_64,numComponents>
    getFieldPositions_SI(const DataSpace<simDim>& cellIdx) const
    {
        /* Note: Neither direct precisionCast on picongpu::cellSize
           or casting on floatD_ does work. */
        const floatD_64 cellDim(picongpu::cellSize);
        const floatD_64 cellDimensions = cellDim * unit_length;
        
        /* TWTS laser coordinate origin is centered transversally and defined longitudinally by
           the laser center in y (usually maximum of intensity). */
        floatD_X laserOrigin = precisionCast<float_X>(halfSimSize);
        laserOrigin.y() = float_X( focus_y_SI/cellDimensions.y() );
        
        /* For the Yee-Cell shifted fields, obtain the fractional cell index components and add
         * that to the total cell indices. The physical field coordinate origin is transversally
         * centered with respect to the global simulation volume. */
        //PMacc::math::Vector<floatD_X, numComponents> fieldPositions = 
        //                fieldSolver::NumericalCellType::getEFieldPosition();
        PMacc::math::Vector<floatD_X, numComponents> fieldPositions = fieldOnGridPositions;
        
        PMacc::math::Vector<floatD_64,numComponents> fieldPositions_SI;
        
        for( uint32_t i = 0; i < numComponents; ++i ) /* cellIdx Ex, Ey and Ez */
        {
            fieldPositions[i]   += ( precisionCast<float_X>(cellIdx) - laserOrigin );
            fieldPositions_SI[i] = precisionCast<float_64>(fieldPositions[i]) * cellDimensions;

            fieldPositions_SI[i] = rotateField(fieldPositions_SI[i],phi);
        }
        
        return eFieldPositions_SI;
    }
    
    } /* namespace detail */
            
    HINLINE
    TWTSFieldE::TWTSFieldE( const float_64 focus_y_SI,
                            const float_64 wavelength_SI,
                            const float_64 pulselength_SI,
                            const float_64 w_x_SI,
                            const float_64 w_y_SI,
                            const float_X phi,
                            const float_X beta_0,
                            const float_64 tdelay_user_SI,
                            const bool auto_tdelay ) :
        focus_y_SI(focus_y_SI), wavelength_SI(wavelength_SI),
        pulselength_SI(pulselength_SI), w_x_SI(w_x_SI),
        w_y_SI(w_y_SI), phi(phi), beta_0(beta_0),
        tdelay_user_SI(tdelay_user_SI), dt(SI::DELTA_T_SI),
        unit_length(UNIT_LENGTH), auto_tdelay(auto_tdelay)
    {
        /* Note: These objects cannot be instantiated on CUDA GPU device. Since this is done
                 on host (see fieldBackground.param), this is no problem. */
        const SubGrid<simDim>& subGrid = Environment<simDim>::get().SubGrid();
        halfSimSize = subGrid.getGlobalDomain().size / 2;
        tdelay = detail::get_tdelay_SI<simDim>()(auto_tdelay, tdelay_user_SI, 
                                                 halfSimSize, pulselength_SI,
                                                 focus_y_SI, phi, beta_0) ;
    }
    
    HDINLINE PMacc::math::Vector<float3_64,detail::numComponents>
    TWTSFieldE::getEfieldPositions_SI(const DataSpace<simDim>& cellIdx) const
    {
        /* Note: Neither direct precisionCast on picongpu::cellSize
           or casting on floatD_ does work. */
        const floatD_64 cellDim(picongpu::cellSize);
        const floatD_64 cellDimensions = cellDim * unit_length;
        
        /* TWTS laser coordinate origin is centered transversally and defined longitudinally by
           the laser center in y (usually maximum of intensity). */
        floatD_X laserOrigin = precisionCast<float_X>(halfSimSize);
        laserOrigin.y() = float_X( focus_y_SI/cellDimensions.y() );
        
        /* For the Yee-Cell shifted fields, obtain the fractional cell index components and add
         * that to the total cell indices. The physical field coordinate origin is transversally
         * centered with respect to the global simulation volume. */
        PMacc::math::Vector<floatD_X, detail::numComponents> eFieldPositions = 
                        fieldSolver::NumericalCellType::getEFieldPosition();
        
        PMacc::math::Vector<floatD_64,detail::numComponents> eFieldPositions_SI;
        
        for( uint32_t i = 0; i < detail::numComponents; ++i ) /* cellIdx Ex, Ey and Ez */
        {
            eFieldPositions[i]   += ( precisionCast<float_X>(cellIdx) - laserOrigin );
            eFieldPositions_SI[i] = precisionCast<float_64>(eFieldPositions[i]) * cellDimensions;
            eFieldPositions_SI[i] = detail::rotateField<floatD_64>()(eFieldPositions_SI[i],phi);
        }
        
        return eFieldPositions_SI;
    }
    
    template<>
    HDINLINE float3_X
    TWTSFieldE::getTWTSEfield_Normalized<DIM3>(
                const PMacc::math::Vector<floatD_64,detail::numComponents>& eFieldPositions_SI,
                const float_64 time) const
    {
        float3_64 pos(0.0);
        for (uint32_t i = 0; i<simDim;++i) pos[i] = eFieldPositions_SI[0][i];
        return float3_X( float_X( calcTWTSEx(pos,time) ),
                         float_X(0.), float_X(0.) );
    }
    
    template<>
    HDINLINE float3_X
    TWTSFieldE::getTWTSEfield_Normalized<DIM2>(
        const PMacc::math::Vector<floatD_64,detail::numComponents>& eFieldPositions_SI,
        const float_64 time) const
    {
        /* Ex->Ez, so also the grid cell offset for Ez has to be used. */
        float3_64 pos(0.0);
        for (uint32_t i = 1; i<simDim;++i) pos[i] = eFieldPositions_SI[2][i];
        return float3_X( float_X(0.), float_X(0.),
                         float_X( calcTWTSEx(pos,time) ) );
    }
    
    HDINLINE float3_X
    TWTSFieldE::operator()( const DataSpace<simDim>& cellIdx,
                            const uint32_t currentStep ) const
    {
        const float_64 time_SI = float_64(currentStep) * dt - tdelay;
        const PMacc::math::Vector<float3_64,detail::numComponents> eFieldPositions_SI =
                                                        getEfieldPositions_SI(cellIdx);
        /* Single TWTS-Pulse */
        return getTWTSEfield_Normalized<simDim>(eFieldPositions_SI, time_SI);
    }

    /** Calculate the Ex(r,t) field here
     *
     * \param pos Spatial position of the target field.
     * \param time Absolute time (SI, including all offsets and transformations) for calculating
     *             the field */
    HDINLINE TWTSFieldE::float_T
    TWTSFieldE::calcTWTSEx( const float3_64& pos, const float_64 time) const
    {
        typedef PMacc::math::Complex<float_T> complex_T;
        /** Unit of Speed */
        const double UNIT_SPEED = SI::SPEED_OF_LIGHT_SI;
        /** Unit of time */
        const double UNIT_TIME = SI::DELTA_T_SI;
        /** Unit of length */
        const double UNIT_LENGTH = UNIT_TIME*UNIT_SPEED;
    
        /* propagation speed of overlap normalized to the speed of light [Default: beta0=1.0] */
        const float_T beta0 = float_T(beta_0);
        const float_T phiReal = float_T(phi);
        const float_T alphaTilt = atan2(float_T(1.0)-beta0*cos(phiReal),beta0*sin(phiReal));
        const float_T phiT = float_T(2.0)*alphaTilt;
        /* Definition of the laser pulse front tilt angle for the laser field below.
         * For beta0=1.0, this is equivalent to our standard definition. Question: Why is the
         * local "phi_T" not equal in value to the object member "phiReal" or "phi"?
         * Because the standard TWTS pulse is defined for beta0=1.0 and in the coordinate-system
         * of the TWTS model phi is responsible for pulse front tilt and dispersion only. Hence
         * the dispersion will (although physically correct) be slightly off the ideal TWTS
         * pulse for beta0!=1.0. This only shows that this TWTS pulse is primarily designed for
         * scenarios close to beta0=1. */
        
        /* Angle between the laser pulse front and the y-axis. Not used, but remains in code for
         * documentation purposes. */
        /* const float_T eta = PI/2 - (phiReal - alphaTilt); */
        
        const float_T cspeed = float_T(1.0);
        const float_T lambda0 = float_T(wavelength_SI/UNIT_LENGTH);
        const float_T om0 = float_T(2.0*PI*cspeed/lambda0*UNIT_TIME);
        /* factor 2  in tauG arises from definition convention in laser formula */
        const float_T tauG = float_T(pulselength_SI*2.0/UNIT_TIME);
        /* w0 is wx here --> w0 could be replaced by wx */
        const float_T w0 = float_T(w_x_SI/UNIT_LENGTH);
        const float_T rho0 = float_T(PI*w0*w0/lambda0/UNIT_LENGTH);
        /* wy is width of TWTS pulse */
        const float_T wy = float_T(w_y_SI/UNIT_LENGTH);
        const float_T k = float_T(2.0*PI/lambda0*UNIT_LENGTH);
        const float_T x = float_T(pos.x()/UNIT_LENGTH);
        const float_T y = float_T(pos.y()/UNIT_LENGTH);
        const float_T z = float_T(pos.z()/UNIT_LENGTH);
        const float_T t = float_T(time/UNIT_TIME);
        
        /* Calculating shortcuts for speeding up field calculation */
        const float_T sinPhi = sin(phiT);
        const float_T cosPhi = cos(phiT);
        const float_T sinPhi2 = sin(phiT/float_T(2.0));
        const float_T cosPhi2 = cos(phiT/float_T(2.0));
        const float_T tanPhi2 = tan(phiT/float_T(2.0));
        
        /* The "helpVar" variables decrease the nesting level of the evaluated expressions and
         * thus help with formal code verification through manual code inspection. */
        const complex_T helpVar1 = complex_T(0,1)*rho0 - y*cosPhi - z*sinPhi;
        const complex_T helpVar2 = complex_T(0,-1)*cspeed*om0*tauG*tauG
                                    - y*cosPhi/cosPhi2/cosPhi2*tanPhi2
                                    - float_T(2.0)*z*tanPhi2*tanPhi2;
        const complex_T helpVar3 = complex_T(0,1)*rho0 - y*cosPhi - z*sinPhi;

        const complex_T helpVar4 = (
            -(cspeed*cspeed*k*om0*tauG*tauG*wy*wy*x*x)
            - float_T(2.0)*cspeed*cspeed*om0*t*t*wy*wy*rho0 
            + complex_T(0,2)*cspeed*cspeed*om0*om0*t*tauG*tauG*wy*wy*rho0
            - float_T(2.0)*cspeed*cspeed*om0*tauG*tauG*y*y*rho0
            + float_T(4.0)*cspeed*om0*t*wy*wy*z*rho0
            - complex_T(0,2)*cspeed*om0*om0*tauG*tauG*wy*wy*z*rho0
            - float_T(2.0)*om0*wy*wy*z*z*rho0
            - complex_T(0,8)*om0*wy*wy*y*(cspeed*t - z)*z*sinPhi2*sinPhi2
            + complex_T(0,8)/sinPhi*(
                    +float_T(2.0)*z*z*(cspeed*om0*t*wy*wy+complex_T(0,1)*cspeed*y*y-om0*wy*wy*z)
                    + y*(
                        + cspeed*k*wy*wy*x*x
                        - complex_T(0,2)*cspeed*om0*t*wy*wy*rho0
                        + float_T(2.0)*cspeed*y*y*rho0
                        + complex_T(0,2)*om0*wy*wy*z*rho0
                    )*tan(float_T(PI/2.0)-phiT)/sinPhi
                )*sinPhi2*sinPhi2*sinPhi2*sinPhi2
            - complex_T(0,2)*cspeed*cspeed*om0*t*t*wy*wy*z*sinPhi
            - float_T(2.0)*cspeed*cspeed*om0*om0*t*tauG*tauG*wy*wy*z*sinPhi
            - complex_T(0,2)*cspeed*cspeed*om0*tauG*tauG*y*y*z*sinPhi
            + complex_T(0,4)*cspeed*om0*t*wy*wy*z*z*sinPhi
            + float_T(2.0)*cspeed*om0*om0*tauG*tauG*wy*wy*z*z*sinPhi
            - complex_T(0,2)*om0*wy*wy*z*z*z*sinPhi
            - float_T(4.0)*cspeed*om0*t*wy*wy*y*rho0*tanPhi2
            + float_T(4.0)*om0*wy*wy*y*z*rho0*tanPhi2
            + complex_T(0,2)*y*y*(
                 + cspeed*om0*t*wy*wy + complex_T(0,1)*cspeed*y*y - om0*wy*wy*z
                 )*cosPhi*cosPhi/cosPhi2/cosPhi2*tanPhi2
            + complex_T(0,2)*cspeed*k*wy*wy*x*x*z*tanPhi2*tanPhi2
            - float_T(2.0)*om0*wy*wy*y*y*rho0*tanPhi2*tanPhi2
            + float_T(4.0)*cspeed*om0*t*wy*wy*z*rho0*tanPhi2*tanPhi2
            + complex_T(0,4)*cspeed*y*y*z*rho0*tanPhi2*tanPhi2
            - float_T(4.0)*om0*wy*wy*z*z*rho0*tanPhi2*tanPhi2
            - complex_T(0,2)*om0*wy*wy*y*y*z*sinPhi*tanPhi2*tanPhi2
            - float_T(2.0)*y*cosPhi*(
                + om0*(
                    + cspeed*cspeed*(
                          complex_T(0,1)*t*t*wy*wy
                        + om0*t*tauG*tauG*wy*wy
                        + complex_T(0,1)*tauG*tauG*y*y
                        )
                    - cspeed*(complex_T(0,2)*t
                    + om0*tauG*tauG)*wy*wy*z
                    + complex_T(0,1)*wy*wy*z*z
                    )
                + complex_T(0,2)*om0*wy*wy*y*(cspeed*t - z)*tanPhi2
                + complex_T(0,1)*tanPhi2*tanPhi2*(
                      complex_T(0,-4)*cspeed*y*y*z
                    + om0*wy*wy*(y*y - float_T(4.0)*(cspeed*t - z)*z)
                )
            )
        )/(float_T(2.0)*cspeed*wy*wy*helpVar1*helpVar2);

        const complex_T helpVar5 = cspeed*om0*tauG*tauG 
            - complex_T(0,8)*y*tan( float_T(PI/2)-phiT )
                                /sinPhi/sinPhi*sinPhi2*sinPhi2*sinPhi2*sinPhi2
            - complex_T(0,2)*z*tanPhi2*tanPhi2;
        const complex_T result = (pmMath::exp(helpVar4)*tauG
            *pmMath::sqrt((cspeed*om0*rho0)/helpVar3))/pmMath::sqrt(helpVar5);
        return result.get_real();
    }

    /* Here comes the B-field part of the TWTS laser pulse. */
    
    HINLINE
    TWTSFieldB::TWTSFieldB( const float_64 focus_y_SI,
                            const float_64 wavelength_SI,
                            const float_64 pulselength_SI,
                            const float_64 w_x_SI,
                            const float_64 w_y_SI,
                            const float_X phi,
                            const float_X beta_0,
                            const float_64 tdelay_user_SI,
                            const bool auto_tdelay ) :
        focus_y_SI(focus_y_SI), wavelength_SI(wavelength_SI),
        pulselength_SI(pulselength_SI), w_x_SI(w_x_SI),
        w_y_SI(w_y_SI), phi(phi), beta_0(beta_0),
        tdelay_user_SI(tdelay_user_SI), dt(SI::DELTA_T_SI),
        unit_length(UNIT_LENGTH), auto_tdelay(auto_tdelay)
    {
        /* These objects cannot be instantiated on CUDA GPU device. Since this is done on host
         * (see fieldBackground.param), this is no problem. */
        const SubGrid<simDim>& subGrid = Environment<simDim>::get().SubGrid();
        halfSimSize = subGrid.getGlobalDomain().size / 2;
        tdelay = detail::get_tdelay_SI<simDim>()(auto_tdelay, tdelay_user_SI, 
                                                 halfSimSize, pulselength_SI,
                                                 focus_y_SI, phi, beta_0) ;
    }
        
    template<>
    HDINLINE PMacc::math::Vector<float3_64,detail::numComponents>
    TWTSFieldB::getBfieldPositions_SI<DIM3>(const DataSpace<simDim>& cellIdx) const
    {
        /* Note: Neither direct precisionCast on picongpu::cellSize
                 or casting on floatD_ does work. */
        const floatD_64 cellDim(picongpu::cellSize);
        const floatD_64 cellDimensions = cellDim * unit_length;
        
        /* TWTS laser coordinate origin is centered transversally and defined longitudinally
         * by the laser center (usually maximum of intensity) in y. */
        floatD_X laserOrigin = precisionCast<float_X>(halfSimSize);
        laserOrigin.y() = float_X( focus_y_SI/cellDimensions.y() );
        
        /* For the Yee-Cell shifted fields, obtain the fractional cell index components and
         * add that to the total cell indices. The physical field coordinate origin is
         * transversally centered with respect to the global simulation volume. */
        PMacc::math::Vector<floatD_X, detail::numComponents> bFieldPositions = 
                        fieldSolver::NumericalCellType::getBFieldPosition();
        PMacc::math::Vector<floatD_64,detail::numComponents> bFieldPositions_SI;
        
        for( uint32_t i = 0; i < detail::numComponents; ++i ) // cellIdx Ex, Ey and Ez
        {
            bFieldPositions[i]   += precisionCast<float_X>(cellIdx) - laserOrigin;
            bFieldPositions_SI[i] = precisionCast<float_64>(bFieldPositions[i]) *cellDimensions;
            
            /*  Since, the laser propagation direction encloses an angle of phi with the
             *  simulation y-axis (i.e. direction of sliding window), the positions vectors
             *  are rotated around the simulation x-axis before calling the TWTS field
             *  functions. Note: The TWTS field functions are in non-rotated frame and only
             *  use the angle phi to determine the required amount of pulse front tilt.
             *  RotationMatrix[PI/2+phi].(y,z) (180Deg-flip at phi=90Deg since coordinate
             *  system in paper is oriented the other way round.) */
            bFieldPositions_SI[i] = detail::rotateField<floatD_64>()(bFieldPositions_SI[i],phi);
            /*bFieldPositions_SI[i] = float3_64( bFieldPositions_SI[i].x(),
                     -sin(phi)*bFieldPositions_SI[i].y()-cos(phi)*bFieldPositions_SI[i].z(),
                     +cos(phi)*bFieldPositions_SI[i].y()-sin(phi)*bFieldPositions_SI[i].z() );*/
        }

        return bFieldPositions_SI;
    }
    
    template<>
    HDINLINE PMacc::math::Vector<float3_64,detail::numComponents>
    TWTSFieldB::getBfieldPositions_SI<DIM2>(const DataSpace<simDim>& cellIdx) const
    {
        const floatD_64 cellDim(picongpu::cellSize);
        const floatD_64 cellDimensions = cellDim * unit_length;
                                           
        /* TWTS laser coordinate origin is centered transversally and defined longitudinally by
         * the laser center in y (usually maximum of intensity). */
        floatD_X laserOrigin = precisionCast<float_X>(halfSimSize);
        laserOrigin.y() = float_X( focus_y_SI/cellDimensions.y() );
        
        /* For the Yee-Cell shifted fields, obtain the fractional cell index components and add
         * that to the total cell indices. The physical field coordinate origin is transversally
         * centered with respect to the global simulation volume. */
        PMacc::math::Vector<floatD_X, detail::numComponents> bFieldPositions = 
                        fieldSolver::NumericalCellType::getBFieldPosition();
        PMacc::math::Vector<floatD_64,detail::numComponents> bFieldPositions_SI;
        
        for( uint32_t i = 0; i < detail::numComponents; ++i ) /* cellIdx Ex, Ey and Ez */
        {
            bFieldPositions[i]   += precisionCast<float_X>(cellIdx) - laserOrigin;
            bFieldPositions_SI[i] = precisionCast<float_64>(bFieldPositions[i]) *cellDimensions;
            
            /*  Rotate 90 degree around y-axis, so that TWTS laser propagates within
             *  the 2D (x,y)-plane. Corresponding position vector for the Ez-components
             *  in 2D simulations.
             *  3D vectors in 2D space (x,y)
             *  x -->  z
             *  y -->  y
             *  z --> -x (Since z=0 for 2D, we use the existing
             *            TWTS-field-function and set -x=0)
             *  Ex --> Ez (--> Same function values can be used in 2D,
             *             but with Yee-Cell-Positions for Ez.)
             *  By --> By
             *  Bz --> -Bx
             */
            /*bFieldPositions_SI[i] = float3_64( -bFieldPositions_SI[i].z(),
                                                  bFieldPositions_SI[i].y(),
                                                  bFieldPositions_SI[i].x() );*/
            floatD_64 storeForSwap = bFieldPositions_SI[i];
            bFieldPositions_SI[i].x() = bFieldPositions_SI[i].y() ;
            bFieldPositions_SI[i].y() = storeForSwap.x() ;
            
            /*  Since, the laser propagation direction encloses an angle of phi with the
             *  simulation y-axis (i.e. direction of sliding window), the positions vectors are
             *  rotated around the simulation x-axis before calling the TWTS field functions.
             *  Note: The TWTS field functions are in non-rotated frame and only use the angle
             *  phi to determine the required amount of pulse front tilt.
             *  RotationMatrix[PI/2+phi].(y,z) (180Deg-flip at phi=90Deg since coordinate system
             *  in paper is oriented the other way round.) */
            
            /* Note: The x-axis of rotation is fine in 2D, because that component now contains
             *       the (non-existing) simulation z-coordinate. */
            bFieldPositions_SI[i] = detail::rotateField<floatD_64>()(bFieldPositions_SI[i],phi); 
            /*bFieldPositions_SI[i] = float3_64(*/
                 /* leave 2D z-component unchanged */
                 /* bFieldPositions_SI[i].x(),*/
                 /* rotates  2D y-component */
                 /*-sin(phi)*bFieldPositions_SI[i].y()-cos(phi)*bFieldPositions_SI[i].z(),*/
                 /* and      2D x-component */
                 /*+cos(phi)*bFieldPositions_SI[i].y()-sin(phi)*bFieldPositions_SI[i].z()  );*/
        }
        
        return bFieldPositions_SI;
    }
    
    template<>
    HDINLINE float3_X
    TWTSFieldB::getTWTSBfield_Normalized<DIM3>(
            const PMacc::math::Vector<floatD_64,detail::numComponents>& bFieldPositions_SI,
            const float_64 time) const
    {
        PMacc::math::Vector<float3_64,detail::numComponents> pos(0.0);
        for (uint32_t k = 0; k<detail::numComponents;++k) {
            for (uint32_t i = 0; i<simDim;++i) pos[k][i] = bFieldPositions_SI[k][i];
        }
        
        /* Calculate By-component with the Yee-Cell offset of a By-field */
        const float_64 By_By = calcTWTSBy(pos[1], time);
        /* Calculate Bz-component the Yee-Cell offset of a Bz-field */
        const float_64 Bz_By = calcTWTSBz(pos[1], time);
        const float_64 By_Bz = calcTWTSBy(pos[2], time);
        const float_64 Bz_Bz = calcTWTSBz(pos[2], time);
        /* Since we rotated all position vectors before calling calcTWTSBy and calcTWTSBz,
         * we need to back-rotate the resulting B-field vector. */
        /* RotationMatrix[-(PI/2+phi)].(By,Bz) for rotating back the Field-Vektors. */
        const float_64 By_rot = -sin(+phi)*By_By+cos(+phi)*Bz_By;
        const float_64 Bz_rot = -cos(+phi)*By_Bz-sin(+phi)*Bz_Bz;
        
        /* Finally, the B-field normalized to the peak amplitude. */
        return float3_X( float_X(0.0),
                         float_X(By_rot),
                         float_X(Bz_rot) );
    }
    
    template<>
    HDINLINE float3_X
    TWTSFieldB::getTWTSBfield_Normalized<DIM2>(
            const PMacc::math::Vector<floatD_64,detail::numComponents>& bFieldPositions_SI,
            const float_64 time) const
    {
        PMacc::math::Vector<float3_64,detail::numComponents> pos(0.0);
        for (uint32_t k = 0; k<detail::numComponents;++k) {
            for (uint32_t i = 1; i<simDim;++i) pos[k][i] = bFieldPositions_SI[k][i];
        }
        /*  Corresponding position vector for the Field-components in 2D simulations.
         *  3D     3D vectors in 2D space (x, y)
         *  x -->  z (Meaning: In 2D-sim, insert cell-coordinate x
         *            into TWTS field function coordinate z.)
         *  y -->  y
         *  z --> -x (Since z=0 for 2D, we use the existing
         *            3D TWTS-field-function and set x=-0)
         *  Ex --> Ez (Meaning: Calculate Ex-component of existing 3D TWTS-field to obtain
         *             corresponding Ez-component in 2D.
         *             Note: the position offset due to the Yee-Cell for Ez.)
         *  By --> By
         *  Bz --> -Bx (Yes, the sign is necessary.)
         */ 
        /* Analogous to 3D case, but replace By->By and Bz->-Bx. Hence the grid cell offset
         * for Bx has to be used instead of Bz. Mind the -sign. */
         
        /* Calculate By-component with the Yee-Cell offset of a By-field */
        const float_64 By_By =  calcTWTSBy(pos[1], time);
        /* Calculate Bx-component with the Yee-Cell offset of a By-field */
        const float_64 Bx_By = -calcTWTSBz(pos[1], time);
        const float_64 By_Bx =  calcTWTSBy(pos[0], time);
        const float_64 Bx_Bx = -calcTWTSBz(pos[0], time);
        /* Since we rotated all position vectors before calling calcTWTSBy and calcTWTSBz, we
         * need to back-rotate the resulting B-field vector. Now the rotation is done
         * analogously in the (y,x)-plane. (Reverse of the position vector transformation.) */
        /* RotationMatrix[-(PI/2+phi)].(By,Bx) */
        const float_64 By_rot = -sin(phi)*By_By+cos(phi)*Bx_By;
        /* for rotating back the Field-Vektors.*/
        const float_64 Bx_rot = -cos(phi)*By_Bx-sin(phi)*Bx_Bx;
        
        /* Finally, the B-field normalized to the peak amplitude. */
        return float3_X( float_X(0.0),
                         float_X(By_rot),
                         float_X(Bx_rot) );
    }
    
    HDINLINE float3_X
    TWTSFieldB::operator()( const DataSpace<simDim>& cellIdx,
                            const uint32_t currentStep ) const
    {
        const float_64 time_SI = float_64(currentStep) * dt - tdelay;
        const PMacc::math::Vector<float3_64,detail::numComponents> bFieldPositions_SI =
                                                        getBfieldPositions_SI<simDim>(cellIdx);
        /* Single TWTS-Pulse */
        return getTWTSBfield_Normalized<simDim>(bFieldPositions_SI, time_SI);
    }

    /** Calculate the By(r,t) field here
     *
     * \param pos Spatial position of the target field.
     * \param time Absolute time (SI, including all offsets and transformations)
     *             for calculating the field */
    HDINLINE TWTSFieldB::float_T
    TWTSFieldB::calcTWTSBy( const float3_64& pos, const float_64 time ) const
    {
        typedef PMacc::math::Complex<float_T> complex_T;
        /** Unit of Speed */
        const double UNIT_SPEED = SI::SPEED_OF_LIGHT_SI;
        /** Unit of time */
        const double UNIT_TIME = SI::DELTA_T_SI;
        /** Unit of length */
        const double UNIT_LENGTH = UNIT_TIME*UNIT_SPEED;
        
        /* propagation speed of overlap normalized to the speed of light [Default: beta0=1.0] */
        const float_T beta0 = float_T(beta_0);
        const float_T phiReal = float_T(phi);
        const float_T alphaTilt = atan2(float_T(1.0)-beta0*cos(phiReal),beta0*sin(phiReal));
        const float_T phiT = float_T(2.0)*alphaTilt;
        /* Definition of the laser pulse front tilt angle for the laser field below.
         * For beta0=1.0, this is equivalent to our standard definition. Question: Why is the
         * local "phi_T" not equal in value to the object member "phiReal" or "phi"?
         * Because the standard TWTS pulse is defined for beta0=1.0 and in the coordinate-system
         * of the TWTS model phi is responsible for pulse front tilt and dispersion only. Hence
         * the dispersion will (although physically correct) be slightly off the ideal TWTS
         * pulse for beta0!=1.0. This only shows that this TWTS pulse is primarily designed for
         * scenarios close to beta0=1. */
        
        /* Angle between the laser pulse front and the y-axis. Not used, but remains in code for
         * documentation purposes. */
        /* const float_T eta = float_T(PI/2) - (phiReal - alphaTilt); */
        
        const float_T cspeed = float_T(1.0);
        const float_T lambda0 = float_T(wavelength_SI/UNIT_LENGTH);
        const float_T om0 = float_T(2.0*PI*cspeed/lambda0*UNIT_TIME);
        /* factor 2  in tauG arises from definition convention in laser formula */
        const float_T tauG = float_T(pulselength_SI*2.0/UNIT_TIME);
        /* w0 is wx here --> w0 could be replaced by wx */
        const float_T w0 = float_T(w_x_SI/UNIT_LENGTH);
        const float_T rho0 = float_T(PI*w0*w0/lambda0/UNIT_LENGTH);
        /* wy is width of TWTS pulse */
        const float_T wy = float_T(w_y_SI/UNIT_LENGTH);
        const float_T k = float_T(2.0*PI/lambda0*UNIT_LENGTH);
        const float_T x = float_T(pos.x()/UNIT_LENGTH);
        const float_T y = float_T(pos.y()/UNIT_LENGTH);
        const float_T z = float_T(pos.z()/UNIT_LENGTH);
        const float_T t = float_T(time/UNIT_TIME);
                        
        /* Shortcuts for speeding up the field calculation. */
        const float_T sinPhi = sin(phiT);
        const float_T cosPhi = cos(phiT);
        const float_T cosPhi2 = cos(phiT/2.0);
        const float_T tanPhi2 = tan(phiT/2.0);
        
        /* The "helpVar" variables decrease the nesting level of the evaluated expressions and
         * thus help with formal code verification through manual code inspection. */
        const complex_T helpVar1 = rho0 + complex_T(0,1)*y*cosPhi + complex_T(0,1)*z*sinPhi;
        const complex_T helpVar2 = cspeed*om0*tauG*tauG + complex_T(0,2)
                                    *(-z - y*tan(float_T(PI/2)-phiT))*tanPhi2*tanPhi2;
        const complex_T helpVar3 = complex_T(0,1)*rho0 - y*cosPhi - z*sinPhi;
        
        const complex_T helpVar4 = float_T(-1.0)*(
            cspeed*cspeed*k*om0*tauG*tauG*wy*wy*x*x
            + float_T(2.0)*cspeed*cspeed*om0*t*t*wy*wy*rho0
            - complex_T(0,2)*cspeed*cspeed*om0*om0*t*tauG*tauG*wy*wy*rho0
            + float_T(2.0)*cspeed*cspeed*om0*tauG*tauG*y*y*rho0
            - float_T(4.0)*cspeed*om0*t*wy*wy*z*rho0
            + complex_T(0,2)*cspeed*om0*om0*tauG*tauG*wy*wy*z*rho0
            + float_T(2.0)*om0*wy*wy*z*z*rho0
            + float_T(4.0)*cspeed*om0*t*wy*wy*y*rho0*tanPhi2
            - float_T(4.0)*om0*wy*wy*y*z*rho0*tanPhi2
            - complex_T(0,2)*cspeed*k*wy*wy*x*x*z*tanPhi2*tanPhi2
            + float_T(2.0)*om0*wy*wy*y*y*rho0*tanPhi2*tanPhi2
            - float_T(4.0)*cspeed*om0*t*wy*wy*z*rho0*tanPhi2*tanPhi2
            - complex_T(0,4)*cspeed*y*y*z*rho0*tanPhi2*tanPhi2
            + float_T(4.0)*om0*wy*wy*z*z*rho0*tanPhi2*tanPhi2
            - complex_T(0,2)*cspeed*k*wy*wy*x*x*y*tan(float_T(PI/2)-phiT)*tanPhi2*tanPhi2
            - float_T(4.0)*cspeed*om0*t*wy*wy*y*rho0*tan(float_T(PI/2)-phiT)*tanPhi2*tanPhi2
            - complex_T(0,4)*cspeed*y*y*y*rho0*tan(float_T(PI/2)-phiT)*tanPhi2*tanPhi2
            + float_T(4.0)*om0*wy*wy*y*z*rho0*tan(float_T(PI/2)-phiT)*tanPhi2*tanPhi2
            + float_T(2.0)*z*sinPhi*(
                + om0*(
                    + cspeed*cspeed*(
                          complex_T(0,1)*t*t*wy*wy
                        + om0*t*tauG*tauG*wy*wy
                        + complex_T(0,1)*tauG*tauG*y*y
                    )
                    - cspeed*(complex_T(0,2)*t + om0*tauG*tauG)*wy*wy*z
                    + complex_T(0,1)*wy*wy*z*z
                    )
                + complex_T(0,2)*om0*wy*wy*y*(cspeed*t - z)*tanPhi2
                + complex_T(0,1)*tanPhi2*tanPhi2*(
                      complex_T(0,-2)*cspeed*y*y*z
                    + om0*wy*wy*( y*y - float_T(2.0)*(cspeed*t - z)*z )
                )
            )
            + float_T(2.0)*y*cosPhi*(
                + om0*(
                    + cspeed*cspeed*(
                          complex_T(0,1)*t*t*wy*wy
                        + om0*t*tauG*tauG*wy*wy
                        + complex_T(0,1)*tauG*tauG*y*y
                    )
                - cspeed*(complex_T(0,2)*t + om0*tauG*tauG)*wy*wy*z
                + complex_T(0,1)*wy*wy*z*z
                )
            + complex_T(0,2)*om0*wy*wy*y*(cspeed*t - z)*tanPhi2
            + complex_T(0,1)*(
                  complex_T(0,-4)*cspeed*y*y*z
                + om0*wy*wy*(y*y - float_T(4.0)*(cspeed*t - z)*z)
                - float_T(2.0)*y*(
                    + cspeed*om0*t*wy*wy
                    + complex_T(0,1)*cspeed*y*y
                    - om0*wy*wy*z
                    )*tan(float_T(PI/2)-phiT)
                )*tanPhi2*tanPhi2
            )
        )/(float_T(2.0)*cspeed*wy*wy*helpVar1*helpVar2);

        const complex_T helpVar5 = complex_T(0,-1)*cspeed*om0*tauG*tauG
                                + (-z - y*tan(float_T(PI/2)-phiT))*tanPhi2*tanPhi2*float_T(2.0);
        const complex_T helpVar6 = (cspeed*(cspeed*om0*tauG*tauG + complex_T(0,2)
                                *(-z - y*tan(float_T(PI/2)-phiT))*tanPhi2*tanPhi2))/(om0*rho0);
        const complex_T result = (pmMath::exp(helpVar4)*tauG/cosPhi2/cosPhi2
            *(rho0 + complex_T(0,1)*y*cosPhi + complex_T(0,1)*z*sinPhi)
            *(
                  complex_T(0,2)*cspeed*t + cspeed*om0*tauG*tauG - complex_T(0,4)*z
                + cspeed*(complex_T(0,2)*t + om0*tauG*tauG)*cosPhi
                + complex_T(0,2)*y*tanPhi2
            )*pmMath::pow(helpVar3,float_T(-1.5))
        )/(float_T(2.0)*helpVar5*pmMath::sqrt(helpVar6));

        return result.get_real();
    }
    
    /** Calculate the Bz(r,t) field
     *
     * \param pos Spatial position of the target field.
     * \param time Absolute time (SI, including all offsets and transformations)
     *             for calculating the field */
    HDINLINE TWTSFieldB::float_T
    TWTSFieldB::calcTWTSBz( const float3_64& pos, const float_64 time ) const
    {
        typedef PMacc::math::Complex<float_T> complex_T;
        /** Unit of Speed */
        const double UNIT_SPEED = SI::SPEED_OF_LIGHT_SI;
        /** Unit of time */
        const double UNIT_TIME = SI::DELTA_T_SI;
        /** Unit of length */
        const double UNIT_LENGTH = UNIT_TIME*UNIT_SPEED;
        
        /* propagation speed of overlap normalized to the speed of light [Default: beta0=1.0] */
        const float_T beta0 = float_T(beta_0);
        const float_T phiReal = float_T(phi);
        const float_T alphaTilt = atan2(float_T(1.0)-beta0*cos(phiReal),beta0*sin(phiReal));
        const float_T phiT = float_T(2.0)*alphaTilt;
        /* Definition of the laser pulse front tilt angle for the laser field below.
         * For beta0=1.0, this is equivalent to our standard definition. Question: Why is the
         * local "phi_T" not equal in value to the object member "phiReal" or "phi"?
         * Because the standard TWTS pulse is defined for beta0=1.0 and in the coordinate-system
         * of the TWTS model phi is responsible for pulse front tilt and dispersion only. Hence
         * the dispersion will (although physically correct) be slightly off the ideal TWTS
         * pulse for beta0!=1.0. This only shows that this TWTS pulse is primarily designed for
         * scenarios close to beta0=1. */
        
        /* Angle between the laser pulse front and the y-axis. Not used, but remains in code for
         * documentation purposes. */
        /* const float_T eta = float_T(float_T(PI/2)) - (phiReal - alphaTilt); */
        
        const float_T cspeed = float_T(1.0);
        const float_T lambda0 = float_T(wavelength_SI/UNIT_LENGTH);
        const float_T om0 = float_T(2.0*PI*cspeed/lambda0*UNIT_TIME);
        /* factor 2  in tauG arises from definition convention in laser formula */
        const float_T tauG = float_T(pulselength_SI*2.0/UNIT_TIME);
        /* w0 is wx here --> w0 could be replaced by wx */
        const float_T w0 = float_T(w_x_SI/UNIT_LENGTH);
        const float_T rho0 = float_T(PI*w0*w0/lambda0/UNIT_LENGTH);
        /* wy is width of TWTS pulse */
        const float_T wy = float_T(w_y_SI/UNIT_LENGTH);
        const float_T k = float_T(2.0*PI/lambda0*UNIT_LENGTH);
        const float_T x = float_T(pos.x()/UNIT_LENGTH);
        const float_T y = float_T(pos.y()/UNIT_LENGTH);
        const float_T z = float_T(pos.z()/UNIT_LENGTH);
        const float_T t = float_T(time/UNIT_TIME);
                        
        /* Shortcuts for speeding up the field calculation. */
        const float_T sinPhi = sin(phiT);
        const float_T cosPhi = cos(phiT);
        const float_T sinPhi2 = sin(phiT/float_T(2.0));
        const float_T cosPhi2 = cos(phiT/float_T(2.0));
        const float_T tanPhi2 = tan(phiT/float_T(2.0));
        
        /* The "helpVar" variables decrease the nesting level of the evaluated expressions and
         * thus help with formal code verification through manual code inspection. */
        const complex_T helpVar1 = -(cspeed*z) - cspeed*y*tan(float_T(PI/2)-phiT)
                                    + complex_T(0,1)*cspeed*rho0/sinPhi;
        const complex_T helpVar2 = complex_T(0,1)*rho0 - y*cosPhi - z*sinPhi;
        const complex_T helpVar3 = helpVar2*cspeed;
        const complex_T helpVar4 = cspeed*om0*tauG*tauG
                                    - complex_T(0,1)*y*cosPhi/cosPhi2/cosPhi2*tanPhi2
                                    - complex_T(0,2)*z*tanPhi2*tanPhi2;
        const complex_T helpVar5 = float_T(2.0)*cspeed*t - complex_T(0,1)*cspeed*om0*tauG*tauG
                            - float_T(2.0)*z + float_T(8.0)*y/sinPhi/sinPhi/sinPhi
                                *sinPhi2*sinPhi2*sinPhi2*sinPhi2
                            - float_T(2.0)*z*tanPhi2*tanPhi2;

        const complex_T helpVar6 = (
        (om0*y*rho0/cosPhi2/cosPhi2/cosPhi2/cosPhi2)/helpVar1 
        - (complex_T(0,2)*k*x*x)/helpVar2 
        - (complex_T(0,1)*om0*om0*tauG*tauG*rho0)/helpVar2
        - (complex_T(0,4)*y*y*rho0)/(wy*wy*helpVar2)
        + (om0*om0*tauG*tauG*y*cosPhi)/helpVar2
        + (float_T(4.0)*y*y*y*cosPhi)/(wy*wy*helpVar2)
        + (om0*om0*tauG*tauG*z*sinPhi)/helpVar2
        + (float_T(4.0)*y*y*z*sinPhi)/(wy*wy*helpVar2)
        + (complex_T(0,2)*om0*y*y*cosPhi/cosPhi2/cosPhi2*tanPhi2)/helpVar3
        + (om0*y*rho0*cosPhi/cosPhi2/cosPhi2*tanPhi2)/helpVar3
        + (complex_T(0,1)*om0*y*y*cosPhi*cosPhi/cosPhi2/cosPhi2*tanPhi2)/helpVar3
        + (complex_T(0,4)*om0*y*z*tanPhi2*tanPhi2)/helpVar3
        - (float_T(2.0)*om0*z*rho0*tanPhi2*tanPhi2)/helpVar3
        - (complex_T(0,2)*om0*z*z*sinPhi*tanPhi2*tanPhi2)/helpVar3
        - (om0*helpVar5*helpVar5)/(cspeed*helpVar4)
        )/float_T(4.0);
                
        const complex_T helpVar7 = cspeed*om0*tauG*tauG
                                    - complex_T(0,1)*y*cosPhi/cosPhi2/cosPhi2*tanPhi2
                                    - complex_T(0,2)*z*tanPhi2*tanPhi2;
        const complex_T result = ( complex_T(0,2)*pmMath::exp(helpVar6)*tauG*tanPhi2
                                    *(cspeed*t - z + y*tanPhi2)
                                    *pmMath::sqrt( (om0*rho0)/helpVar3 )
                                  )/pmMath::pow(helpVar7,float_T(1.5));

        return result.get_real();
    }
    
} /* namespace templates */
} /* namespace picongpu */
