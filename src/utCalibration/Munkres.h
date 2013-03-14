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
 * Munkres class
 * This file contains the Munkres class for using the
 * Munkres' Assignment Algorithm (sometimes referred to as the Hungarian Algorithm)
 *
 * You use a matrix which should be solved
 *
 * the result can be a masked matrix or a ordered new list of vectors
 *
 * @author Daniel Muhra <muhra@in.tum.de>
 */
#ifndef __MUNKRES_INCLUDED__
#define __MUNKRES_INCLUDED__


#include <utCore.h>
#include <utMath/Vector.h>
#include <list>
#include <vector>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>

namespace ublas = boost::numeric::ublas;

#define Z_NORMAL 0
#define Z_STAR 1
#define Z_PRIME 2

namespace Ubitrack { namespace Calibration {

/**
 * @ingroup calibration
 * The actual Munkres class
 * to solve various problems using the Hungarian Algorithm
 * @param T type of vector/matrix elements 
 * @param N dimension of the vectors (2 or 3)
 */
template< typename T >
class Munkres {

public:

	/** Default constructor */
	Munkres();

	/** Constructor directly using a matrix which should be solved */
	Munkres(ublas::matrix< T > & matrix);

	/** this function must be called AFTER the input data was set*/
	void solve();

	/**
	 * sets the input data
	 * @param matrix the matrix to be solved
	 */
	void setMatrix( ublas::matrix< T > & matrix );

	/**
	 * returns the result as a masked Matrix
	 * @return every 1 in the matrix represents a match
	 */
	ublas::matrix< T >  getMaskMatrix();

	/**
	 * returns the result as a ordered list of matches
	 * the order is corresponding to the old points ( rows )
	 *
	 * @return match list of colums to rows
	 */
	std::vector< unsigned int > getRowMatchList();

	/**
	 * returns the result as a ordered list of matches
	 * the order is corresponding to the current points ( columns )
	 *
	 * @return match list of colums to rows
	 */
	std::vector< unsigned int > getColMatchList();

private:
	inline bool find_uncovered_in_matrix( T item , unsigned int & row, unsigned int & col );
	inline bool pair_in_list( const std::pair<int,int> & needle, const std::list< std::pair< int, int > > & haystack );
	int step1();
	int step2();
	int step3();
	int step4();
	int step5();
	int step6();
	ublas::matrix< int > mask_matrix;
	ublas::matrix< T > m_matrix;
	bool *row_mask;
	bool *col_mask;
	unsigned int saverow, savecol;
	unsigned int m_max;
};

/** internal */
template< typename T >
Munkres< T >::Munkres()
{
	saverow = 0; 
	savecol = 0;
}

template< typename T >
Munkres< T >::Munkres(ublas::matrix< T > & matrix)
{	
	saverow = 0; 
	savecol = 0;
	setMatrix( matrix );
}

template< typename T >
bool Munkres< T >::find_uncovered_in_matrix(T item, unsigned int & row, unsigned int & col) {
	for ( row = 0 ; row < m_max ; row++ )
		if ( !row_mask[row] )
			for ( col = 0 ; col < m_max ; col++ )
				if ( !col_mask[col] )
					if ( m_matrix( row, col ) == item )
						return true;

	return false;
}

template< typename T >
bool Munkres< T >::pair_in_list(const std::pair<int,int> &needle, const std::list<std::pair<int,int> > &haystack) {
	for ( std::list<std::pair<int,int> >::const_iterator i = haystack.begin() ; i != haystack.end() ; i++ ) {
		if ( needle == *i )
			return true;
	}
	
	return false;
}

template< typename T >
int Munkres< T >::step1() 
{
	for ( unsigned  int row = 0 ; row < m_max ; row++ )
		for ( unsigned  int col = 0 ; col < m_max ; col++ )
			if ( m_matrix( row, col ) == 0 ) 
			{
				bool isstarred = false;
				for ( unsigned int nrow = 0 ; nrow < m_max ; nrow++ )
					if ( mask_matrix( nrow, col ) == Z_STAR )
						isstarred = true;

				if ( !isstarred ) 
				{
					for ( unsigned  int ncol = 0 ; ncol < m_max ; ncol++ )
						if ( mask_matrix( row, ncol ) == Z_STAR )
							isstarred = true;
				}
							
				if ( !isstarred ) 
				{
					mask_matrix( row, col ) = Z_STAR;
				}
			}

	return 2;
}

template< typename T >
int Munkres< T >::step2() 
{
	unsigned int covercount = 0;
	for ( unsigned int row = 0 ; row < m_max ; row++ )
		for ( unsigned int col = 0 ; col < m_max ; col++ )
			if ( mask_matrix( row, col ) == Z_STAR ) 
			{
				col_mask[col] = true;
				covercount++;
			}
			
	if ( covercount >= m_max )
		return 0;
	else return 3;
}

template< typename T >
int Munkres< T >::step3() 
{
	/*
	Main Zero Search

   1. Find an uncovered Z in the distance matrix and prime it. If no such zero exists, go to Step 5
   2. If No Z* exists in the row of the Z', go to Step 4.
   3. If a Z* exists, cover this row and uncover the column of the Z*. Return to Step 3.1 to find a new Z
	*/
	if ( find_uncovered_in_matrix( 0, saverow, savecol ) ) 
		mask_matrix( saverow, savecol ) = Z_PRIME; // prime it.
	else return 5;

	for ( unsigned int ncol = 0 ; ncol < m_max ; ncol++ )
		if ( mask_matrix( saverow, ncol ) == Z_STAR )
		{
			row_mask[saverow] = true; //cover this row and
			col_mask[ncol] = false; // uncover the column containing the starred zero
			return 3; // repeat
		}

	return 4; // no starred zero in the row containing this primed zero
}

template< typename T >
int Munkres< T >::step4() 
{
	std::list<std::pair<int,int> > seq;
	// use saverow, savecol from step 3.
	std::pair<int,int> z0(saverow, savecol);
	std::pair<int,int> z1(-1,-1);
	std::pair<int,int> z2n(-1,-1);
	seq.insert(seq.end(), z0);
	unsigned int row, col = savecol;
	/*
	Increment Set of Starred Zeros

   1. Construct the ``alternating sequence'' of primed and starred zeros:

         Z0 : Unpaired Z' from Step 4.2 
         Z1 : The Z* in the column of Z0
         Z[2N] : The Z' in the row of Z[2N-1], if such a zero exists 
         Z[2N+1] : The Z* in the column of Z[2N]

      The sequence eventually terminates with an unpaired Z' = Z[2N] for some N.
	*/
	bool madepair;
	do {
		madepair = false;
		for ( row = 0 ; row < m_max ; row++ )
			if ( mask_matrix(row,col) == Z_STAR ) {
				z1.first = row;
				z1.second = col;
				if ( pair_in_list(z1, seq) )
					continue;
				
				madepair = true;
				seq.insert(seq.end(), z1);
				break;
			}

		if ( !madepair )
			break;

		madepair = false;

		for ( col = 0 ; col < m_max ; col++ )
			if ( mask_matrix(row,col) == Z_PRIME ) {
				z2n.first = row;
				z2n.second = col;
				if ( pair_in_list(z2n, seq) )
					continue;
				madepair = true;
				seq.insert(seq.end(), z2n);
				break;
			}
	} while ( madepair );

	for ( std::list<std::pair<int,int> >::iterator i = seq.begin() ;
		  i != seq.end() ;
		  i++ ) {
		// 2. Unstar each starred zero of the sequence.
		if ( mask_matrix(i->first,i->second) == Z_STAR )
			mask_matrix(i->first,i->second) = Z_NORMAL;

		// 3. Star each primed zero of the sequence,
		// thus increasing the number of starred zeros by one.
		if ( mask_matrix(i->first,i->second) == Z_PRIME )
			mask_matrix(i->first,i->second) = Z_STAR;
	}

	// 4. Erase all primes, uncover all columns and rows, 
	for ( unsigned int row = 0 ; row < m_max ; row++ )
		for ( unsigned int col = 0 ; col < m_max ; col++ )
			if ( mask_matrix(row,col) == Z_PRIME )
				mask_matrix(row,col) = Z_NORMAL;
	
	for ( unsigned int i = 0 ; i < m_max ; i++ ) {
		row_mask[i] = false;
	}

	for ( unsigned int i = 0 ; i < m_max ; i++ ) {
		col_mask[i] = false;
	}

	// and return to Step 2. 
	return 2;
}

template< typename T >
int Munkres< T >::step5() 
{
	/*
	New Zero Manufactures

   1. Let h be the smallest uncovered entry in the (modified) distance matrix.
   2. Add h to all covered rows.
   3. Subtract h from all uncovered columns
   4. Return to Step 3, without altering stars, primes, or covers. 
	*/
	T h = 0;
	for ( unsigned int row = 0 ; row < m_max ; row++ ) 
	{
		if ( !row_mask[row] ) 
		{
			for ( unsigned int col = 0 ; col < m_max ; col++ ) 
			{
				if ( !col_mask[col] ) 
				{
					if ( (h > m_matrix( row, col ) && m_matrix( row, col ) != 0) || h == 0 ) 
						h = m_matrix( row, col );
				}
			}
		}
	}

	for ( unsigned int row = 0 ; row < m_max ; row++ )
		for ( unsigned int col = 0 ; col < m_max ; col++ ) 
		{
			if ( row_mask[row] )
				m_matrix( row, col ) += h;

			if ( !col_mask[col] )
				m_matrix( row, col ) -= h;
		}

	return 3;
}

template< typename T >
void Munkres< T >::setMatrix( ublas::matrix< T > & matrix )
{
	//find maximum matrix value vMax
	T vMax = static_cast< T >( 0 );
	for( unsigned int row=0; row < matrix.size1(); row++ )
	{
		for( unsigned int col=0; col < matrix.size2(); col++ )
		{
			if( matrix( row, col ) > vMax )
				vMax = matrix( row, col );
		}
	}

	//create a M x M matrix
	if ( matrix.size1() == matrix.size2() )
	{
		m_matrix = matrix;
	}
	else if ( matrix.size1() > matrix.size2() )
	{
		m_matrix.resize( matrix.size1(), matrix.size1() );
		ublas::subrange( m_matrix, 0, matrix.size1(), 0, matrix.size2() ) = matrix;
		ublas::subrange( m_matrix, 0, matrix.size1(), matrix.size2(), matrix.size1() - matrix.size2() ) = 
			ublas::scalar_matrix< T >( matrix.size1(), matrix.size1() - matrix.size2(), vMax );
	}
	else
	{
		m_matrix.resize( matrix.size2(), matrix.size2(), false );
		ublas::subrange( m_matrix, 0, matrix.size1(), 0, matrix.size2() ) = matrix;
		ublas::subrange( m_matrix, matrix.size1(), matrix.size2() - matrix.size1(), 0, matrix.size2() ) = 
			ublas::scalar_matrix< T >( matrix.size2() - matrix.size1(), matrix.size2(), vMax );
	}

	m_max = m_matrix.size1();

	//create a zero in every row
	T min = vMax;	
	for( unsigned int row=0; row<m_max; row++ )
	{
		for( unsigned int col=0; col<m_max; col++ )
		{
			if( m_matrix( row, col ) < min )
				min = m_matrix( row, col );
		}
		for( unsigned int col=0; col<m_max; col++ )
		{
			m_matrix( row, col ) = m_matrix( row, col ) - min;
		}
		min = vMax;
	}	

	//initialize mask matrix
	mask_matrix = ublas::scalar_matrix< int >( m_matrix.size1(), m_matrix.size1(), Z_NORMAL );
}

template< typename T >
void Munkres< T >::solve()
{
	bool notdone = true;
	int step = 1;

	// Z_STAR == 1 == starred, Z_PRIME == 2 == primed
	row_mask = new bool[m_max];
	col_mask = new bool[m_max];
	for ( unsigned int i = 0 ; i < m_max ; i++ ) 
	{
		row_mask[i] = false;
		col_mask[i] = false;
	}

	while ( notdone ) 
	{
		switch ( step ) 
		{
			case 0:
				notdone = false;
				break;
			case 1:
				step = step1();
				break;
			case 2:
				step = step2();
				break;
			case 3:
				step = step3();
				break;
			case 4:
				step = step4();
				break;
			case 5:
				step = step5();
				break;
		}
	}
}

template< typename T >
ublas::matrix< T >  Munkres< T >::getMaskMatrix()
{
	return mask_matrix;
}

template< typename T >
std::vector< unsigned int > Munkres< T >::getRowMatchList()
{
	std::vector< unsigned int > list;
	bool found = false;
	unsigned int col = 0;

	for( unsigned int row=0; row<m_max; row++ )
	{
		while( !found && col < m_max)
		{
			if( mask_matrix( row, col ) == Z_STAR )
			{
				list.push_back( col );
				found = true;
			}
			col++;
		}
		found = false;
		col = 0;
	}

	return list;
}

template< typename T >
std::vector< unsigned int > Munkres< T >::getColMatchList()
{
	std::vector< unsigned int > list;
	bool found = false;
	unsigned int row = 0;

	for( unsigned int col=0; col<m_max; col++ )
	{
		while( !found && row < m_max)
		{
			if( mask_matrix( row, col ) == Z_STAR )
			{
				list.push_back( row );
				found = true;
			}
			row++;
		}
		found = false;
		row = 0;
	}

	return list;
}

} } // namespace Ubitrack::Calibration

#endif