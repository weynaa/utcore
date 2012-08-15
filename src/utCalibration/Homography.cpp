/*
 * Ubitrack - Library for Ubiquitous Tracking
 * Copyright 2006, Technische Universitaet Muenchen, and individual
 * contributors as indicated by the @authors tag. See the 
 * copyright.txt in the distribution for a full listing of individual
 * contributors.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA, or see the FSF site: http://www.fsf.org.
 */

/**
 * @ingroup tracking_algorithms
 * @file
 * Implements functions for homography estimation.
 *
 * @author Daniel Pustka <daniel.pustka@in.tum.de>
 */ 

#include "Homography.h"

#ifdef HAVE_LAPACK
#include <boost/numeric/bindings/lapack/gesvd.hpp>

// shortcuts to namespaces
namespace ublas = boost::numeric::ublas;
namespace lapack = boost::numeric::bindings::lapack;

namespace Ubitrack { namespace Calibration {

/** \internal */
template< typename T > Math::Matrix< 3, 3, T > homographyDLTImpl( const std::vector< Math::Vector< 2, T > >& fromPoints, 
	const std::vector< Math::Vector< 2, T > >& toPoints )
{
	assert( fromPoints.size() == toPoints.size() );
	assert( fromPoints.size() >= 4 );

	// normalize input points
	Math::Vector< 2, T > fromShift;
	Math::Vector< 2, T > fromScale;
	Math::Matrix< 3, 3, T > fromCorrect;
	dltNormalizeVectors< 2, T >( fromShift, fromScale, fromCorrect, false, fromPoints );

	Math::Vector< 2, T > toShift;
	Math::Vector< 2, T > toScale;
	Math::Matrix< 3, 3, T > toCorrect;
	dltNormalizeVectors< 2, T >( toShift, toScale, toCorrect, true, toPoints );

	// construct equation system
	ublas::matrix< T, ublas::column_major > A( 2 * fromPoints.size(), 9 );
	for ( unsigned i = 0; i < fromPoints.size(); i++ )
	{
		Math::Vector< 2, T > to = ublas::element_div( toPoints[ i ] - toShift, toScale );
		Math::Vector< 2, T > from = ublas::element_div( fromPoints[ i ] - fromShift, fromScale );

		A( i * 2, 0 ) = A( i * 2, 1 ) = A( i * 2, 2 ) = 0.0f;
		A( i * 2, 3 ) = -from( 0 ); 
		A( i * 2, 4 ) = -from( 1 ); 
		A( i * 2, 5 ) = -1.0f;
		A( i * 2, 6 ) = to( 1 ) * from( 0 ); 
		A( i * 2, 7 ) = to( 1 ) * from( 1 );
		A( i * 2, 8 ) = to( 1 );
		A( i * 2 + 1, 0 ) = from( 0 ); 
		A( i * 2 + 1, 1 ) = from( 1 ); 
		A( i * 2 + 1, 2 ) = 1.0f;
		A( i * 2 + 1, 3 ) = A( i * 2 + 1, 4 ) = A( i * 2 + 1, 5 ) = 0.0f;
		A( i * 2 + 1, 6 ) = -to( 0 ) * from( 0 ); 
		A( i * 2 + 1, 7 ) = -to( 0 ) * from( 1 );
		A( i * 2 + 1, 8 ) = -to( 0 );
	}

	// solve using SVD
	unsigned nSingularValues = std::min( A.size1(), A.size2() );
	ublas::vector< T > s( nSingularValues );
	Math::Matrix< 9, 9, T > Vt;
	ublas::matrix< T, ublas::column_major > U( 2 * fromPoints.size(), 2 * fromPoints.size() );
	lapack::gesvd( 'N', 'A', A, s, U, Vt );

	// copy result to 3x3 matrix
	Math::Matrix< 3, 3, T > H;
	H( 0, 0 ) = Vt( 8, 0 ); H( 0, 1 ) = Vt( 8, 1 ); H( 0, 2 ) = Vt( 8, 2 );
	H( 1, 0 ) = Vt( 8, 3 ); H( 1, 1 ) = Vt( 8, 4 ); H( 1, 2 ) = Vt( 8, 5 );
	H( 2, 0 ) = Vt( 8, 6 ); H( 2, 1 ) = Vt( 8, 7 ); H( 2, 2 ) = Vt( 8, 8 );
	
	// reverse normalization
	Math::Matrix< 3, 3, T > Htemp( ublas::prod( toCorrect, H ) );
	ublas::noalias( H ) = ublas::prod( Htemp, fromCorrect );
	
	return H;
}


Math::Matrix< 3, 3, float > homographyDLT( const std::vector< Math::Vector< 2, float > >& fromPoints, 
	const std::vector< Math::Vector< 2, float > >& toPoints )
{
	return homographyDLTImpl( fromPoints, toPoints );
}

Math::Matrix< 3, 3, double > homographyDLT( const std::vector< Math::Vector< 2, double > >& fromPoints, 
	const std::vector< Math::Vector< 2, double > >& toPoints )
{
	return homographyDLTImpl( fromPoints, toPoints );
}


/** \internal */
template< typename T > Math::Matrix< 3, 3, T > squareHomographyImpl( const std::vector< Math::Vector< 2, T > >& corners )
{
	// homography computation � la Harker & O'Leary, simplified for squares
	assert( corners.size() == 4 );
	
	// subtract mean from points
	Math::Vector< 2, T > c[ 4 ];
	Math::Vector< 2, T > mean( ublas::zero_vector< T >( 2 ) );
	for ( int i = 0; i < 4; i++ )
		mean = mean + corners[ i ];
	mean /= 4;
	for ( int i = 0; i < 4; i++ )
		c[ i ] = corners[ i ] - mean;

	// build simplified matrix A
	Math::Matrix< 4, 3, T > matA;
	matA( 0, 0 ) =   c[ 0 ][ 0 ] - c[ 1 ][ 0 ] - c[ 2 ][ 0 ] + c[ 3 ][ 0 ];
	matA( 0, 1 ) = - c[ 0 ][ 0 ] - c[ 1 ][ 0 ] + c[ 2 ][ 0 ] + c[ 3 ][ 0 ];
	matA( 0, 2 ) =  -2 * ( c[ 0 ][ 0 ] + c[ 2 ][ 0 ] );
	matA( 1, 0 ) = - matA( 0, 0 );
	matA( 1, 1 ) = - matA( 0, 1 );
	matA( 1, 2 ) =  -2 * ( c[ 1 ][ 0 ] + c[ 3 ][ 0 ] );

	matA( 2, 0 ) =   c[ 0 ][ 1 ] - c[ 1 ][ 1 ] - c[ 2 ][ 1 ] + c[ 3 ][ 1 ];
	matA( 2, 1 ) = - c[ 0 ][ 1 ] - c[ 1 ][ 1 ] + c[ 2 ][ 1 ] + c[ 3 ][ 1 ];
	matA( 2, 2 ) =  -2 * ( c[ 0 ][ 1 ] + c[ 2 ][ 1 ] );
	matA( 3, 0 ) = - matA( 2, 0 );
	matA( 3, 1 ) = - matA( 2, 1 );
	matA( 3, 2 ) =  -2 * ( c[ 1 ][ 1 ] + c[ 3 ][ 1 ] );

	// compute SVD
	Math::Vector< 3, T > s;
	Math::Matrix< 3, 3, T > Vt;
	Math::Matrix< 4, 4, T > U;
	lapack::gesvd( 'N', 'S', matA, s, U, Vt );

	// copy bottom line of homography
	Math::Matrix< 3, 3, T > result;
	result( 2, 0 ) = Vt( 2, 0 );
	result( 2, 1 ) = Vt( 2, 1 );
	result( 2, 2 ) = Vt( 2, 2 );

	// compute entries 1,1 and 1,2, multiply by 2 to compensate scaling
	result( 0, 0 ) = ( (   c[ 0 ][ 0 ] + c[ 1 ][ 0 ] + c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 0 ) + 
		( - c[ 0 ][ 0 ] + c[ 1 ][ 0 ] - c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 1 ) + 
		( - c[ 0 ][ 0 ] - c[ 1 ][ 0 ] + c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 2 ) ) / 2;
	result( 0, 1 ) = ( ( - c[ 0 ][ 0 ] + c[ 1 ][ 0 ] - c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 0 ) + 
		(   c[ 0 ][ 0 ] + c[ 1 ][ 0 ] + c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 1 ) + 
		(   c[ 0 ][ 0 ] - c[ 1 ][ 0 ] - c[ 2 ][ 0 ] + c[ 3 ][ 0 ] ) * result( 2, 2 ) ) / 2;

	// compute entries 2,1 and 2,2, multiply by 2 to compensate scaling
	result( 1, 0 ) = ( (   c[ 0 ][ 1 ] + c[ 1 ][ 1 ] + c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 0 ) + 
		( - c[ 0 ][ 1 ] + c[ 1 ][ 1 ] - c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 1 ) + 
		( - c[ 0 ][ 1 ] - c[ 1 ][ 1 ] + c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 2 ) ) / 2;
	result( 1, 1 ) = ( ( - c[ 0 ][ 1 ] + c[ 1 ][ 1 ] - c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 0 ) + 
		(   c[ 0 ][ 1 ] + c[ 1 ][ 1 ] + c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 1 ) + 
		(   c[ 0 ][ 1 ] - c[ 1 ][ 1 ] - c[ 2 ][ 1 ] + c[ 3 ][ 1 ] ) * result( 2, 2 ) ) / 2;

	// compute entries 1,3 and 2,3
	result( 0, 2 ) = ( (   c[ 0 ][ 0 ] + c[ 1 ][ 0 ] - c[ 2 ][ 0 ] - c[ 3 ][ 0 ] ) * result( 2, 0 ) + 
		( - c[ 0 ][ 0 ] + c[ 1 ][ 0 ] + c[ 2 ][ 0 ] - c[ 3 ][ 0 ] ) * result( 2, 1 ) ) / -4;
	result( 1, 2 ) = ( (   c[ 0 ][ 1 ] + c[ 1 ][ 1 ] - c[ 2 ][ 1 ] - c[ 3 ][ 1 ] ) * result( 2, 0 ) + 
		( - c[ 0 ][ 1 ] + c[ 1 ][ 1 ] + c[ 2 ][ 1 ] - c[ 3 ][ 1 ] ) * result( 2, 1 ) ) / -4;

	// now multiply last row with factor 2 to compensate scaling
	result( 2, 0 ) *= 2;
	result( 2, 1 ) *= 2;

	// multiply with shift to compensate mean subtraction
	for ( int i = 0; i < 3; i++ )
	{
		result( 0, i ) = result( 0, i ) + result( 2, i ) * mean[ 0 ];
		result( 1, i ) = result( 1, i ) + result( 2, i ) * mean[ 1 ];
	}
	
	return result;
}

Math::Matrix< 3, 3, float > squareHomography( const std::vector< Math::Vector< 2, float > >& corners )
{
	return squareHomographyImpl( corners );
}

Math::Matrix< 3, 3, double > squareHomography( const std::vector< Math::Vector< 2, double > >& corners )
{
	return squareHomographyImpl( corners );
}

} } // namespace Ubitrack::Calibration

#endif // HAVE_LAPACK
