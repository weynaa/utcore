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
 * Implementations of functions dealing with projection matrices
 *
 * @author Daniel Pustka <daniel.pustka@in.tum.de>
 */

#include "Projection.h"
#include "Homography.h"

#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>

namespace ublas = boost::numeric::ublas;

#ifndef HAVE_LAPACK
namespace Ubitrack { namespace Calibration {
#else
#include <utMath/MatrixOperations.h>
#include <boost/numeric/bindings/lapack/gesvd.hpp>
#include <boost/numeric/bindings/lapack/gerqf.hpp>
#include <boost/numeric/bindings/lapack/orgrq.hpp>
#include <boost/numeric/bindings/blas/blas3.hpp>

// shortcuts to namespaces
namespace lapack = boost::numeric::bindings::lapack;
namespace blas = boost::numeric::bindings::blas;

namespace Ubitrack { namespace Calibration {

/** \internal */
template< typename T > Math::Matrix< 3, 4, T > projectionDLTImpl( const std::vector< Math::Vector< 3, T > >& fromPoints,
	const std::vector< Math::Vector< 2, T > >& toPoints )
{
	assert( fromPoints.size() == toPoints.size() );
	assert( fromPoints.size() >= 6 );

	// normalize input points
	Math::Vector< 3, T > fromShift;
	Math::Vector< 3, T > fromScale;
	Math::Matrix< 4, 4, T > fromCorrect;
	dltNormalizeVectors< 3, T >( fromShift, fromScale, fromCorrect, false, fromPoints );

	Math::Vector< 2, T > toShift;
	Math::Vector< 2, T > toScale;
	Math::Matrix< 3, 3, T > toCorrect;
	dltNormalizeVectors< 2, T >( toShift, toScale, toCorrect, true, toPoints );

	// construct equation system
	ublas::matrix< T, ublas::column_major > A( 2 * fromPoints.size(), 12 );
	for ( unsigned i = 0; i < fromPoints.size(); i++ )
	{
		Math::Vector< 2, T > to = ublas::element_div( toPoints[ i ] - toShift, toScale );
		Math::Vector< 3, T > from = ublas::element_div( fromPoints[ i ] - fromShift, fromScale );

		A( i * 2,  0 ) = A( i * 2, 1 ) = A( i * 2, 2 ) = A( i * 2, 3 ) = 0;
		A( i * 2,  4 ) = -from( 0 );
		A( i * 2,  5 ) = -from( 1 );
		A( i * 2,  6 ) = -from( 2 );
		A( i * 2,  7 ) = -1;
		A( i * 2,  8 ) = to( 1 ) * from( 0 );
		A( i * 2,  9 ) = to( 1 ) * from( 1 );
		A( i * 2, 10 ) = to( 1 ) * from( 2 );
		A( i * 2, 11 ) = to( 1 );
		A( i * 2 + 1,  0 ) = from( 0 );
		A( i * 2 + 1,  1 ) = from( 1 );
		A( i * 2 + 1,  2 ) = from( 2 );
		A( i * 2 + 1,  3 ) = 1;
		A( i * 2 + 1,  4 ) = A( i * 2 + 1, 5 ) = A( i * 2 + 1, 6 ) = A( i * 2 + 1, 7 ) = 0;
		A( i * 2 + 1,  8 ) = -to( 0 ) * from( 0 );
		A( i * 2 + 1,  9 ) = -to( 0 ) * from( 1 );
		A( i * 2 + 1, 10 ) = -to( 0 ) * from( 2 );
		A( i * 2 + 1, 11 ) = -to( 0 );
	}

	// solve using SVD
	ublas::vector< T > s( 12 );
	Math::Matrix< 12, 12, T > Vt;
	ublas::matrix< T, ublas::column_major > U( 2 * fromPoints.size(), 2 * fromPoints.size() );
	lapack::gesvd( 'N', 'A', A, s, U, Vt );

	// copy result to 3x4 matrix
	Math::Matrix< 3, 4, T > P;
	P( 0, 0 ) = Vt( 11, 0 ); P( 0, 1 ) = Vt( 11, 1 ); P( 0, 2 ) = Vt( 11,  2 ); P( 0, 3 ) = Vt( 11,  3 );
	P( 1, 0 ) = Vt( 11, 4 ); P( 1, 1 ) = Vt( 11, 5 ); P( 1, 2 ) = Vt( 11,  6 ); P( 1, 3 ) = Vt( 11,  7 );
	P( 2, 0 ) = Vt( 11, 8 ); P( 2, 1 ) = Vt( 11, 9 ); P( 2, 2 ) = Vt( 11, 10 ); P( 2, 3 ) = Vt( 11, 11 );

	// reverse normalization
	Math::Matrix< 3, 4, T > Ptemp( ublas::prod( toCorrect, P ) );
	ublas::noalias( P ) = ublas::prod( Ptemp, fromCorrect );

	// normalize result to have a viewing direction of length 1 (optional)
	T fViewDirLen = sqrt( P( 2, 0 ) * P( 2, 0 ) + P( 2, 1 ) * P( 2, 1 ) + P( 2, 2 ) * P( 2, 2 ) );
	
	// if first point is projected onto a negative z value, negate matrix
	const Math::Vector< 3, T > p1st = fromPoints[ 0 ];
	if ( P( 2, 0 ) * p1st( 0 ) + P( 2, 1 ) * p1st( 1 ) + P( 2, 2 ) * p1st( 2 ) + P( 2, 3 ) < 0 )
		fViewDirLen = -fViewDirLen;
	
	P *= T( 1 ) / fViewDirLen;

	return P;
}

Math::Matrix< 3, 4, float > projectionDLT( const std::vector< Math::Vector< 3, float > >& fromPoints,
	const std::vector< Math::Vector< 2, float > >& toPoints )
{
	return projectionDLTImpl( fromPoints, toPoints );
}

Math::Matrix< 3, 4, double > projectionDLT( const std::vector< Math::Vector< 3, double > >& fromPoints,
	const std::vector< Math::Vector< 2, double > >& toPoints )
{
	return projectionDLTImpl( fromPoints, toPoints );
}


/** \internal */
template< typename T > void decomposeProjectionImpl( Math::Matrix< 3, 3, T >& k,
	Math::Matrix< 3, 3, T >& r, Math::Vector< 3, T >& t, const Math::Matrix< 3, 4, T >& p_ )
{
	// copy matrix
	Math::Matrix< 3, 4, T > P( p_ );
	ublas::matrix_range< Math::Matrix< 3, 4, T > > A( P, ublas::range( 0, 3 ), ublas::range( 0, 3 ) );

	// origin must lie in front of camera
	if ( P( 2, 3 ) < 0 )
		P *= -1;

	// perform RQ decomposition
	Math::Vector< 3, T > tau;
	lapack::gerqf( A, tau );

	// generate matrix k
	k = A;
	k( 1, 0 ) = k( 2, 0 ) = k( 2, 1 ) = 0.0f;

	// generate matrix r
	lapack::orgrq( A, tau );
	r = A;

	// do some normalization
	// normalization is done by changing the product K R to (K S) (S^-1 R),
	// where S(i,i) == +/- 1 and S(i,j) == 0 for i!=j. Thus, S == S^-1
	Math::Vector< 3, T > scale( 1, 1, 1 );

	// det( R ) must be positive
	if ( Math::determinant( r ) < 0 )
		scale *= -1;

	// K_11 must be positive
	if ( k( 0, 0 ) * scale( 0 ) < 0 )
	{
		scale( 0 ) *= -1;
		scale( 1 ) *= -1;
	}

	// K_33 must be negative
	if ( k( 2, 2 ) * scale( 2 ) > 0 )
	{
		scale( 2 ) *= -1;
		scale( 1 ) *= -1;
	}

	for ( unsigned i = 0; i < 3; i++ )
	{
		ublas::column( k, i ) *= scale( i );
		ublas::row( r, i ) *= scale( i );
	}

	// normalize k
	T f = -1 / k( 2, 2 );
	k *= f;

	// compute translation vector: t = K^-1 p^4
	Math::Matrix< 3, 1, T > ttmp = ublas::subrange( P, 0, 3, 3, 4 );
	blas::trsm( 'L', 'U', 'N', 'N', 1.0f, k, ttmp );
	t = ublas::column( ttmp, 0 );
}


void decomposeProjection( Math::Matrix< 3, 3, float >& k,
	Math::Matrix< 3, 3, float >& r, Math::Vector< 3, float >& t, const Math::Matrix< 3, 4, float >& p )
{
	decomposeProjectionImpl( k, r, t, p );
}

void decomposeProjection( Math::Matrix< 3, 3, double >& k,
	Math::Matrix< 3, 3, double >& r, Math::Vector< 3, double >& t, const Math::Matrix< 3, 4, double >& p )
{
	decomposeProjectionImpl( k, r, t, p );
}

#endif // HAVE_LAPACK

/** \internal */
template < typename T >
Math::Matrix< 4, 4, T > projectionMatrixToOpenGLimp( T l, T r, T b, T t, T n, T f, Math::Matrix< 3, 4, T > m )
{
	Math::Matrix< 4, 4, T > m2;
	ublas::subrange( m2, 0, 3, 0, 4 ) = m;

	T norm  = sqrt ( m2( 2, 0 )*m2( 2, 0 ) + m2( 2, 1 )*m2( 2, 1 ) + m2( 2, 2 )*m2( 2, 2 ) );

	// copy 3rd row to 4th row
	ublas::row( m2, 3 ) = ublas::row( m2, 2 );
	ublas::row( m2, 2 ) *= ( -f - n );

	//factor for normalization
	T add =  f * n * norm;

	m2( 2, 3 ) += add;

	//compute ortho matrix
	Math::Matrix< 4, 4, T > ortho;

	ortho( 0, 0 ) = static_cast< T >( 2.0 ) / ( r - l );
	ortho( 0, 1 ) = static_cast< T >( 0.0 );
	ortho( 0, 2 ) = static_cast< T >( 0.0 );
	ortho( 0, 3 ) = ( r + l ) / ( l - r );
	ortho( 1, 0 ) = static_cast< T >( 0.0 );
	ortho( 1, 1 ) = static_cast< T >( 2.0 ) / ( t - b );
	ortho( 1, 2 ) = static_cast< T >( 0.0 );
	ortho( 1, 3 ) = ( t + b ) / ( b - t );
	ortho( 2, 0 ) = static_cast< T >( 0.0 );
	ortho( 2, 1 ) = static_cast< T >( 0.0 );
	ortho( 2, 2 ) = static_cast< T >( 2.0 ) / ( n - f );
	ortho( 2, 3 ) = ( f + n ) / ( n - f );
	ortho( 3, 0 ) = static_cast< T >( 0.0 );
	ortho( 3, 1 ) = static_cast< T >( 0.0 );
	ortho( 3, 2 ) = static_cast< T >( 0.0 );
	ortho( 3, 3 ) = static_cast< T >( 1.0 );

	return ublas::prod( ortho, m2 );
}

Math::Matrix< 4, 4, double > projectionMatrixToOpenGL( double l, double r, double b, double t, double n, double f, Math::Matrix< 3, 4, double > m )
{
	return projectionMatrixToOpenGLimp( l, r, b, t, n, f, m );
}

Math::Matrix< 4, 4, float > projectionMatrixToOpenGL( float l, float r, float b, float t, float n, float f, Math::Matrix< 3, 4, float > m )
{
	return projectionMatrixToOpenGLimp( l, r, b, t, n, f, m );
}

Math::Matrix< 4, 4, double > projectionMatrixToOpenGL( double l, double r, double b, double t, double n, double f, Math::Matrix< 3, 3, double > m )
{
	//construct the 4th column
	Math::Matrix< 3, 4, double > mat;
	mat( 0, 3 ) = 0.0;
	mat( 1, 3 ) = 0.0;
	mat( 2, 3 ) = 0.0;
	ublas::subrange(mat, 0, 3, 0, 3 ) = m;

	return projectionMatrixToOpenGLimp( l, r, b, t, n, f, mat );
}

Math::Matrix< 4, 4, float > projectionMatrixToOpenGL( float l, float r, float b, float t, float n, float f, Math::Matrix< 3, 3, float > m )
{
	//construct the 4th column
	Math::Matrix< 3, 4, float > mat;
	mat( 0, 3 ) = float( 0.0 );
	mat( 1, 3 ) = float( 0.0 );
	mat( 2, 3 ) = float( 0.0 );
	ublas::subrange(mat, 0, 3, 0, 3 ) = m;

	return projectionMatrixToOpenGLimp( l, r, b, t, n, f, mat );
}

Math::Matrix< 4, 4, double > offAxisProjectionMatrix( Math::Vector< 3, double >& eye, Math::Vector< 3, double >& ll, Math::Vector< 3, double >& ul, Math::Vector< 3, double >& lr, double n, double f, double sw, double sh ) {

	Math::Vector<3> Xs = (lr - ll) / sw;
	Math::Vector<3> Ys = (ul - ll) / sh;
	Math::Vector<3> Zs = cross_prod(Xs, Ys);

	Math::Vector<3> Es = eye - ll;

	double distance = inner_prod(Es, Zs);

	double L = inner_prod(Es, Xs);
	double R = sw - L;
	double Bu = inner_prod(Es, Ys);
	double T = sh - Bu;

	double left = -L * n/distance;
	double right  = R * n/distance;
	double buttom = -Bu * n/distance;
	double top = T * n/distance;

	// Build up the projection matrix
	double A = (right+left)/(right-left);
	double B = (top+buttom)/(top-buttom);
	double C = -((f+n)/(f-n));
	double D = -((2*f*n)/(f-n));

	Math::Matrix<4, 4> proj = boost::numeric::ublas::zero_matrix<double>(4,4);
	proj(0,0) = (2*n)/(right-left);
	proj(1,1) = (2*n)/(top-buttom);
	proj(0,2) = A;
	proj(1,2) = B;
	proj(2,2) = C;
	proj(3,2) = -1;
	proj(2,3) = D;

	Math::Matrix<4, 4> translation = boost::numeric::ublas::identity_matrix<double>(4);
	translation(0, 3) = -eye(0);
	translation(1, 3) = -eye(1);
	translation(2, 3) = -eye(2);

	// Build a rotation matrix.
	Math::Matrix<4, 4> m = boost::numeric::ublas::zero_matrix<double>(4,4);
	m(0, 0) = Xs(0); m(0, 1) = Ys(0); m(0, 2) = Zs(0);
	m(1, 0) = Xs(1); m(1, 1) = Ys(1); m(1, 2) = Zs(1);
	m(2, 0) = Xs(2); m(2, 1) = Ys(2); m(2, 2) = Zs(2);
								m(3, 3) = 1.0;
	
	// The rotation matrix is orthogonal so we can transpose it instead of inverting.
	Math::Matrix<4, 4> view = boost::numeric::ublas::prod(boost::numeric::ublas::trans(m), translation );
	Math::Matrix<4, 4> view2 = boost::numeric::ublas::prod(proj, view);
	return view2;
}


template< class T >
void correctOriginImpl( Math::Matrix< 3, 3, T >& k, int origin, int height )
{
	if ( !origin )
	{
		k( 1, 1 ) *= T( -1 );
		k( 1, 2 ) = -k( 1, 2 ) + k( 2, 2 ) * T( height - 1 );
	}
}

void correctOrigin( Math::Matrix< 3, 3, float >& k, int origin, int height )
{ correctOriginImpl( k, origin, height ); }

void correctOrigin( Math::Matrix< 3, 3, double >& k, int origin, int height )
{ correctOriginImpl( k, origin, height ); }

} } // namespace Ubitrack::Calibration
