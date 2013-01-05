#ifndef SYMMETRY_MATRIX_ILDL_H
#define SYMMETRY_MATRIX_ILDL_H

using std::endl;
using std::cout;
using std::abs;

template <class el_type>
void symmetry_matrix<el_type>::ildl(ultriangular_matrix<el_type>& L, block_diag_matrix<el_type>& D, idx_vector_type& perm, const double& fill_factor, const double& tol)
{
	//----------------- initialize temporary variables --------------------//
	const int ncols = n_cols(); //number of cols in A.
	const double stat_piv = 1e-6; 
	
	int lfil;
	if (fill_factor > 1e4) lfil = ncols; //just incase users decide to enter a giant fill factor for fun...
	else lfil = (int) (fill_factor*nnz()/ncols); //roughly a factor of 2 since only lower tri. of A is stored
	
	const el_type alpha = (1.0+sqrt(17.0))/8.0;  //for use in pivoting.
	el_type w1(-1), wr(-1), d1(-1), dr(-1);		//for use in bk-pivoting
	el_type det_D, D_inv11, D_inv22, D_inv12;	//for use in 2x2 pivots
	el_type l_11, l_12;							//for use in 2x2 pivots

	vector<bool> in_set(ncols, false); //bitset used for unsorted merges
	symmetry_swap_struct<el_type> s;	//struct containing temp vars used in pivoting.
	
	elt_vector_type work(ncols, 0), temp(ncols, 0); ////work vector for the current column
	idx_vector_type curr_nnzs, temp_nnzs;  //non-zeros on current col.
	curr_nnzs.reserve(ncols); //reserves space for worse case (entire col is non-zero)
	temp_nnzs.reserve(ncols); //reserves space for worse case (entire col is non-zero)

	int count = 0; //the total number of nonzeros stored in L.
	int i, j, k, r, offset, col_size, col_size2(-1);
	bool size_two_piv = false;	//boolean indicating if the pivot is 2x2 or 1x1

	//--------------- allocate memory for L and D ------------------//
	L.resize(ncols, ncols); //allocate a vector of size n for Llist as well
	D.resize(ncols );
	
	//remember to remove debug before release
	int debug = -1;
	
	//------------------- main loop: factoring begins -------------------------//
	for (k = 0; k < ncols; k++) {

		if (k == debug) { 
			cout << "m_idx[k]: " << m_idx[k] << endl;
			cout << "m_x[k]: " << m_x[k] << endl;
		}
		
		//curr nnz vector starts out empty and is cleared at the end of each loop iteration.
		//assign nonzeros indices of A(k:n, k) to curr_nnzs
		curr_nnzs.assign (m_idx[k].begin(), m_idx[k].end());

		//assign nonzero values of A(k:n, k) to work
		for (j = 0; j < (int) curr_nnzs.size(); j++) {
			work[curr_nnzs[j]] = m_x[k][j];
		}

		//--------------begin pivoting--------------//

		//do delayed updates on current column. work = Sum_{i=0}^{k-1} L(k,i) * D(i,i) * L(k:n, i)
		//(the formula above generalizes to block matrix form in the case of 2x2 pivots).
		update(k, work, curr_nnzs, L, D, in_set);
		
		//store diagonal element in d1. set diagonal element in work vector to 0
		//since we want to find the maximum off-diagonal element.
		d1 = work[k];
		work[k] = 0;

		//find maximum element in work and store its index in r.
		w1 = max(work, curr_nnzs, r);

		if (k == debug) { 
			cout << "curr_nnzs: " << curr_nnzs << endl;
			for (i = 0; i < (int) curr_nnzs.size(); i++) {
				cout << work[curr_nnzs[i]] << " ";
			}
			cout << endl;
			cout << "w1: " << w1 << endl;
		}
		
		//bunch-kaufman partial pivoting is used below. for a more detailed reference,
		//refer to "Accuracy and Stability of Numerical Algorithms." by Higham (2002).
		//------------------- begin bunch-kaufman pivoting ------------------//
		if (w1 < eps) {
			//case 0: do nothing. pivot is k.
			if (k == debug) {
				cout << "case 0" << endl;
			}
		} else if ( (alpha * w1 - abs(d1)) < eps  ) {
			//case 1: do nothing. pivot is k.
			if (k == debug) {
				cout << "case 1" << endl;
			}
		} else {
			//temp vector starts out empty and is cleared at the end of each loop iteration.
			
			offset = list_first[r];
			//assign all nonzero indices and values in A(r, k:r) 
			//( not including A(r,r) ) to temp and temp_nnzs
			for (j = offset; j < (int) list[r].size(); j++) {
				temp_nnzs.push_back(list[r][j]);
				temp[list[r][j]] = coeff(r, list[r][j]);
			}

			//assign nonzero indices of A(r:n, r) to temp_nnzs 
			temp_nnzs.insert(temp_nnzs.end(), m_idx[r].begin(), m_idx[r].end());

			//assign nonzero values of to temp
			for (j = 0; j < (int) m_idx[r].size(); j++) {
				temp[m_idx[r][j]] = m_x[r][j];
			}

			//perform delayed updates on temp. temp = Sum_{i=0}^{k-1} L(r,i) * D(i,i) * L(k:n, i).
			//(the formula above generalizes to block matrix form in the case of 2x2 pivots).
			update(r, temp, temp_nnzs, L, D, in_set);

			dr = temp[r];
			temp[r] = 0;

			//find maximum element in temp.
			wr = max(temp, temp_nnzs, j);

			if ((alpha*w1*w1 - abs(d1)*wr) < eps) {
				//case 2: do nothing. pivot is k.
				if (k == debug) {
					cout << "case 2" << endl;
					cout << "d1: " << d1 << " wr: " << wr << " w1: " << w1 << endl;
				}
				
				
			} else if ( (alpha * wr - abs(dr)) < eps) {
				//case 3: pivot is k with r: 1x1 pivot case.
				temp[r] = dr;
				work[k] = d1;

				if (k > r) {
					cout << "case 3 " << k << " " << r << endl;
					cout << "fault! " << k << " " << r << endl;
					return;
				}

				//--------pivot A and L ---------//
				pivot(s, L, k, r);

				//----------pivot rest ----------//
				std::swap(d1, dr);

				//permute perm
				std::swap(perm[k], perm[r]);

				work.swap(temp);	//swap work with temp.
				std::swap(work[k], work[r]); //swap kth and rth row of work

				curr_nnzs.swap(temp_nnzs);	//swap curr_nnzs with temp_nnzs

				safe_swap(curr_nnzs, k, r); //swap k and r if they are present in curr_nnzs

				//--------end pivot rest---------//

			} else {
				//case 4: pivot is k+1 with r: 2x2 pivot case.
				if (k == debug) {
					cout << "case 4" << " " << k+1 << " " << r << endl;
					cout << "temp_nnzs: " << temp_nnzs << " " << r << endl;
					for (i = 0; i < (int) temp_nnzs.size(); i++) {
						cout << temp[temp_nnzs[i]] << " ";
					}
					cout << endl;
				}
				if (k >= r) {
					cout << "fault! " << k+1 << " " << r << endl;
					return;
				}

				//must advance list for 2x2 pivot since we are pivoting on col k+1
				advance_list(k);
				//for the same reason as above, we must advance L.first as well
				L.advance_column(k);

				//restore diagonal elements in work and temp
				temp[r] = dr;
				work[k] = d1;

				//indicate that pivot is 2x2
				size_two_piv = true;

				if (k+1 != r) {
					//symmetrically permute row/col k+1 and r.
					pivot(s, L, k+1, r);


					//----------pivot rest ----------//

					//permute perm
					std::swap(perm[k+1], perm[r]);

					//swap rows k+1 and r of work and temp
					std::swap(work[k+1], work[r]);
					std::swap(temp[k+1], temp[r]);

					//swap k+1 and r in curr_nnzs and temp_nnzs
					safe_swap(curr_nnzs, k+1, r);
					safe_swap(temp_nnzs, k+1, r);
				}
			}
		}
		//--------------end pivoting--------------//
		
		//erase diagonal element from non-zero indices (to exclude it from being dropped)
		curr_nnzs.erase(std::remove(curr_nnzs.begin(), curr_nnzs.end(), k), curr_nnzs.end());

		//performs the dual dropping procedure.
		if (!size_two_piv) {
			//perform dual dropping criteria on work
			drop_tol(work, curr_nnzs, lfil, tol);

		} else {
			//erase diagonal 2x2 block from non-zero indices (to exclude it from being dropped)
			temp_nnzs.erase(std::remove(temp_nnzs.begin(), temp_nnzs.end(), k), temp_nnzs.end());
			curr_nnzs.erase(std::remove(curr_nnzs.begin(), curr_nnzs.end(), k+1), curr_nnzs.end());
			temp_nnzs.erase(std::remove(temp_nnzs.begin(), temp_nnzs.end(), k+1), temp_nnzs.end());
			
			//compute inverse of the 2x2 block diagonal pivot.
			det_D = d1*dr - work[k+1]*work[k+1];
			if ( abs(det_D) < eps) det_D = stat_piv;  //statically pivot;
			D_inv11 = dr/det_D;
			D_inv22 = d1/det_D;
			D_inv12 = -work[k+1]/det_D;
			
			//assign pivot to D (d1 is assigned to D(k,k) later)
			D.off_diagonal(k) = work[k+1];
			D[k+1] = dr;
			
			//merge nonzeros of curr and temp together so iterating through them will be easier
			unordered_inplace_union(curr_nnzs, temp_nnzs.begin(), temp_nnzs.end(), in_set);

			
			//multiply inverse of pivot to work and temp (gives us two columns of l)
			for (auto it = curr_nnzs.begin(); it != curr_nnzs.end(); it++) {
				l_11 = work[*it]*D_inv11 + temp[*it]*D_inv12;
				l_12 = work[*it]*D_inv12 + temp[*it]*D_inv22;
				
				//note that work and temp roughly share the same non-zero indices
				work[*it] = l_11;
				temp[*it] = l_12;
			}
			
			//since the work and temp non-zero indices are roughly the same,
			//we can copy it over to temp_nnzs
			temp_nnzs.assign(curr_nnzs.begin(), curr_nnzs.end());
			
			//perform dual dropping procedure on work and temp
			drop_tol(temp, temp_nnzs, lfil, tol);
			drop_tol(work, curr_nnzs, lfil, tol);
		}

		//resize kth column of L to proper size.
		L.m_idx[k].resize(curr_nnzs.size());
		L.m_x[k].resize(curr_nnzs.size());
		
		//assign diagonal element to D
		D[k] = d1;
		
		
		if (!size_two_piv) {
			if ( abs(D[k]) < eps) D[k] = stat_piv; //statically pivot
			i = 0;
			for (auto it = curr_nnzs.begin(); it != curr_nnzs.end(); it++) { 
				if ( abs(work[*it]) > eps) {
					L.m_idx[k][i] = *it; //col k nonzero indices of L are stored
					L.m_x[k][i] = work[*it]/D[k]; //col k nonzero values of L are stored

					L.list[*it].push_back(k); //update Llist
					count++;
					i++;
				}
			}
			
			col_size = i;
			
			//advance list and L.first
			L.advance_column(k);
			advance_list(k);
		} else {
			//resize k+1th column of L to proper size.
			L.m_idx[k+1].resize(temp_nnzs.size());
			L.m_x[k+1].resize(temp_nnzs.size());

			i = 0;
			for (auto it = curr_nnzs.begin(); it != curr_nnzs.end(); it++) {
				if ( abs(work[*it]) > eps) {
					L.m_x[k][i] = work[*it]; //col k nonzero indices of L are stored
					L.m_idx[k][i] = *it; //col k nonzero values of L are stored
					
					L.list[*it].push_back(k); //update L.list
					count++;
					i++;
				}
				
			}
			
			j = 0;
			for (auto it = temp_nnzs.begin(); it != temp_nnzs.end(); it++) {
				if ( abs(temp[*it]) > eps) {
					L.m_x[k+1][j] = temp[*it]; //col k+1 nonzero indices of L are stored
					L.m_idx[k+1][j] = *it; //col k+1 nonzero values of L are stored
					
					L.list[*it].push_back(k+1); //update L.list
					count++;
					j++;
				}
				
			}

			col_size = i;
			col_size2 = j;

			//update list and L.first
			L.advance_column(k+1);
			advance_list(k+1);
			
		}
		
		// ------------- reset temp and work back to zero -----------------//
		work[k] = 0;
		temp[k] = 0;
		
		if (k + 1 < ncols) {
			temp[k+1] = 0;
			work[k+1] = 0;
		}
		
		for (auto it = curr_nnzs.begin(); it != curr_nnzs.end(); it++)
			work[*it] = 0;
		curr_nnzs.clear(); //zero out work vector
		
		for (auto it = temp_nnzs.begin(); it != temp_nnzs.end(); it++)
			temp[*it] = 0;
		temp_nnzs.clear(); //zero out work vector
		
		//-------------------------------------------------------------------//
		
		//resize columns of L to correct size
		L.m_x[k].resize(col_size);
		L.m_idx[k].resize(col_size);

		if (size_two_piv) {
			L.m_x[k+1].resize(col_size2);
			L.m_idx[k+1].resize(col_size2);
			k++;
			
			size_two_piv = false;
		}
		
		if (k == debug) {
			cout << "part of two piv? " << size_two_piv << endl;
			cout << "lfil: " << lfil << endl;
			cout << "D[k]: " << D[k] << endl;
			cout << "D(k,k+1): " << D.off_diagonal(k) << endl;
			cout << "D[k+1]: " << D[k+1] << endl;
			
			cout << "curr_nnzs: " << curr_nnzs << " " << r << endl;
			for (i = 0; i < col_size; i++) {
				cout << work[curr_nnzs[i]]/D[k] << " ";
			}
			cout << endl;
		}
		
		// std::cout << k << std::endl;
		// std::string s;
		// std::cin >> s;
		// if (s == "l") {
		// std::cout << L << std::endl;
		// } else if (s == "a") {
		// std::cout << to_string() << std::endl;
		// } else if (s == "w") {
			// cout << work << endl;
		// } else if (s == "t") {
			// cout << temp << endl;
		// }
		
	}

	//assign number of non-zeros in L to L.nnz_count
	L.nnz_count = count + ncols;

}

#endif